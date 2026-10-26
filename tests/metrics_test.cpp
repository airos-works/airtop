#include "../src/Metrics.h"
#include <cassert>
#include <iostream>
int main() {
    assert(Metrics::Percent(1, 4) == 25); assert(Metrics::Percent(2, 1) == 100);
    assert(Metrics::Percent(-1, 1) == 0); assert(Metrics::Percent(1, 0) == 0);
    assert(Metrics::Rate(4096, 2048, .5) == 4096);
    assert(Metrics::Rate(0, 2048, 1) == 0); // reset/disappearing counter
    assert(Metrics::Rate(1, 0, 0) == 0);
    assert(Metrics::Bytes(1073741824) == "1.0 GiB");
    double temp = 0;
    assert(Metrics::Temperature("  Current Temperature: 52.5 C\n", temp) && temp == 52.5);
    assert(!Metrics::Temperature("Current Temperature: nan", temp));
    assert(!Metrics::Temperature("Current Temperature: 999", temp));
    assert(!Metrics::Temperature("unavailable", temp));
    assert(Metrics::Threadripper1950XTemperature(616u << 21, temp) && temp == 50);
    assert(Metrics::Threadripper1950XTemperature((1008u << 21) | (1u << 19), temp) && temp == 50);
    assert(Metrics::Threadripper1950XTemperature((1008u << 21) | (3u << 16), temp) && temp == 50);
    assert(!Metrics::Threadripper1950XTemperature(0, temp));
    assert(!Metrics::Threadripper1950XTemperature(UINT32_MAX, temp));
    assert(!Metrics::Threadripper1950XTemperature(2047u << 21, temp));
    std::cout << "metrics: 18 checks passed\n";
}
