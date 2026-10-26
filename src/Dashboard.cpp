#include "Dashboard.h"
#include "Metrics.h"
#include "DisplayScale.h"
#include "AuxPanel.h"
#include <Alert.h>
#include <Application.h>
#include <File.h>
#include <FindDirectory.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <Screen.h>
#include <Region.h>
#include <Shape.h>
#include <String.h>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "Theme.h"
using namespace Theme;
namespace {
std::string Lower(std::string text) { for (char& c : text) c = std::tolower((unsigned char)c); return text; }
BPath SettingsPath() { BPath path; find_directory(B_USER_SETTINGS_DIRECTORY, &path); path.Append("AirTop_settings"); return path; }
}
Dashboard::Dashboard(BRect frame, Sampler& sampler)
    : BView(frame, "dashboard", B_FOLLOW_ALL, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_PULSE_NEEDED | B_NAVIGABLE | B_FRAME_EVENTS), fSampler(sampler) {
    SetViewColor(B_TRANSPARENT_COLOR); SetLowColor(panel);
}
Dashboard::~Dashboard() { delete fBuffer; }
void Dashboard::AttachedToWindow() {
    MakeFocus();
    accelerant_device_info gpu{}; BScreen screen(Window());
    if (screen.GetDeviceInfo(&gpu) == B_OK) fGpu = std::string(gpu.name) + "  /  " + gpu.chipset;
    if (fGpu.empty()) fGpu = "Display adapter";
    UpdateDisplayScale();
    Layout();
}
void Dashboard::UpdateDisplayScale() {
    BScreen screen(Window()); display_mode mode{};
    if (screen.GetMode(&mode) != B_OK) return;
    BRect frame = screen.Frame();
    float scale = DisplayScale::FromDimensions(mode.virtual_width, mode.virtual_height,
        frame.Width() + 1, frame.Height() + 1);
    if (scale == fBufferScale) return;
    fBufferScale = scale;
    delete fBuffer; fBuffer = nullptr; fBufferView = nullptr;
    Invalidate();
}
void Dashboard::Layout() {
    float w = Bounds().Width(), h = Bounds().Height();
    float x = std::floor(w * .65f), middle = 330;
    fCPU = BRect(16, 84, x - 6, middle);
    fMemory = BRect(x + 6, 84, w - 16, middle);
    fNetwork = BRect(16, middle + 12, x - 6, middle + 176);
    fHardware = BRect(x + 6, middle + 12, w - 16, middle + 176);
    fProcesses = BRect(16, middle + 188, x - 6, h - 38);
    fDisks = BRect(x + 6, middle + 188, w - 16, h - 38);
    fPauseButton = BRect(w - 211, 21, w - 121, 54);
    fSpeedButton = BRect(w - 109, 21, w - 17, 54);
    fSearch = BRect(fProcesses.left + 14, fProcesses.top + 42, fProcesses.right - 14, fProcesses.top + 72);
    fTable = BRect(fProcesses.left + 10, fProcesses.top + 82, fProcesses.right - 10, fProcesses.bottom - 29);
}
void Dashboard::FrameResized(float, float) {
    delete fBuffer; fBuffer = nullptr; fBufferView = nullptr;
    Layout(); Invalidate();
}
void Dashboard::Text(float x, float y, const std::string& text, rgb_color color, float size, bool bold) {
    BFont font(bold ? be_bold_font : be_plain_font); font.SetSize(size); fPainter->SetFont(&font); fPainter->SetHighColor(color); fPainter->DrawString(text.c_str(), BPoint(x, y));
}
void Dashboard::Right(float x, float y, const std::string& text, rgb_color color, float size) {
    BFont font(be_plain_font); font.SetSize(size); fPainter->SetFont(&font); Text(x - fPainter->StringWidth(text.c_str()), y, text, color, size);
}
void Dashboard::FitText(float x, float y, float width, const std::string& text, rgb_color color, float size, bool bold) {
    BFont font(bold ? be_bold_font : be_plain_font); font.SetSize(size); BString value(text.c_str()); font.TruncateString(&value, B_TRUNCATE_END, std::max(1.f, width)); Text(x, y, value.String(), color, size, bold);
}
void Dashboard::Panel(BRect rect, const std::string& title, const std::string& meta, rgb_color color) {
    fPainter->SetHighColor(panel); fPainter->FillRoundRect(rect, 9, 9); fPainter->SetHighColor(border); fPainter->StrokeRoundRect(rect, 9, 9);
    fPainter->SetHighColor(color); fPainter->FillRoundRect(BRect(rect.left + 14, rect.top + 16, rect.left + 17, rect.top + 27), 1, 1);
    Text(rect.left + 25, rect.top + 26, title, ink, 12, true); Right(rect.right - 15, rect.top + 26, meta, muted, 10);
}
void Dashboard::Bar(BRect rect, double percent, rgb_color color, bool segmented) {
    fPainter->SetHighColor(faint); fPainter->FillRoundRect(rect, 2, 2);
    float end = rect.left + rect.Width() * Metrics::Percent(percent, 100) / 100;
    if (end > rect.left) { fPainter->SetHighColor(color); fPainter->FillRoundRect(BRect(rect.left, rect.top, end, rect.bottom), 2, 2); }
    if (segmented) { fPainter->SetHighColor(panel); for (float x = rect.left + 5; x < rect.right; x += 7) fPainter->StrokeLine(BPoint(x, rect.top), BPoint(x, rect.bottom)); }
}
void Dashboard::Graph(BRect rect, const std::deque<Animation::Sample>& history, double maximum, rgb_color color, bool grid) {
    if (grid) { fPainter->SetHighColor(faint); for (int i = 0; i < 4; ++i) { float y = rect.top + rect.Height() * i / 3; fPainter->StrokeLine(BPoint(rect.left, y), BPoint(rect.right, y)); } }
    const auto points = Animation::Graph(history, fGraphTime);
    if (points.empty() || maximum <= 0) return;
    rgb_color fill = {(uint8)((color.red + panel.red * 4) / 5), (uint8)((color.green + panel.green * 4) / 5), (uint8)((color.blue + panel.blue * 4) / 5), 255};
    BShape line, area;
    BPoint previous;
    for (size_t i = 0; i < points.size(); ++i) {
        BPoint point(rect.left + rect.Width() * points[i].x,
            rect.bottom - rect.Height() * std::clamp(points[i].value / maximum, 0.0, 1.0));
        if (i == 0) {
            line.MoveTo(point); area.MoveTo(BPoint(point.x, rect.bottom)); area.LineTo(point);
        } else {
            // Horizontal handles keep the curve within the observed endpoints:
            // rounded joins without spline overshoot or invented load spikes.
            float third = (point.x - previous.x) / 3;
            BPoint controls[] = {BPoint(previous.x + third, previous.y), BPoint(point.x - third, point.y), point};
            line.BezierTo(controls); area.BezierTo(controls);
        }
        previous = point;
    }
    area.LineTo(BPoint(previous.x, rect.bottom)); area.Close();
    fPainter->MovePenTo(B_ORIGIN); fPainter->SetHighColor(fill); fPainter->FillShape(&area);
    fPainter->MovePenTo(B_ORIGIN); fPainter->SetHighColor(color); fPainter->SetPenSize(1.5); fPainter->StrokeShape(&line); fPainter->SetPenSize(1);
    fPainter->FillEllipse(previous, 2.5, 2.5);
}
void Dashboard::Retarget(bigtime_t now) {
    bigtime_t duration = std::min<bigtime_t>(250000, fSampler.Interval());
    fCPUValue.Target(fData.cpu, now, duration);
    fMemoryValue.Target(Metrics::Percent(fData.totalMemory - fData.availableMemory, fData.totalMemory), now, duration);
    fSwapValue.Target(Metrics::Percent(fData.swapUsed, fData.swapTotal), now, duration);
    fTemperatureValue.Target(fData.temperature, now, duration);
    fCoreValues.resize(fData.cores.size());
    for (size_t i = 0; i < fData.cores.size(); ++i) fCoreValues[i].Target(fData.cores[i], now, duration);
    double maximum = 1024;
    for (const auto& s : fData.downHistory) maximum = std::max(maximum, s.value);
    for (const auto& s : fData.upHistory) maximum = std::max(maximum, s.value);
    fNetworkScale.Target(maximum, now, duration);
}
void Dashboard::Pulse() {
    // Also detect display changes while paused; cached pixels at the old
    // density must not be stretched when the desktop scale changes.
    bigtime_t now = system_time();
    if (now - fScaleCheckTime >= 1000000) {
        fScaleCheckTime = now; UpdateDisplayScale();
    }
    if (fPaused || IsHidden() || Window()->IsMinimized()
        || !(Window()->Workspaces() & (1u << current_workspace()))) return;
    fFrameTime = system_time();
    bigtime_t processTime = fData.processTime, diskTime = fData.diskTime;
    if (fSampler.Latest(fData)) {
        Retarget(fFrameTime);
        Invalidate(fMemory); Invalidate(fHardware);
        if (processTime != fData.processTime) {
            Sort(); Invalidate(fProcesses);
            Invalidate(BRect(0, 0, Bounds().right, 76));
            Invalidate(BRect(0, Bounds().bottom - 32, Bounds().right, Bounds().bottom));
        }
        if (diskTime != fData.diskTime) Invalidate(fDisks);
    }
    // Show one observation interval behind collection so the advancing edge
    // can interpolate known data. Never scroll backwards on an interval change.
    bigtime_t cursor = std::min(fFrameTime - fSampler.Interval(), fData.time);
    bool scrolling = cursor > fGraphTime;
    if (scrolling) fGraphTime = cursor;
    if (scrolling || fCPUValue.Moving(fFrameTime)) {
        Invalidate(BRect(fCPU.left + 10, fCPU.top + 37, fCPU.right - 10, fCPU.bottom - 6));
        Invalidate(BRect(fNetwork.left + 10, fNetwork.top + 35, fNetwork.right - 10, fNetwork.bottom - 7));
    }
    if (fMemoryValue.Moving(fFrameTime) || fSwapValue.Moving(fFrameTime)) Invalidate(fMemory);
    if (fTemperatureValue.Moving(fFrameTime)) Invalidate(fHardware);
    if (fData.valid && fFrameTime - fData.time > 10000000)
        Invalidate(BRect(0, Bounds().bottom - 32, Bounds().right, Bounds().bottom));
}
void Dashboard::Draw(BRect update) {
    bigtime_t started = system_time();
    bool fresh = false;
    if (!fBuffer) {
        BRect pixels = DisplayScale::Pixels(Bounds(), fBufferScale);
        fBuffer = new BBitmap(pixels, B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
        if (fBuffer->InitCheck() == B_OK) {
            fBufferView = new BView(pixels, "frame buffer", B_FOLLOW_NONE, B_WILL_DRAW);
            fBuffer->AddChild(fBufferView);
            fresh = true;
        } else { delete fBuffer; fBuffer = nullptr; }
    }
    if (fBuffer && fBuffer->Lock()) {
        fPainter = fBufferView;
        BRect paint = fresh ? Bounds() : update;
        BRect pixels = DisplayScale::Pixels(paint, fBufferScale);
        BRegion clip(pixels);
        fPainter->PushState(); fPainter->ConstrainClippingRegion(&clip);
        fPainter->SetHighColor(bg); fPainter->FillRect(pixels);
        // Keep clipping in device pixels in the parent state, then rasterize
        // fonts and geometry at native density using logical layout units.
        fPainter->PushState(); fPainter->SetScale(fBufferScale); fPainter->SetLowColor(panel);
        Render(paint);
        fPainter->PopState(); fPainter->PopState(); fPainter->Sync();
        fBuffer->Unlock(); fPainter = this;
        // Present a completed frame; the user never sees the intermediate
        // background clear, graph fill, text, and line drawing operations.
        // The window's update clip limits the copy. Using the full mapping
        // avoids rounding a different scale for each partial dirty rectangle.
        BView::DrawBitmap(fBuffer, fBuffer->Bounds(), Bounds());
    } else { fPainter = this; Render(update); }
    ++fFrames;
    fDrawMs = (system_time() - started) / 1000.0;
}
void Dashboard::Render(BRect update) {
    fPainter->SetHighColor(bg); fPainter->FillRect(update);
    if (update.top < 77) {
        Text(17, 41, "air", ink, 29, true); Text(55, 41, "TOP", cyan, 29, true);
        Text(18, 63, "SYSTEM OBSERVATORY", muted, 9, true);
        FitText(165, 34, Bounds().Width() - 400, fData.host + "  /  " + std::to_string(fData.cores.size()) + " logical CPUs", ink, 12, true);
        bigtime_t seconds = fData.time / 1000000; char uptime[100];
        snprintf(uptime, sizeof(uptime), "UP %lldd %02lldh %02lldm", (long long)(seconds / 86400), (long long)(seconds / 3600 % 24), (long long)(seconds / 60 % 60));
        FitText(165, 55, Bounds().Width() - 400, std::string(uptime) + "   •   " + fData.os, muted, 10);
        for (BRect rect : {fPauseButton, fSpeedButton}) { fPainter->SetHighColor(panel); fPainter->FillRoundRect(rect, 6, 6); fPainter->SetHighColor(border); fPainter->StrokeRoundRect(rect, 6, 6); }
        Text(fPauseButton.left + 13, 43, fPaused ? "RESUME" : "PAUSE", fPaused ? amber : cyan, 11, true);
        Text(fSpeedButton.left + 15, 43, Metrics::Number(fSampler.Interval() / 1000000.0, fSampler.Interval() == 250000 ? 2 : 1) + "s", ink, 11);
    }
    if (update.Intersects(fCPU)) DrawCPU();
    if (update.Intersects(fMemory)) DrawMemory();
    if (update.Intersects(fNetwork)) DrawNetwork();
    if (update.Intersects(fHardware)) DrawHardware();
    if (update.Intersects(fProcesses)) DrawProcesses();
    if (update.Intersects(fDisks)) DrawDisks();
    if (update.bottom >= Bounds().bottom - 32) {
        Text(17, Bounds().bottom - 14, "SPACE pause    / filter    C CPU    M memory    ↑↓ select    ENTER details    F1 help", muted, 10);
        std::string status = fPaused ? "PAUSED" : !fData.valid ? "WAITING FOR DATA" : (system_time() - fData.time > 10000000 ? "DATA DELAYED" : "LIVE");
        Right(Bounds().right - 16, Bounds().bottom - 14, status + "  •  " + Metrics::Number(fData.sampleMs) + " ms sample", fPaused ? amber : green, 10);
    }
}
void Dashboard::DrawCPU() {
    BRect r = fCPU; Panel(r, "PROCESSOR", "2 MIN HISTORY", cyan);
    FitText(r.left + 15, r.top + 49, r.Width() - 170, fData.cpuName, muted, 11);
    Right(r.right - 16, r.top + 59, fData.cpuValid ? Metrics::Number(fCPUValue.At(fFrameTime)) + "%" : "—", cyan, 29);
    BRect graph(r.left + 16, r.top + 71, r.right - 16, r.top + 124);
    Graph(graph, fData.cpuHistory, 100, cyan);
    Text(graph.left, graph.top + 12, "100", muted, 8);
    int columns = fData.cores.size() > 32 ? 8 : 4;
    int rows = std::max(1, (int(fData.cores.size()) + columns - 1) / columns);
    float cell = (r.Width() - 32) / columns, rowHeight = std::min(19.f, 89.f / rows);
    for (size_t i = 0; i < fData.cores.size(); ++i) {
        int column = i / rows, row = i % rows;
        float x = r.left + 16 + column * cell, y = graph.bottom + 18 + row * rowHeight;
        Text(x, y, std::to_string(i), muted, 9);
        float numberWidth = columns > 4 ? 24 : 30;
        Bar(BRect(x + 19, y - 7, x + cell - numberWidth - 9, y), fCoreValues[i].At(fFrameTime), fCoreValues[i].At(fFrameTime) > 85 ? amber : cyan, true);
        Right(x + cell - 8, y + 1, Metrics::Number(fCoreValues[i].At(fFrameTime), 0), ink, 9);
    }
    std::string speed = fData.frequency > 0 ? Metrics::Number(fData.frequency / 1e9, 2) + " GHz" : "Clock unavailable";
    Text(r.left + 16, r.bottom - 10, speed + "  •  " + std::to_string(fData.threads) + " threads", muted, 10);
    Right(r.right - 16, r.bottom - 10, "CORE LOAD  /  %", muted, 9);
}
void Dashboard::DrawMemory() {
    BRect r = fMemory; Panel(r, "MEMORY", Metrics::Bytes(fData.totalMemory), purple);
    uint64 used = fData.totalMemory - fData.availableMemory;
    Text(r.left + 16, r.top + 67, Metrics::Bytes(used), purple, 29, true);
    Right(r.right - 16, r.top + 66, Metrics::Number(Metrics::Percent(used, fData.totalMemory)) + "%", purple, 20);
    Text(r.left + 16, r.top + 87, "IN USE", muted, 9, true);
    Bar(BRect(r.left + 16, r.top + 100, r.right - 16, r.top + 108), fMemoryValue.At(fFrameTime), purple);
    Text(r.left + 16, r.top + 133, "Available", muted, 11); Right(r.right - 16, r.top + 133, Metrics::Bytes(fData.availableMemory), ink, 11);
    Text(r.left + 16, r.top + 154, "Cached pages", muted, 11); Right(r.right - 16, r.top + 154, Metrics::Bytes(fData.cache), ink, 11);
    Text(r.left + 16, r.top + 175, "Block cache", muted, 11); Right(r.right - 16, r.top + 175, Metrics::Bytes(fData.blockCache), ink, 11);
    Text(r.left + 16, r.top + 199, "Swap", muted, 11); Right(r.right - 16, r.top + 199, Metrics::Bytes(fData.swapUsed) + " / " + Metrics::Bytes(fData.swapTotal), ink, 11);
    Bar(BRect(r.left + 16, r.top + 211, r.right - 16, r.top + 215), fSwapValue.At(fFrameTime), pink);
    Text(r.left + 16, r.bottom - 10, Metrics::Number(fData.faults, 0) + " page faults/s", muted, 10);
}
void Dashboard::DrawNetwork() {
    BRect r = fNetwork; Panel(r, "NETWORK", "ALL INTERFACES", green);
    float half = r.left + r.Width() / 2;
    Text(r.left + 16, r.top + 52, "↓  " + Metrics::Bytes(fData.down, true), green, 20, true);
    Text(half + 8, r.top + 52, "↑  " + Metrics::Bytes(fData.up, true), cyan, 20, true);
    double max = std::max(1024.0, fNetworkScale.At(fFrameTime));
    for (const auto& p : Animation::Graph(fData.downHistory, fGraphTime)) max = std::max(max, p.value);
    for (const auto& p : Animation::Graph(fData.upHistory, fGraphTime)) max = std::max(max, p.value);
    Graph(BRect(r.left + 16, r.top + 65, half - 10, r.top + 101), fData.downHistory, max, green);
    Graph(BRect(half + 8, r.top + 65, r.right - 16, r.top + 101), fData.upHistory, max, cyan);
    if (fData.networks.empty()) { Text(r.left + 16, r.bottom - 20, "No network interfaces available", muted, 11); return; }
    fNetIndex %= fData.networks.size(); const auto& n = fData.networks[fNetIndex];
    FitText(r.left + 16, r.top + 124, r.Width() - 115, n.name + "  •  " + n.address + (n.link ? "  •  connected" : "  •  disconnected"), muted, 10);
    Right(r.right - 16, r.top + 124, std::to_string(fNetIndex + 1) + "/" + std::to_string(fData.networks.size()) + "  click ›", muted, 9);
    Text(r.left + 16, r.bottom - 13, "RX " + Metrics::Bytes(n.received) + "   TX " + Metrics::Bytes(n.sent) + "   Errors " + std::to_string(n.errors), muted, 10);
}
void Dashboard::DrawHardware() {
    BRect r = fHardware; Panel(r, "THERMALS & GPU", "HARDWARE", amber);
    Text(r.left + 16, r.top + 58, fData.hasTemperature ? Metrics::Number(fTemperatureValue.At(fFrameTime)) + " °C" : "— °C", amber, 25, true);
    float x = r.left + r.Width() * .43f;
    FitText(x, r.top + 49, r.right - x - 14, fData.hasTemperature ? (fData.sensor == "AMD 1950X Tdie / SMN" ? "CPU die temperature" : "Hottest thermal sensor") : "CPU sensor unavailable", muted, 10);
    FitText(x, r.top + 66, r.right - x - 14, fData.hasTemperature ? fData.sensor : "No thermal device published", muted, 9);
    fPainter->SetHighColor(border); fPainter->StrokeLine(BPoint(r.left + 16, r.top + 81), BPoint(r.right - 16, r.top + 81));
    FitText(r.left + 16, r.top + 106, r.Width() - 32, fGpu, ink, 11, true);
    Text(r.left + 16, r.top + 128, "GPU load  —", purple, 14);
    FitText(r.left + 16, r.bottom - 13, r.Width() - 32, "Utilization is not exposed by the display API", muted, 10);
}
void Dashboard::DrawDisks() {
    BRect r = fDisks; Panel(r, "VOLUMES", std::to_string(fData.disks.size()) + " MOUNTED", amber);
    int capacity = std::max(1, int((r.Height() - 70) / 83));
    fDiskOffset = std::max(0, std::min(fDiskOffset, int(fData.disks.size()) - capacity));
    for (int row = 0; row < capacity && row + fDiskOffset < int(fData.disks.size()); ++row) {
        const auto& d = fData.disks[row + fDiskOffset]; float y = r.top + 54 + row * 83;
        double used = Metrics::Percent(d.capacity - d.free, d.capacity);
        FitText(r.left + 16, y, r.Width() - 82, d.name, ink, 12, true); Right(r.right - 16, y, Metrics::Number(used, 0) + "%", amber, 12);
        FitText(r.left + 16, y + 17, r.Width() - 32, d.path + "  •  " + d.fs + (d.readOnly ? "  •  read only" : ""), muted, 9);
        Bar(BRect(r.left + 16, y + 27, r.right - 16, y + 32), used, used > 90 ? pink : amber);
        Text(r.left + 16, y + 49, Metrics::Bytes(d.free) + " free / " + Metrics::Bytes(d.capacity), muted, 10);
    }
    if (fData.disks.empty()) Text(r.left + 16, r.top + 57, "No mounted storage volumes", muted, 11);
    FitText(r.left + 16, r.bottom - 13, r.Width() - 32, "Capacity  •  5s refresh  •  scroll for more", muted, 9);
}
void Dashboard::Sort() {
    fRows.clear(); std::string query = Lower(fFilter);
    for (const auto& p : fData.processes)
        if (query.empty() || Lower(p.name + " " + p.command + " " + std::to_string(p.id)).find(query) != std::string::npos) fRows.push_back(p);
    std::sort(fRows.begin(), fRows.end(), [this](const Process& a, const Process& b) {
        int order = 0;
        switch (fSort) {
            case 0: order = a.cpu < b.cpu ? -1 : a.cpu > b.cpu; break;
            case 1: order = a.resident < b.resident ? -1 : a.resident > b.resident; break;
            case 2: order = a.id < b.id ? -1 : a.id > b.id; break;
            case 3: order = a.name.compare(b.name); break;
            case 4: order = a.threads < b.threads ? -1 : a.threads > b.threads; break;
        }
        if (order == 0) return a.id < b.id;
        return fDescending ? order > 0 : order < 0;
    });
    int capacity = std::max(1, int((fTable.Height() - 27) / 23));
    fOffset = std::max(0, std::min(fOffset, int(fRows.size()) - capacity));
}
void Dashboard::DrawProcesses() {
    BRect r = fProcesses; Panel(r, "PROCESSES", std::to_string(fRows.size()) + " / " + std::to_string(fData.processes.size()) + " TEAMS", cyan);
    fPainter->SetHighColor(bg); fPainter->FillRoundRect(fSearch, 5, 5); fPainter->SetHighColor(fFiltering ? cyan : border); fPainter->StrokeRoundRect(fSearch, 5, 5);
    FitText(fSearch.left + 10, fSearch.top + 20, fSearch.Width() - 20, fFilter.empty() && !fFiltering ? "/  Filter by name, command or team ID" : "/  " + fFilter + (fFiltering ? "|" : ""), fFiltering ? ink : muted, 11);
    const float left = fTable.left, right = fTable.right;
    fPainter->SetHighColor(faint); fPainter->FillRoundRect(BRect(left, fTable.top, right, fTable.top + 23), 3, 3);
    auto label = [&](int id, const char* name) { return std::string(name) + (fSort == id ? (fDescending ? " ↓" : " ↑") : ""); };
    Text(left + 7, fTable.top + 16, label(2, "PID"), fSort == 2 ? cyan : muted, 10);
    Text(left + 68, fTable.top + 16, label(3, "NAME"), fSort == 3 ? cyan : muted, 10);
    Right(right - 186, fTable.top + 16, label(4, "THR"), fSort == 4 ? cyan : muted, 10);
    Right(right - 99, fTable.top + 16, label(1, "MEM"), fSort == 1 ? cyan : muted, 10);
    Right(right - 9, fTable.top + 16, label(0, "CPU %"), fSort == 0 ? cyan : muted, 10);
    int capacity = std::max(1, int((fTable.Height() - 27) / 23));
    for (int i = 0; i < capacity && i + fOffset < int(fRows.size()); ++i) {
        const auto& p = fRows[i + fOffset]; float y = fTable.top + 28 + i * 23;
        if (p.id == fSelected || i % 2 == 0) { fPainter->SetHighColor(p.id == fSelected ? rgb_color{34, 65, 80, 255} : rgb_color{22, 32, 46, 255}); fPainter->FillRect(BRect(left, y - 2, right, y + 19)); }
        Text(left + 7, y + 13, std::to_string(p.id), muted, 10);
        FitText(left + 68, y + 13, fTable.Width() - 278, p.name, ink, 11);
        Right(right - 186, y + 13, std::to_string(p.threads), muted, 10);
        Right(right - 99, y + 13, Metrics::Bytes(p.resident), purple, 10);
        Right(right - 9, y + 13, Metrics::Number(p.cpu), p.cpu >= 50 ? amber : cyan, 11);
    }
    if (fRows.empty()) Text(left + 10, fTable.top + 51, fData.valid ? "No matching processes" : "Reading system counters…", muted, 12);
    FitText(r.left + 16, r.bottom - 11, r.Width() - 32, "CPU: 100% = one core   •   MEM: resident areas, includes shared pages   •   " + std::to_string(fRows.empty() ? 0 : fOffset + 1) + "–" + std::to_string(std::min(int(fRows.size()), fOffset + capacity)), muted, 9);
}
void Dashboard::GetState(BMessage& state) const {
    state.AddBool("focused", IsFocus()); state.AddBool("paused", fPaused); state.AddBool("filtering", fFiltering);
    state.AddString("filter", fFilter.c_str()); state.AddInt32("sort", fSort);
    state.AddBool("descending", fDescending); state.AddInt32("rows", fRows.size());
    state.AddInt32("offset", fOffset); state.AddInt32("selected", fSelected);
    state.AddInt32("disk_offset", fDiskOffset); state.AddInt32("network_index", fNetIndex);
    state.AddInt64("frames", fFrames); state.AddInt64("graph_time", fGraphTime);
    state.AddInt64("process_time", fData.processTime); state.AddInt64("sample_time", fData.time);
    state.AddDouble("cpu_display", fCPUValue.At(fFrameTime)); state.AddDouble("cpu_sample", fData.cpu);
    state.AddDouble("draw_ms", fDrawMs); state.AddDouble("sample_ms", fData.sampleMs);
    state.AddBool("buffered", fBuffer != nullptr);
    state.AddDouble("render_scale", fBufferScale);
    state.AddInt32("buffer_width", fBuffer ? fBuffer->Bounds().IntegerWidth() + 1 : 0);
    state.AddInt32("buffer_height", fBuffer ? fBuffer->Bounds().IntegerHeight() + 1 : 0);
    state.AddInt32("view_width", Bounds().IntegerWidth() + 1);
    state.AddInt32("view_height", Bounds().IntegerHeight() + 1);
    state.AddInt64("sequence", fData.sequence); state.AddInt64("interval", fSampler.Interval());
}
void Dashboard::TogglePause() { fPaused = !fPaused; if (!fPaused) Pulse(); Invalidate(); }
void Dashboard::CycleSpeed() {
    auto value = fSampler.Interval(); fSampler.SetInterval(value == 250000 ? 500000 : value == 500000 ? 1000000 : value == 1000000 ? 2000000 : 250000); Invalidate();
}
void Dashboard::Help() {
    BAlert* alert = new BAlert("AirTop", "AirTop · System observatory\n\nSpace: pause / resume the display\n/: filter processes; Escape: clear filter\nC / M / I / N / T: sort CPU / memory / PID / name / threads\nRepeat a sort key or click a column to reverse it\nArrow keys / Page Up / Page Down: browse processes\nEnter or double-click: process details\n+ or -: cycle 0.25 / 0.5 / 1 / 2 second sampling\nClick Network: cycle interface details\nScroll over Volumes: browse mounted storage\n\nCPU totals are normalized across enabled cores. A process can exceed 100% when it uses multiple cores. Resident areas can include shared pages. Memory in use is total minus the kernel's available memory.\n\nThermal sensors are discovered automatically. Missing temperature and GPU telemetry are shown as unavailable. No values are simulated.", "Got it", nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
    alert->SetShortcut(0, B_ESCAPE); alert->Go(nullptr);
}
void Dashboard::Details() {
    auto found = std::find_if(fRows.begin(), fRows.end(), [this](const Process& p) { return p.id == fSelected; });
    if (found == fRows.end()) return;
    const auto& p = *found;
    std::string details = p.name + "\n\nTeam ID: " + std::to_string(p.id) + "\nCPU: " + Metrics::Number(p.cpu) + "% (100% per core)\nResident areas: " + Metrics::Bytes(p.resident) + "\nVirtual areas: " + Metrics::Bytes(p.virtualSize) + "\nThreads: " + std::to_string(p.threads) + "\nMain thread priority: " + std::to_string(p.priority) + "\nUser ID: " + std::to_string(p.uid) + "\n\nCommand (as reported by Haiku):\n" + p.command;
    BAlert* alert = new BAlert("Process details", details.c_str(), "Close", nullptr, nullptr, B_WIDTH_AS_USUAL, B_INFO_ALERT);
    alert->SetShortcut(0, B_ESCAPE); alert->Go(nullptr);
}
void Dashboard::KeyDown(const char* bytes, int32 count) {
    if (count <= 0) return;
    unsigned char key = bytes[0];
    int32 rawKey = 0;
    if (Window()->CurrentMessage()->FindInt32("key", &rawKey) == B_OK && rawKey == B_F1_KEY) { Help(); return; }
    if (key == B_ESCAPE) { fFiltering = false; fFilter.clear(); fOffset = 0; Sort(); Invalidate(); return; }
    if (fFiltering) {
        if (key == B_ENTER) fFiltering = false;
        else if (key == B_BACKSPACE) { if (!fFilter.empty()) { size_t start = fFilter.size() - 1; while (start > 0 && ((unsigned char)fFilter[start] & 0xc0) == 0x80) --start; fFilter.erase(start); } }
        else if (key >= 32 && key != B_DELETE && fFilter.size() < 100) fFilter.append(bytes, count);
        fOffset = 0; Sort(); Invalidate(); return;
    }
    if (key == '/') { fFiltering = true; Invalidate(); return; }
    if (key == ' ') { TogglePause(); return; }
    if (key == '+' || key == '-') { CycleSpeed(); return; }
    if (key == B_ENTER) { Details(); return; }
    int sort = -1;
    switch (std::tolower(key)) { case 'c': sort = 0; break; case 'm': sort = 1; break; case 'i': sort = 2; break; case 'n': sort = 3; break; case 't': sort = 4; break; }
    if (sort >= 0) { fDescending = sort == fSort ? !fDescending : sort != 3; fSort = sort; fOffset = 0; Sort(); Invalidate(); return; }
    if (key == B_UP_ARROW || key == B_DOWN_ARROW || key == B_PAGE_UP || key == B_PAGE_DOWN || key == B_HOME || key == B_END) {
        int index = 0;
        for (size_t i = 0; i < fRows.size(); ++i) if (fRows[i].id == fSelected) index = i;
        int capacity = std::max(1, int((fTable.Height() - 27) / 23));
        if (key == B_UP_ARROW) --index;
        if (key == B_DOWN_ARROW) ++index;
        if (key == B_PAGE_UP) index -= capacity;
        if (key == B_PAGE_DOWN) index += capacity;
        if (key == B_HOME) index = 0;
        if (key == B_END) index = fRows.size() - 1;
        index = std::max(0, std::min(index, int(fRows.size()) - 1));
        if (!fRows.empty()) fSelected = fRows[index].id;
        if (index < fOffset) fOffset = index;
        if (index >= fOffset + capacity) fOffset = index - capacity + 1;
        Invalidate(); return;
    }
    BView::KeyDown(bytes, count);
}
void Dashboard::MouseDown(BPoint point) {
    MakeFocus();
    if (fPauseButton.Contains(point)) { TogglePause(); return; }
    if (fSpeedButton.Contains(point)) { CycleSpeed(); return; }
    if (fSearch.Contains(point)) { fFiltering = true; Invalidate(); return; }
    fFiltering = false;
    if (fNetwork.Contains(point)) { ++fNetIndex; Invalidate(); return; }
    if (fTable.Contains(point)) {
        if (point.y <= fTable.top + 23) {
            int sort = point.x < fTable.left + 62 ? 2 : point.x < fTable.right - 232 ? 3 : point.x < fTable.right - 157 ? 4 : point.x < fTable.right - 77 ? 1 : 0;
            fDescending = sort == fSort ? !fDescending : sort != 3; fSort = sort; fOffset = 0; Sort();
        } else {
            int row = int((point.y - fTable.top - 26) / 23) + fOffset;
            if (row >= 0 && row < int(fRows.size())) {
                fSelected = fRows[row].id; int32 clicks = 1; Window()->CurrentMessage()->FindInt32("clicks", &clicks); if (clicks == 2) Details();
            }
        }
    }
    Invalidate();
}
void Dashboard::MessageReceived(BMessage* message) {
    if (message->what == B_MOUSE_WHEEL_CHANGED) {
        float delta = 0; message->FindFloat("be:wheel_delta_y", &delta);
        BPoint point; uint32 buttons; GetMouse(&point, &buttons, false);
        if (fDisks.Contains(point)) fDiskOffset = std::max(0, fDiskOffset + (delta > 0 ? 1 : -1));
        else { fOffset += int(delta * 3); Sort(); }
        Invalidate(); return;
    }
    if (message->what == B_UNMAPPED_KEY_DOWN) {
        int32 key; if (message->FindInt32("key", &key) == B_OK && key == B_F1_KEY) { Help(); return; }
    }
    BView::MessageReceived(message);
}
MonitorWindow::MonitorWindow(Sampler& sampler, AuxPanel* panel, bool quitsApp)
    : BWindow(BRect(80, 60, 1300, 950), "AirTop", B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS), fSampler(sampler), fAuxPanel(panel), fQuitsApp(quitsApp) {
    fSampler.SetProcessDetailsEnabled(true);
    SetSizeLimits(1000, 2400, 790, 1800);
    BMenuBar* bar = new BMenuBar(BRect(0, 0, Bounds().Width(), 20), "menu");
    BMenu* menu = new BMenu("AirTop"); menu->AddItem(new BMenuItem("About AirTop…", new BMessage(B_ABOUT_REQUESTED))); menu->AddSeparatorItem(); menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q')); bar->AddItem(menu);
    BMenu* view = new BMenu("View"); view->AddItem(new BMenuItem("Pause / Resume", new BMessage(kPause), 'P')); view->AddItem(new BMenuItem("Change refresh interval", new BMessage(kSpeed), 'R')); view->AddItem(new BMenuItem("Keyboard shortcuts…", new BMessage(kHelp)));
    view->AddSeparatorItem(); fAuxItem = new BMenuItem("Show on auxiliary display", new BMessage(kAuxPanel)); view->AddItem(fAuxItem); bar->AddItem(view); AddChild(bar);
    BRect frame = Bounds(); frame.top = bar->Frame().bottom + 1;
    fDashboard = new Dashboard(frame, fSampler); AddChild(fDashboard);
    BMessage settings; BFile file(SettingsPath().Path(), B_READ_ONLY); BRect saved;
    if (settings.Unflatten(&file) == B_OK) {
        if (settings.FindRect("frame", &saved) == B_OK && saved.Width() >= 1000 && saved.Height() >= 790 && BScreen(this).Frame().Contains(saved)) { MoveTo(saved.LeftTop()); ResizeTo(saved.Width(), saved.Height()); }
        int64 interval; if (settings.FindInt64("interval", &interval) == B_OK && (interval == 500000 || interval == 1000000 || interval == 2000000 || interval == 250000)) fSampler.SetInterval(interval);
    }
    SetPulseRate(33333); fSampler.Start(); Show();
}
MonitorWindow::~MonitorWindow() {}
void MonitorWindow::MenusBeginning() {
    fAuxItem->SetEnabled(fAuxPanel != nullptr && (fAuxPanel->IsRunning() || BAuxDisplay::CountDisplays() > 0));
    fAuxItem->SetMarked(fAuxPanel != nullptr && fAuxPanel->IsRunning());
}
bool MonitorWindow::QuitRequested() {
    fSampler.SetProcessDetailsEnabled(false);
    BMessage settings; settings.AddRect("frame", Frame()); settings.AddInt64("interval", fSampler.Interval());
    BFile file(SettingsPath().Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE); settings.Flatten(&file);
    if (fQuitsApp) be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}
BHandler* MonitorWindow::ResolveSpecifier(BMessage* message, int32 index, BMessage* specifier, int32 form, const char* property) {
    if (strcmp(property, "MonitorState") == 0) return this;
    return BWindow::ResolveSpecifier(message, index, specifier, form, property);
}
status_t MonitorWindow::GetSupportedSuites(BMessage* message) {
    static property_info properties[] = {
        {"MonitorState", {B_GET_PROPERTY, 0}, {B_DIRECT_SPECIFIER, 0}, "Read current monitor display state", 0, {B_MESSAGE_TYPE}, {}, {}},
        {nullptr, {0}, {0}, nullptr, 0, {0}, {}, {}}
    };
    BPropertyInfo info(properties); message->AddString("suites", "suite/vnd.AirOS-AirTop"); message->AddFlat("messages", &info);
    return BWindow::GetSupportedSuites(message);
}
void MonitorWindow::MessageReceived(BMessage* message) {
    if (message->what == B_GET_PROPERTY) {
        BMessage specifier; const char* property; int32 index, form;
        if (message->GetCurrentSpecifier(&index, &specifier, &form, &property) == B_OK && strcmp(property, "MonitorState") == 0) {
            BMessage reply(B_REPLY), state; fDashboard->GetState(state);
            reply.AddMessage("result", &state); reply.AddInt32("error", B_OK); message->SendReply(&reply); return;
        }
    }
    switch (message->what) {
        case kShowMonitor: Activate(); break;
        case kPause: fDashboard->TogglePause(); break;
        case kSpeed: fDashboard->CycleSpeed(); break;
        case kHelp: case B_ABOUT_REQUESTED: fDashboard->Help(); break;
        case kAuxPanel:
            if (fAuxPanel == nullptr) break;
            if (fAuxPanel->IsRunning()) fAuxPanel->Stop(); else fAuxPanel->Start();
            fAuxItem->SetMarked(fAuxPanel->IsRunning());
            break;
        default: BWindow::MessageReceived(message);
    }
}
