#include "../src/Sampler.h"
#include <cassert>
#include <cstdio>
#include <unistd.h>

int main() {
    Sampler sampler;
    Snapshot data;
    assert(sampler.Interval() == 250000);
    sampler.Start();
    std::atomic<bool> stopLoad{false};
    std::thread load([&] { while (!stopLoad.load(std::memory_order_relaxed)) {} });
    int samples = 0, processUpdates = 0;
    bigtime_t start = system_time(), processTime = 0, sampleTime = 0;
    double worstMs = 0;
    double peakProcessCPU = 0;
    while (system_time() - start < 5500000) {
        if (sampler.Latest(data)) {
            assert(data.valid && data.cpuValid);
            if (sampleTime) assert(data.time - sampleTime >= 200000);
            sampleTime = data.time;
            ++samples;
            if (processTime != data.processTime) {
                if (processTime) assert(data.processTime - processTime >= 1000000);
                processTime = data.processTime;
                ++processUpdates;
                if (processUpdates > 1)
                    for (const auto& process : data.processes)
                        if (process.id == getpid()) peakProcessCPU = std::max(peakProcessCPU, process.cpu);
            }
            worstMs = std::max(worstMs, data.sampleMs);
        }
        snooze(30000);
    }
    sampler.Stop();
    stopLoad = true; load.join();
    assert(samples >= 19 && samples <= 24);
    assert(processUpdates >= 4 && processUpdates <= 6);
    assert(data.cpuHistory.size() == size_t(samples - 1));
    assert(peakProcessCPU > 70 && peakProcessCPU < 150);
    printf("cadence: %d observations, %d process updates in 5.5s, worst collection %.2f ms, one-core load %.1f%%\n",
        samples, processUpdates, worstMs, peakProcessCPU);
}
