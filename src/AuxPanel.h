#pragma once
#include "Sampler.h"
#include <AuxDisplay.h>
#include <atomic>
#include <thread>
#include <memory>

class BBitmap;
class BView;

// AirTop on an auxiliary display (BAuxDisplay, Device Kit): the CPU graph
// with the core loads, the network graph and the temperatures, drawn into
// the display's bitmap by a thread of its own at a modest frame rate. The
// window is not needed; `AirTop --panel` runs only this.
class AuxPanel {
public:
    explicit AuxPanel(Sampler& sampler);
    ~AuxPanel();

    // Opens the first auxiliary display; false when there is none.
    bool Start();
    void Stop();
    bool IsRunning() const { return fRunning; }
    const char* DisplayName() const { return fDisplay.Name(); }
    int32 Width() const { return fDisplay.Width(); }
    int32 Height() const { return fDisplay.Height(); }
    uint64 Frames() const { return fFrames; }

private:
    void Run();
    void Render(bigtime_t now);
    void PrepareBackground();
    void PanelFrame(BRect rect, rgb_color color);
    void Text(float x, float y, const std::string& text, rgb_color color, float size = 12, bool bold = false);
    void Right(float x, float y, const std::string& text, rgb_color color, float size = 12, bool bold = false);
    void Panel(BRect rect, const std::string& title, const std::string& meta, rgb_color color);
    void Bar(BRect rect, double percent, rgb_color color);
    void Graph(BRect rect, const std::deque<Animation::Sample>& history, double maximum, rgb_color color);
    void DrawHeader(BRect r);
    void DrawCPU(BRect r);
    void DrawNetwork(BRect r);
    void DrawTemperatures(BRect r);

    Sampler& fSampler;
    BAuxDisplay fDisplay;
    BBitmap* fBitmap = nullptr;
    BView* fView = nullptr;
    std::unique_ptr<BBitmap> fBackground;
    bool fBackgroundAttempted = false;
    bool fBackgroundDrawn = false;
    Snapshot fData;
    std::thread fThread;
    std::atomic<bool> fStop{false};
    std::atomic<bool> fRunning{false};
    std::atomic<uint64> fFrames{0};
    bigtime_t fFrameTime = 0, fGraphTime = 0;
    Animation::Value fCPUValue, fTemperatureValue, fNetworkScale;
    std::vector<Animation::Value> fCoreValues;
    float fScale = 1;   // 640x480 is the design size
};
