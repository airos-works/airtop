#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

namespace Animation {
constexpr int64_t kHistorySpan = 120000000; // Two minutes at every sample rate.
struct Sample { int64_t time; double value; };
struct Point { double x, value; }; // Normalized horizontal position, raw value.

// Finite, bounded display interpolation. Retarget from the current displayed
// value, including when a fresh observation arrives before a transition ends.
class Value {
public:
    double At(int64_t now) const {
        if (!fDuration) return fTarget;
        double t = std::clamp(double(now - fStart) / fDuration, 0.0, 1.0);
        t = t * t * (3 - 2 * t);
        return fFrom + (fTarget - fFrom) * t;
    }
    void Target(double target, int64_t now, int64_t duration) {
        fFrom = fInitialized ? At(now) : target;
        fTarget = target; fStart = now; fDuration = std::max<int64_t>(0, duration);
        fInitialized = true;
    }
    bool Moving(int64_t now) const { return now < fStart + fDuration && std::abs(fTarget - fFrom) > .0001; }
private:
    double fFrom = 0, fTarget = 0;
    int64_t fStart = 0, fDuration = 0;
    bool fInitialized = false;
};

inline void Append(std::deque<Sample>& samples, int64_t now, double value) {
    samples.push_back({now, value});
    // Keep the point just before the left edge for exact edge interpolation.
    // Two seconds of extra history cover the delayed display at every cadence.
    while (samples.size() > 2 && samples[1].time < now - kHistorySpan - 2000000) samples.pop_front();
    while (samples.size() > 1024) samples.pop_front();
}

// Clip a time series at both viewport edges, interpolating only between real
// observations. Never extrapolate a future measurement while sampling stalls.
inline std::vector<Point> Graph(const std::deque<Sample>& samples, int64_t right) {
    std::vector<Point> points;
    int64_t left = right - kHistorySpan;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        if (i && s.time > samples[i - 1].time) {
            const auto& before = samples[i - 1];
            for (int64_t edge : {left, right}) {
                if (before.time < edge && s.time > edge) {
                    double t = double(edge - before.time) / (s.time - before.time);
                    points.push_back({double(edge - left) / kHistorySpan,
                        before.value + (s.value - before.value) * t});
                }
            }
        }
        if (s.time >= left && s.time <= right)
            points.push_back({double(s.time - left) / kHistorySpan, s.value});
        if (s.time >= right) break;
    }
    return points;
}
}
