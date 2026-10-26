#include "AuxPanel.h"
#include "Metrics.h"
#include "Theme.h"
#include <Bitmap.h>
#include <Font.h>
#include <Shape.h>
#include <String.h>
#include <View.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <new>
#include <ctime>

using namespace Theme;

namespace {
constexpr bigtime_t kFrameInterval = 200000;  // 5 frames a second: a frame costs app_server ~7 ms of software drawing on a Pi 4 (about 1% of the system at this rate)
constexpr float kDesignWidth = 640, kDesignHeight = 480;
}

AuxPanel::AuxPanel(Sampler& sampler) : fSampler(sampler) {}
AuxPanel::~AuxPanel() { Stop(); }

bool AuxPanel::Start() {
    if (fRunning) return true;
    if (fDisplay.SetTo(0) != B_OK) return false;
    fBitmap = fDisplay.Bitmap();
    if (fBitmap == nullptr) { fDisplay.Unset(); return false; }
    fScale = std::min(fDisplay.Width() / kDesignWidth, fDisplay.Height() / kDesignHeight);
    fView = new BView(fBitmap->Bounds(), "aux panel", B_FOLLOW_NONE, 0);
    fBitmap->AddChild(fView);
    if (fDisplay.SetPower(true) != B_OK) {
        delete fBitmap; fBitmap = nullptr; fView = nullptr; fDisplay.Unset(); return false;
    }
    fStop = false; fRunning = true;
    fThread = std::thread([this] { Run(); });
    return true;
}

void AuxPanel::Stop() {
    if (!fRunning) return;
    fStop = true;
    if (fThread.joinable()) fThread.join();
    fRunning = false;
    // Leave a dark panel, then release the hardware.
    if (fBitmap && fBitmap->Lock()) {
        fView->SetHighColor(bg); fView->FillRect(fBitmap->Bounds()); fView->Sync(); fBitmap->Unlock();
        fDisplay.Present();
    }
    fDisplay.SetPower(false);
    fBackground.reset();
    fBackgroundAttempted = false;
    fDisplay.Unset();   // owns and deletes the bitmap
    fBitmap = nullptr; fView = nullptr;
}

void AuxPanel::Run() {
    while (!fStop) {
        bigtime_t now = system_time();
        if (fSampler.Latest(fData)) {
            bigtime_t duration = std::min<bigtime_t>(250000, fSampler.Interval());
            fCPUValue.Target(fData.cpu, now, duration);
            fTemperatureValue.Target(fData.temperature, now, duration);
            fCoreValues.resize(fData.cores.size());
            for (size_t i = 0; i < fData.cores.size(); ++i) fCoreValues[i].Target(fData.cores[i], now, duration);
            double maximum = 1024;
            for (const auto& s : fData.downHistory) maximum = std::max(maximum, s.value);
            for (const auto& s : fData.upHistory) maximum = std::max(maximum, s.value);
            fNetworkScale.Target(maximum, now, duration);
        }
        fFrameTime = now;
        bigtime_t cursor = std::min(now - fSampler.Interval(), fData.time);
        if (cursor > fGraphTime) fGraphTime = cursor;
        if (fBitmap->Lock()) {
            bigtime_t t0 = system_time();
            Render(now);
            fView->Sync();
            bigtime_t t1 = system_time();
            fBitmap->Unlock();
            const char* skipPresent = getenv("AIRTOP_PANEL_SKIP");
            if (skipPresent != nullptr && strstr(skipPresent, "present") != nullptr) ++fFrames;
            else if (fDisplay.Present() == B_OK) ++fFrames;
            if (getenv("AIRTOP_PANEL_TRACE") != nullptr && fFrames % 10 == 0)
                printf("panel frame: draw %lld us, present %lld us\n", (long long)(t1 - t0), (long long)(system_time() - t1));
        }
        bigtime_t spent = system_time() - now;
        if (spent < kFrameInterval) snooze(kFrameInterval - spent);
    }
}

// Drawing: the design is 640x480; other panels get the same layout scaled.
void AuxPanel::Render(bigtime_t) {
    const char* skip = getenv("AIRTOP_PANEL_SKIP");   // lab: which parts to leave out
    fBackgroundDrawn = false;
    if (skip == nullptr) {
        if (!fBackgroundAttempted) PrepareBackground();
        if (fBackground) {
            // Copy device pixels directly; the cached rounded edges were
            // already rasterized at this panel's actual scale.
            fView->SetScale(1);
            fView->DrawBitmap(fBackground.get(), B_ORIGIN);
            fBackgroundDrawn = true;
        }
    }
    fView->SetScale(fScale);
    if (!fBackgroundDrawn) {
        fView->SetHighColor(bg);
        fView->FillRect(BRect(0, 0, kDesignWidth, kDesignHeight));
    }
    if (skip && strstr(skip, "all")) return;
    if (!(skip && strstr(skip, "header"))) DrawHeader(BRect(0, 0, kDesignWidth, 40));
    if (!(skip && strstr(skip, "cpu"))) DrawCPU(BRect(10, 46, 630, 262));
    if (!(skip && strstr(skip, "net"))) DrawNetwork(BRect(10, 270, 395, 470));
    if (!(skip && strstr(skip, "temp"))) DrawTemperatures(BRect(403, 270, 630, 470));
}

// Cache the fixed panel frames, titles and captions. Samples, graphs and
// animation still render every frame; allocation failure keeps the direct path.
void AuxPanel::PrepareBackground() {
    fBackgroundAttempted = true;
    BRect bounds = fBitmap->Bounds() & BRect(0, 0,
        floorf(kDesignWidth * fScale), floorf(kDesignHeight * fScale));
    std::unique_ptr<BBitmap> bitmap(new(std::nothrow) BBitmap(bounds, 0,
        fBitmap->ColorSpace()));
    if (!bitmap || bitmap->InitCheck() != B_OK) return;
    // Use the panel's existing drawing window once, then retain only its
    // pixels. The cache needs no extra offscreen window or drawing thread.
    fView->SetScale(fScale);
    fView->SetHighColor(bg);
    fView->FillRect(BRect(0, 0, kDesignWidth, kDesignHeight));
    Panel(BRect(10, 46, 630, 262), "PROCESSOR", "2 MIN HISTORY", cyan);
    Panel(BRect(10, 270, 395, 470), "NETWORK", "ALL INTERFACES", green);
    Panel(BRect(403, 270, 630, 470), "TEMPERATURE", "", amber);
    fView->Sync();
    if (bitmap->ImportBits(fBitmap, B_ORIGIN, B_ORIGIN,
            BSize(bounds.Width(), bounds.Height())) != B_OK) return;
    fBackground = std::move(bitmap);
}

void AuxPanel::Text(float x, float y, const std::string& text, rgb_color color, float size, bool bold) {
    BFont font(bold ? be_bold_font : be_plain_font); font.SetSize(size);
    fView->SetFont(&font); fView->SetHighColor(color); fView->DrawString(text.c_str(), BPoint(x, y));
}
void AuxPanel::Right(float x, float y, const std::string& text, rgb_color color, float size, bool bold) {
    BFont font(bold ? be_bold_font : be_plain_font); font.SetSize(size); fView->SetFont(&font);
    Text(x - fView->StringWidth(text.c_str()), y, text, color, size, bold);
}
void AuxPanel::PanelFrame(BRect rect, rgb_color color) {
    fView->SetHighColor(panel); fView->FillRoundRect(rect, 9, 9);
    fView->SetHighColor(border); fView->StrokeRoundRect(rect, 9, 9);
    fView->SetHighColor(color); fView->FillRoundRect(BRect(rect.left + 12, rect.top + 12, rect.left + 15, rect.top + 25), 1, 1);
}
void AuxPanel::Panel(BRect rect, const std::string& title, const std::string& meta, rgb_color color) {
    if (fBackgroundDrawn) return;
    PanelFrame(rect, color);
    Text(rect.left + 23, rect.top + 23, title, ink, 13, true);
    Right(rect.right - 13, rect.top + 23, meta, muted, 10);
}
void AuxPanel::Bar(BRect rect, double percent, rgb_color color) {
    fView->SetHighColor(faint); fView->FillRoundRect(rect, 2, 2);
    BRect fill = rect; fill.right = rect.left + rect.Width() * std::clamp(percent, 0.0, 100.0) / 100;
    if (fill.Width() > 0) { fView->SetHighColor(color); fView->FillRoundRect(fill, 2, 2); }
}
void AuxPanel::Graph(BRect rect, const std::deque<Animation::Sample>& history, double maximum, rgb_color color) {
    fView->SetHighColor(faint);
    for (int i = 0; i < 4; ++i) { float y = rect.top + rect.Height() * i / 3; fView->StrokeLine(BPoint(rect.left, y), BPoint(rect.right, y)); }
    auto points = Animation::Graph(history, fGraphTime);
    if (points.empty() || maximum <= 0) return;
    // app_server draws the shape in software: a point every few pixels is
    // all the panel can show anyway. Keep each bucket's largest value so
    // short spikes stay visible.
    size_t budget = std::max<size_t>(16, size_t(rect.Width() / 4));
    if (points.size() > budget) {
        std::vector<Animation::Point> reduced;
        size_t per = (points.size() + budget - 1) / budget;
        for (size_t i = 0; i < points.size(); i += per) {
            Animation::Point best = points[i];
            for (size_t j = i; j < std::min(points.size(), i + per); ++j)
                if (points[j].value > best.value) best = points[j];
            reduced.push_back(best);
        }
        if (reduced.back().x != points.back().x) reduced.push_back(points.back());
        points.swap(reduced);
    }
    rgb_color fill = {(uint8)((color.red + panel.red * 4) / 5), (uint8)((color.green + panel.green * 4) / 5), (uint8)((color.blue + panel.blue * 4) / 5), 255};
    BShape line, area; BPoint previous;
    for (size_t i = 0; i < points.size(); ++i) {
        BPoint point(rect.left + rect.Width() * points[i].x,
            rect.bottom - rect.Height() * std::clamp(points[i].value / maximum, 0.0, 1.0));
        if (i == 0) { line.MoveTo(point); area.MoveTo(BPoint(point.x, rect.bottom)); area.LineTo(point); }
        else {
            float third = (point.x - previous.x) / 3;
            BPoint controls[] = {BPoint(previous.x + third, previous.y), BPoint(point.x - third, point.y), point};
            line.BezierTo(controls); area.BezierTo(controls);
        }
        previous = point;
    }
    area.LineTo(BPoint(previous.x, rect.bottom)); area.Close();
    fView->MovePenTo(B_ORIGIN); fView->SetHighColor(fill); fView->FillShape(&area);
    fView->MovePenTo(B_ORIGIN); fView->SetHighColor(color); fView->SetPenSize(1.5); fView->StrokeShape(&line); fView->SetPenSize(1);
    fView->FillEllipse(previous, 2.5, 2.5);
}

void AuxPanel::DrawHeader(BRect r) {
    Text(r.left + 12, r.top + 27, fData.host.empty() ? "AirTop" : fData.host, ink, 18, true);
    time_t clock = time(nullptr); char stamp[32]; strftime(stamp, sizeof(stamp), "%H:%M:%S", localtime(&clock));
    Right(r.right - 12, r.top + 27, stamp, ink, 18, true);
    bigtime_t up = system_time() / 1000000; char uptime[64];
    snprintf(uptime, sizeof(uptime), "up %lldd %02lld:%02lld", (long long)(up / 86400), (long long)(up % 86400 / 3600), (long long)(up % 3600 / 60));
    Right(r.right - 120, r.top + 27, uptime, muted, 11);
    if (!fData.valid) Text(r.left + 140, r.top + 27, fData.error.empty() ? "waiting for the first sample" : fData.error, amber, 11);
}

void AuxPanel::DrawCPU(BRect r) {
    Panel(r, "PROCESSOR", "2 MIN HISTORY", cyan);
    std::string name = fData.cpuName.empty() ? "" : fData.cpuName;
    { BFont font(be_plain_font); font.SetSize(11); BString value(name.c_str()); font.TruncateString(&value, B_TRUNCATE_END, r.Width() - 160); Text(r.left + 14, r.top + 44, value.String(), muted, 11); }
    Right(r.right - 14, r.top + 56, fData.cpuValid ? Metrics::Number(fCPUValue.At(fFrameTime)) + "%" : "—", cyan, 30, true);
    BRect graph(r.left + 14, r.top + 64, r.right - 14, r.top + 130);
    Graph(graph, fData.cpuHistory, 100, cyan);
    Text(graph.left + 3, graph.top + 11, "100", muted, 8);
    size_t cores = fData.cores.size();
    int columns = cores > 8 ? 4 : 2;
    int rows = std::max(1, (int(cores) + columns - 1) / columns);
    float cell = (r.Width() - 28) / columns, rowHeight = std::min(22.f, 70.f / rows);
    for (size_t i = 0; i < cores; ++i) {
        int column = i / rows, row = i % rows;
        float x = r.left + 14 + column * cell, y = graph.bottom + 20 + row * rowHeight;
        double value = i < fCoreValues.size() ? fCoreValues[i].At(fFrameTime) : 0;
        Text(x, y, std::to_string(i), muted, 10);
        Bar(BRect(x + 16, y - 8, x + cell - 44, y), value, value > 85 ? amber : cyan);
        Right(x + cell - 8, y, Metrics::Number(value, 0) + "%", ink, 10);
    }
    std::string speed = fData.frequency > 0 ? Metrics::Number(fData.frequency / 1e9, 2) + " GHz" : "";
    Text(r.left + 14, r.bottom - 9, speed + (speed.empty() ? "" : "  •  ") + std::to_string(fData.threads) + " threads  •  " + std::to_string(fData.teams) + " teams", muted, 10);
}

void AuxPanel::DrawNetwork(BRect r) {
    Panel(r, "NETWORK", "ALL INTERFACES", green);
    float half = r.left + r.Width() / 2;
    Text(r.left + 14, r.top + 50, "↓ " + Metrics::Bytes(fData.down, true), green, 16, true);
    Text(half + 4, r.top + 50, "↑ " + Metrics::Bytes(fData.up, true), cyan, 16, true);
    double max = std::max(1024.0, fNetworkScale.At(fFrameTime));
    for (const auto& p : Animation::Graph(fData.downHistory, fGraphTime)) max = std::max(max, p.value);
    for (const auto& p : Animation::Graph(fData.upHistory, fGraphTime)) max = std::max(max, p.value);
    Graph(BRect(r.left + 14, r.top + 62, half - 6, r.top + 140), fData.downHistory, max, green);
    Graph(BRect(half + 4, r.top + 62, r.right - 14, r.top + 140), fData.upHistory, max, cyan);
    Right(r.right - 14, r.top + 72, Metrics::Bytes(max, true), muted, 8);
    float y = r.top + 160;
    if (fData.networks.empty()) { Text(r.left + 14, y, "No network interfaces", muted, 10); return; }
    for (const auto& n : fData.networks) {
        if (y > r.bottom - 8) break;
        BFont font(be_plain_font); font.SetSize(10);
        BString line((n.name + "  " + n.address + (n.link ? "" : "  (down)")).c_str());
        font.TruncateString(&line, B_TRUNCATE_END, r.Width() - 28);
        Text(r.left + 14, y, line.String(), n.link ? ink : muted, 10);
        y += 15;
    }
}

void AuxPanel::DrawTemperatures(BRect r) {
    Panel(r, "TEMPERATURE", "", amber);
    if (!fData.hasTemperature) {
        Text(r.left + 14, r.top + 70, "No sensor", muted, 14);
        Text(r.left + 14, r.top + 90, "found under /dev/power", muted, 10);
        return;
    }
    double value = fTemperatureValue.At(fFrameTime);
    rgb_color color = value >= 80 ? pink : value >= 65 ? amber : green;
    Text(r.left + 14, r.top + 86, Metrics::Number(value, 1) + " °C", color, 40, true);
    Bar(BRect(r.left + 14, r.top + 100, r.right - 14, r.top + 110), value, color);
    Text(r.left + 14, r.top + 126, "0", muted, 8); Right(r.right - 14, r.top + 126, "100 °C", muted, 8);
    std::string sensor = fData.sensor;
    if (sensor.rfind("/dev/", 0) == 0) sensor = sensor.substr(5);
    { BFont font(be_plain_font); font.SetSize(10); BString line(sensor.c_str()); font.TruncateString(&line, B_TRUNCATE_MIDDLE, r.Width() - 28); Text(r.left + 14, r.top + 150, line.String(), muted, 10); }
    uint64 used = fData.totalMemory > fData.availableMemory ? fData.totalMemory - fData.availableMemory : 0;
    Text(r.left + 14, r.top + 176, "Memory " + Metrics::Bytes(used) + " of " + Metrics::Bytes(fData.totalMemory), muted, 10);
    Bar(BRect(r.left + 14, r.top + 182, r.right - 14, r.top + 188), Metrics::Percent(used, fData.totalMemory), purple);
}
