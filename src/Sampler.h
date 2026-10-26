#pragma once
#include <OS.h>
#include "Animation.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct Process {
    team_id id = -1; bigtime_t start = 0; std::string name, command;
    double cpu = 0; uint64 resident = 0, virtualSize = 0;
    int threads = 0, priority = 0; uid_t uid = 0;
};
struct Network {
    std::string name, address; bool link = false;
    uint64 received = 0, sent = 0, errors = 0;
    double down = 0, up = 0;
};
struct Disk { std::string name, path, fs, device; uint64 capacity = 0, free = 0; bool readOnly = false; };
struct Snapshot {
    uint64 sequence = 0; bigtime_t time = 0, processTime = 0, diskTime = 0; double sampleMs = 0;
    bool valid = false, cpuValid = false; std::string error, host, cpuName, os, gpu;
    std::vector<double> cores; double cpu = 0, frequency = 0;
    uint64 totalMemory = 0, availableMemory = 0, cache = 0, blockCache = 0;
    uint64 swapTotal = 0, swapUsed = 0;
    uint32 threads = 0, teams = 0; double faults = 0;
    double temperature = 0; bool hasTemperature = false; std::string sensor;
    std::vector<Process> processes; std::vector<Network> networks; std::vector<Disk> disks;
    std::deque<Animation::Sample> cpuHistory, downHistory, upHistory;
    double down = 0, up = 0;
};
class Sampler {
public:
    explicit Sampler(bool processDetails = true); ~Sampler();
    Snapshot Sample();
    void Start(); void Stop();
    bool Latest(Snapshot& snapshot);
    void SetProcessDetailsEnabled(bool enabled);
    void SetInterval(bigtime_t interval); bigtime_t Interval() const { return fInterval.load(); }
private:
    Snapshot fState;
    std::vector<cpu_info> fPreviousCPU;
    std::map<team_id, std::pair<bigtime_t, bigtime_t>> fPreviousTeams;
    std::map<std::string, std::pair<uint64, uint64>> fPreviousNetwork;
    bigtime_t fPreviousTime = 0, fDiskTime = 0, fSensorTime = 0, fProcessTime = 0, fTemperatureTime = 0;
    uint32 fPreviousFaults = 0;
    std::vector<std::string> fSensors;
    std::mutex fMutex, fWaitMutex;
    std::condition_variable fWake;
    std::thread fThread;
    std::atomic<bool> fStop{false}; std::atomic<bigtime_t> fInterval{250000};
    std::atomic<bool> fProcessDetails;
    Snapshot fPublished;
};
void PrintSnapshot(const Snapshot& snapshot);
