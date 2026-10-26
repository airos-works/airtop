#include "../src/Animation.h"
#include <cassert>
#include <iostream>

static bool Near(double a, double b) { return std::abs(a - b) < .000001; }

int main() {
    Animation::Value value;
    value.Target(20, 100, 100);
    assert(value.At(100) == 20); // First observation appears immediately.
    value.Target(80, 200, 100);
    assert(value.At(200) == 20 && value.At(300) == 80);
    assert(value.At(250) == 50 && value.Moving(250) && !value.Moving(300));
    for (int t = 200; t <= 300; ++t) assert(value.At(t) >= 20 && value.At(t) <= 80);
    value.Target(0, 250, 100);
    assert(value.At(250) == 50); // An interrupted transition never jumps.
    assert(value.At(350) == 0 && value.At(1000) == 0);
    value.Target(10, 1000, 0);
    assert(value.At(1000) == 10 && !value.Moving(1000));

    using Animation::Sample;
    constexpr int64_t span = Animation::kHistorySpan;
    std::deque<Sample> samples{{0, 0}, {span, 100}, {span * 2, 0}};
    auto points = Animation::Graph(samples, span + span / 2);
    assert(points.size() == 3);
    assert(Near(points[0].x, 0) && Near(points[0].value, 50));
    assert(Near(points[1].x, .5) && Near(points[1].value, 100));
    assert(Near(points[2].x, 1) && Near(points[2].value, 50));
    points = Animation::Graph(samples, span * 2);
    assert(points.size() == 2 && points[0].value == 100 && points[1].value == 0);
    assert(Animation::Graph(samples, -1).empty());
    assert(Animation::Graph(samples, span * 4).empty());
    points = Animation::Graph({{0, 1}, {span * 3, 4}}, span * 2);
    assert(points.size() == 2 && Near(points[0].value, 2) && Near(points[1].value, 3));
    points = Animation::Graph({{0, 10}, {span / 2, 20}}, span);
    assert(points.size() == 2 && Near(points.back().x, .5)); // No extrapolation.
    samples.clear();
    for (int64_t t = 0; t <= span * 3; t += t < span ? 1000000 : 250000)
        Animation::Append(samples, t, t % 100);
    assert(samples.size() < 500 && samples.front().time <= span * 2);
    points = Animation::Graph(samples, span * 3 - 250000);
    assert(Near(points.front().x, 0) && Near(points.back().x, 1));
    samples.clear();
    for (int i = 0; i < 2000; ++i) Animation::Append(samples, i, i);
    assert(samples.size() == 1024);
    std::cout << "animation: transitions, bounds, retargeting, clipping, mixed cadence, and retention passed\n";
}
