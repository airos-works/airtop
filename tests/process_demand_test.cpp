#include "../src/Sampler.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static void require(bool value, const char* message)
{
    if (!value) { fprintf(stderr, "DEMAND_FAIL %s\n", message); exit(1); }
}

static const Process& self(const Snapshot& snapshot)
{
    for (const auto& process : snapshot.processes)
        if (process.id == getpid()) return process;
    require(false, "current process present");
    return snapshot.processes.front();
}

static Snapshot wait_for(Sampler& sampler, bool details)
{
    Snapshot snapshot;
    bigtime_t deadline = system_time() + 3000000;
    while (system_time() < deadline) {
        if (sampler.Latest(snapshot) && snapshot.valid && snapshot.cpuValid
            && (!snapshot.processes.empty() == details)) {
            require(!snapshot.cores.empty() && snapshot.teams > 0
                && snapshot.totalMemory > 0, "system totals continue");
            require((snapshot.processTime != 0) == details, "process timestamp follows demand");
            return snapshot;
        }
        snooze(10000);
    }
    require(false, "demand transition published");
    return snapshot;
}

int main()
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    Sampler defaults;
    Snapshot initial = defaults.Sample();
    require(initial.valid && self(initial).resident > 0, "default collection unchanged");
    Sampler sampler(false);
    sampler.Start();
    std::atomic<bool> stop(false);
    std::thread busy([&] { while (!stop.load(std::memory_order_relaxed)) {} });
    Snapshot disabled = wait_for(sampler, false);
    uint64 sequence = disabled.sequence;
    double peak = 0;
    for (int round = 0; round < 4; round++) {
        sampler.SetProcessDetailsEnabled(true);
        Snapshot enabled = wait_for(sampler, true);
        require(enabled.sequence > sequence, "sampling advances on enable");
        require(self(enabled).cpu == 0, "first CPU sample starts a fresh interval");
        snooze(1250000);
        Snapshot measured;
        require(sampler.Latest(measured) && measured.processTime > enabled.processTime,
            "process counters update while demanded");
        peak = std::max(peak, self(measured).cpu);
        require(self(measured).cpu > 10 && self(measured).cpu < 200,
            "bounded load has a current CPU rate");
        require(measured.cpuHistory.size() >= enabled.cpuHistory.size(),
            "history continues while detailed collection is enabled");
        sampler.SetProcessDetailsEnabled(false);
        disabled = wait_for(sampler, false);
        require(disabled.sequence > measured.sequence, "sampling advances on disable");
        require(disabled.cpuHistory.size() >= measured.cpuHistory.size(),
            "disabling details preserves history");
        sequence = disabled.sequence;
        printf("DEMAND round=%d samples=%llu history=%zu load_cpu=%.3f\n", round,
            (unsigned long long)sequence, disabled.cpuHistory.size(), self(measured).cpu);
    }
    sampler.Stop();
    stop = true;
    busy.join();
    printf("DEMAND_PASS transitions=8 default_details=1 peak_cpu=%.3f\n", peak);
}
