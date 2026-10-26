// Bounded integration workload: one CPU, 64 MiB of touched RAM, no disk writes.
#include <OS.h>
#include <cmath>
#include <cstdio>
#include <vector>
int main() {
    std::vector<unsigned char> memory(64 * 1024 * 1024, 0x5a);
    volatile double value = .1;
    bigtime_t until = system_time() + 8000000;
    while (system_time() < until) {
        for (int i = 0; i < 10000; ++i) value = std::sin(value + i);
        memory[size_t(system_time()) % memory.size()]++;
    }
    printf("workload complete: %zu bytes, %f\n", memory.size(), value);
}
