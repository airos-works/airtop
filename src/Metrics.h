#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace Metrics {
inline double Percent(double part, double whole) {
    return whole > 0 ? std::max(0.0, std::min(100.0, part * 100.0 / whole)) : 0;
}
inline double Rate(uint64_t current, uint64_t previous, double seconds) {
    return seconds > 0 && current >= previous ? (current - previous) / seconds : 0;
}
inline std::string Bytes(double bytes, bool rate = false) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int unit = 0;
    while (bytes >= 1024 && unit < 5) { bytes /= 1024; ++unit; }
    char result[64];
    snprintf(result, sizeof(result), unit ? "%.1f %s%s" : "%.0f %s%s", bytes, units[unit], rate ? "/s" : "");
    return result;
}
inline std::string Number(double value, int precision = 1) {
    char result[64]; snprintf(result, sizeof(result), "%.*f", precision, value); return result;
}
// Family 17h SMN 0x59800: temperature in eighths of a degree. The
// 1950X adds 27 C to Tctl; return physical Tdie. Register semantics and
// offset are also documented by Linux's k10temp driver.
inline bool Threadripper1950XTemperature(uint32_t raw, double& celsius) {
    if (raw == 0 || raw == UINT32_MAX) return false;
    celsius = ((raw >> 21) & 0x7ff) * 0.125 - 27.0;
    if ((raw & (1u << 19)) || (raw & (3u << 16)) == (3u << 16)) celsius -= 49.0;
    return std::isfinite(celsius) && celsius >= 0 && celsius < 115;
}
inline bool Temperature(const std::string& text, double& result) {
    auto pos = text.find("Current Temperature:");
    if (pos == std::string::npos) return false;
    return sscanf(text.c_str() + pos + 20, "%lf", &result) == 1
        && std::isfinite(result) && result > -50 && result < 150;
}
}
