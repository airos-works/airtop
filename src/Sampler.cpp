#include "Sampler.h"
#include "Metrics.h"
#include "HardwareSensors.h"
#include <Directory.h>
#include <Entry.h>
#include <NetworkInterface.h>
#include <NetworkRoster.h>
#include <Path.h>
#include <Screen.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <fs_info.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <cstdio>
#include <functional>

static std::string Json(const std::string& text) {
    std::string result = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { result += '\\'; result += c; }
        else if (c < 32) { char s[8]; snprintf(s, sizeof(s), "\\u%04x", c); result += s; }
        else result += c;
    }
    return result + '"';
}
Sampler::Sampler(bool processDetails) : fProcessDetails(processDetails) {
    char host[256] = {}; gethostname(host, sizeof(host) - 1); fState.host = host;
    utsname os{}; uname(&os); fState.os = std::string(os.sysname) + " " + os.release;
    fState.cpuName = "Processor";
#if defined(__x86_64__) || defined(__i386__)
    cpuid_info cpuid{};
    if (get_cpuid(&cpuid, 0x80000000, 0) == B_OK && cpuid.regs.eax >= 0x80000004) {
        char name[49] = {};
        for (int i = 0; i < 3; ++i) { get_cpuid(&cpuid, 0x80000002 + i, 0); memcpy(name + i * 16, &cpuid.regs.eax, 4); memcpy(name + i * 16 + 4, &cpuid.regs.ebx, 4); memcpy(name + i * 16 + 8, &cpuid.regs.ecx, 4); memcpy(name + i * 16 + 12, &cpuid.regs.edx, 4); }
        fState.cpuName = name; fState.cpuName.erase(0, fState.cpuName.find_first_not_of(' '));
    }
#endif
}
Sampler::~Sampler() { Stop(); }
void Sampler::Start() {
    if (fThread.joinable()) return;
    fStop = false;
    fThread = std::thread([this] {
        while (!fStop) {
            Snapshot value = Sample();
            { std::lock_guard<std::mutex> lock(fMutex); fPublished = std::move(value); }
            std::unique_lock<std::mutex> lock(fWaitMutex);
            fWake.wait_for(lock, std::chrono::microseconds(std::max<bigtime_t>(10000, fInterval.load() - bigtime_t(fPublished.sampleMs * 1000))));
        }
    });
}
void Sampler::Stop() { fStop = true; fWake.notify_all(); if (fThread.joinable()) fThread.join(); }
void Sampler::SetProcessDetailsEnabled(bool enabled) {
    if (fProcessDetails.exchange(enabled) != enabled) fWake.notify_all();
}
void Sampler::SetInterval(bigtime_t interval) { fInterval = interval; fWake.notify_all(); }
bool Sampler::Latest(Snapshot& snapshot) {
    std::lock_guard<std::mutex> lock(fMutex);
    if (snapshot.sequence == fPublished.sequence && snapshot.valid == fPublished.valid) return false;
    snapshot = fPublished;
    return true;
}
Snapshot Sampler::Sample() {
    bigtime_t now = system_time();
    double seconds = (now - fPreviousTime) / 1000000.0;
    system_info info{};
    if (get_system_info(&info) != B_OK) { fState.valid = false; fState.error = "System counters unavailable"; return fState; }
    fState.valid = true; fState.error.clear(); fState.time = now; ++fState.sequence;
    std::vector<cpu_info> cpu(info.cpu_count);
    fState.cpuValid = get_cpu_info(0, info.cpu_count, cpu.data()) == B_OK;
    fState.cores.assign(info.cpu_count, 0); fState.cpu = 0; fState.frequency = 0;
    int enabled = 0;
    for (size_t i = 0; fState.cpuValid && i < cpu.size(); ++i) {
        if (!cpu[i].enabled) continue;
        ++enabled;
        if (fPreviousTime && i < fPreviousCPU.size())
            fState.cores[i] = Metrics::Percent(cpu[i].active_time - fPreviousCPU[i].active_time, now - fPreviousTime);
        fState.cpu += fState.cores[i]; fState.frequency += cpu[i].current_frequency;
    }
    if (enabled) { fState.cpu /= enabled; fState.frequency /= enabled; }
    if (fState.cpuValid) fPreviousCPU = cpu; else fPreviousCPU.clear();
    fState.totalMemory = info.max_pages * B_PAGE_SIZE;
    fState.availableMemory = std::min(info.free_memory, fState.totalMemory);
    fState.cache = info.cached_pages * B_PAGE_SIZE;
    fState.blockCache = info.block_cache_pages * B_PAGE_SIZE;
    fState.swapTotal = info.max_swap_pages * B_PAGE_SIZE;
    fState.swapUsed = (info.max_swap_pages - std::min(info.free_swap_pages, info.max_swap_pages)) * B_PAGE_SIZE;
    fState.threads = info.used_threads; fState.teams = info.used_teams;
    fState.faults = fPreviousTime ? Metrics::Rate(info.page_faults, fPreviousFaults, seconds) : 0;
    fPreviousFaults = info.page_faults;
    // Area enumeration and process sorting need not run at the graph cadence.
    if (!fProcessDetails.load()) {
        // The panel uses system totals; only a dashboard needs per-process areas.
        fState.processes.clear();
        fPreviousTeams.clear();
        fState.processTime = fProcessTime = 0;
    } else if (!fProcessTime || now - fProcessTime >= 1000000) {
        double processSeconds = (now - fProcessTime) / 1000000.0;
        std::map<team_id, std::pair<bigtime_t, bigtime_t>> previousTeams;
        fState.processes.clear();
        int32 cookie = 0; team_info team{};
        while (get_next_team_info(&cookie, &team) == B_OK) {
            Process process; process.id = team.team; process.start = team.start_time;
            process.name = team.name; process.command = team.args;
            process.threads = team.thread_count; process.uid = team.uid;
            team_usage_info usage{};
            if (get_team_usage_info(team.team, B_TEAM_USAGE_SELF, &usage) == B_OK) {
                bigtime_t total = usage.user_time + usage.kernel_time;
                // Haiku charges idle-thread time to the kernel team. Sum only
                // non-idle kernel threads so idle time never looks like CPU load.
                if (team.team == B_SYSTEM_TEAM) {
                    total = 0; int32 threadCookie = 0; thread_info kernelThread{};
                    while (get_next_thread_info(team.team, &threadCookie, &kernelThread) == B_OK)
                        if (kernelThread.priority != B_IDLE_PRIORITY)
                            total += kernelThread.user_time + kernelThread.kernel_time;
                }
                auto previous = fPreviousTeams.find(team.team);
                if (fProcessTime && previous != fPreviousTeams.end() && previous->second.first == team.start_time)
                    process.cpu = Metrics::Rate(total, previous->second.second, processSeconds * 1000000) * 100;
                previousTeams[team.team] = {team.start_time, total};
            }
            ssize_t areaCookie = 0; area_info area{};
            while (get_next_area_info(team.team, &areaCookie, &area) == B_OK) {
                process.resident += area.ram_size; process.virtualSize += area.size;
            }
            thread_info thread{};
            if (get_thread_info(team.team, &thread) == B_OK) process.priority = thread.priority;
            if (process.name.empty()) process.name = process.command;
            fState.processes.push_back(process);
        }
        fPreviousTeams.swap(previousTeams);
        fState.processTime = fProcessTime = now;
    }
    std::map<std::string, std::pair<uint64, uint64>> previousNetwork;
    fState.networks.clear(); fState.down = fState.up = 0;
    uint32 netCookie = 0; BNetworkInterface net;
    while (BNetworkRoster::Default().GetNextInterface(&netCookie, net) == B_OK) {
        if (net.Flags() & IFF_LOOPBACK) continue;
        ifreq_stats stats{};
        if (net.GetStats(stats) != B_OK) continue;
        Network network; network.name = net.Name(); network.link = net.HasLink();
        network.received = stats.receive.bytes; network.sent = stats.send.bytes;
        network.errors = uint64(stats.receive.errors) + stats.send.errors;
        BNetworkInterfaceAddress address;
        int index = net.FindFirstAddress(AF_INET);
        if (index >= 0 && net.GetAddressAt(index, address) == B_OK) network.address = address.Address().ToString().String();
        auto prev = fPreviousNetwork.find(network.name);
        if (fPreviousTime && prev != fPreviousNetwork.end()) {
            network.down = Metrics::Rate(network.received, prev->second.first, seconds);
            network.up = Metrics::Rate(network.sent, prev->second.second, seconds);
        }
        previousNetwork[network.name] = {network.received, network.sent};
        fState.down += network.down; fState.up += network.up;
        fState.networks.push_back(network);
    }
    std::stable_sort(fState.networks.begin(), fState.networks.end(), [](const Network& a, const Network& b) { return a.link > b.link; });
    fPreviousNetwork.swap(previousNetwork);
    if (!fDiskTime || now - fDiskTime >= 5000000) {
        fState.disks.clear(); BVolumeRoster roster; BVolume volume;
        while (roster.GetNextVolume(&volume) == B_OK) {
            fs_info fs{};
            if (fs_stat_dev(volume.Device(), &fs) != B_OK || fs.total_blocks <= 0) continue;
            Disk disk; disk.name = fs.volume_name; disk.fs = fs.fsh_name; disk.device = fs.device_name;
            disk.capacity = uint64(fs.total_blocks) * fs.block_size;
            disk.free = std::min(disk.capacity, uint64(std::max<off_t>(0, fs.free_blocks)) * fs.block_size);
            disk.readOnly = volume.IsReadOnly();
            BDirectory root; BEntry entry; BPath path;
            if (volume.GetRootDirectory(&root) == B_OK && root.GetEntry(&entry) == B_OK && entry.GetPath(&path) == B_OK) disk.path = path.Path();
            if (disk.name.empty()) disk.name = disk.path;
            fState.disks.push_back(disk);
        }
        std::stable_sort(fState.disks.begin(), fState.disks.end(), [](const Disk& a, const Disk& b) { return (a.path == "/boot") > (b.path == "/boot"); });
        fState.diskTime = fDiskTime = now;
    }
    if (!fSensorTime || now - fSensorTime >= 30000000) {
        fSensors.clear();
        std::function<void(const std::string&, int)> scan = [&](const std::string& path, int depth) {
            if (depth > 4) return;
            DIR* dir = opendir(path.c_str()); if (!dir) return;
            while (dirent* entry = readdir(dir)) {
                if (entry->d_name[0] == '.') continue;
                std::string full = path + "/" + entry->d_name;
                BEntry item(full.c_str());
                if (item.IsDirectory()) scan(full, depth + 1);
                else if (full.find("thermal") != std::string::npos) fSensors.push_back(full);
            }
            closedir(dir);
        };
        scan("/dev/power", 0); scan("/dev/sensors", 0); scan("/dev/sensor", 0);
        fSensorTime = now;
    }
    if (!fTemperatureTime || now - fTemperatureTime >= 1000000) {
        fState.hasTemperature = false; fState.sensor.clear();
        for (const auto& sensor : fSensors) {
            int fd = open(sensor.c_str(), O_RDONLY | O_NONBLOCK); if (fd < 0) continue;
            char buffer[512] = {}; ssize_t count = pread(fd, buffer, sizeof(buffer) - 1, 0); close(fd);
            double temperature;
            if (count > 0 && Metrics::Temperature(std::string(buffer, count), temperature)
                && (!fState.hasTemperature || temperature > fState.temperature)) {
                fState.temperature = temperature; fState.hasTemperature = true; fState.sensor = sensor;
            }
        }
        // Only use SMN when no thermal device is published, so a native thermal
        // driver remains the owner of the hardware on systems that provide one.
        if (!fState.hasTemperature && fSensors.empty()
            && ReadThreadripperTemperature(fState.cpuName, fState.temperature)) {
            fState.hasTemperature = true; fState.sensor = "AMD 1950X Tdie / SMN";
        }
        fTemperatureTime = now;
    }
    if (fPreviousTime) {
        if (fState.cpuValid) Animation::Append(fState.cpuHistory, now, fState.cpu);
        Animation::Append(fState.downHistory, now, fState.down);
        Animation::Append(fState.upHistory, now, fState.up);
    }
    fPreviousTime = now; fState.sampleMs = (system_time() - now) / 1000.0;
    return fState;
}
void PrintSnapshot(const Snapshot& s) {
    printf("{\"sequence\":%llu,\"valid\":%s,\"host\":%s,\"cpu_name\":%s,\"cpu_percent\":%.3f,\"cpu_valid\":%s,\"cores\":[",
        (unsigned long long)s.sequence, s.valid ? "true" : "false", Json(s.host).c_str(), Json(s.cpuName).c_str(), s.cpu, s.cpuValid ? "true" : "false");
    for (size_t i = 0; i < s.cores.size(); ++i) printf("%s%.3f", i ? "," : "", s.cores[i]);
    printf("],\"memory_total\":%llu,\"memory_available\":%llu,\"swap_used\":%llu,\"temperature_c\":%s,\"sensor\":%s,\"sample_ms\":%.3f,\"networks\":[",
        (unsigned long long)s.totalMemory, (unsigned long long)s.availableMemory, (unsigned long long)s.swapUsed,
        s.hasTemperature ? Metrics::Number(s.temperature).c_str() : "null", Json(s.sensor).c_str(), s.sampleMs);
    for (size_t i = 0; i < s.networks.size(); ++i) { const auto& n = s.networks[i];
        printf("%s{\"name\":%s,\"address\":%s,\"down\":%.1f,\"up\":%.1f,\"received\":%llu,\"sent\":%llu}", i ? "," : "", Json(n.name).c_str(), Json(n.address).c_str(), n.down, n.up, (unsigned long long)n.received, (unsigned long long)n.sent); }
    printf("],\"disks\":[");
    for (size_t i = 0; i < s.disks.size(); ++i) { const auto& d = s.disks[i]; printf("%s{\"name\":%s,\"path\":%s,\"capacity\":%llu,\"free\":%llu}", i ? "," : "", Json(d.name).c_str(), Json(d.path).c_str(), (unsigned long long)d.capacity, (unsigned long long)d.free); }
    printf("],\"processes\":[");
    for (size_t i = 0; i < s.processes.size(); ++i) { const auto& p = s.processes[i]; printf("%s{\"id\":%ld,\"name\":%s,\"cpu\":%.3f,\"resident\":%llu,\"threads\":%d}", i ? "," : "", (long)p.id, Json(p.name).c_str(), p.cpu, (unsigned long long)p.resident, p.threads); }
    puts("]}"); fflush(stdout);
}
