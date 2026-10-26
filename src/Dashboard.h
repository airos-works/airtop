#pragma once
#include "Sampler.h"
#include <View.h>
#include <Bitmap.h>
#include "Animation.h"
#include <Window.h>
#include <string>

constexpr uint32 kPause = 'paus', kRefresh = 'rfre', kSpeed = 'sped', kHelp = 'help', kAuxPanel = 'auxp', kShowMonitor = 'shwm';
class AuxPanel;
class Dashboard : public BView {
public:
    Dashboard(BRect frame, Sampler& sampler);
    ~Dashboard() override;
    void AttachedToWindow() override;
    void Draw(BRect update) override;
    void Pulse() override;
    void KeyDown(const char* bytes, int32 count) override;
    void MouseDown(BPoint point) override;
    void MessageReceived(BMessage* message) override;
    void FrameResized(float width, float height) override;
    void TogglePause(); void CycleSpeed(); void Help();
    void GetState(BMessage& state) const;
private:
    Sampler& fSampler; Snapshot fData;
    BBitmap* fBuffer = nullptr;
    BView* fPainter = this;
    BView* fBufferView = nullptr;
    float fBufferScale = 1;
    bigtime_t fScaleCheckTime = 0;
    bigtime_t fFrameTime = 0, fGraphTime = 0;
    uint64 fFrames = 0;
    double fDrawMs = 0;
    Animation::Value fCPUValue, fMemoryValue, fSwapValue, fTemperatureValue, fNetworkScale;
    std::vector<Animation::Value> fCoreValues;
    void Render(BRect update);
    void UpdateDisplayScale();
    void Retarget(bigtime_t now);
    bool fPaused = false, fFiltering = false, fDescending = true;
    int fSort = 0, fOffset = 0, fDiskOffset = 0, fNetIndex = 0;
    team_id fSelected = -1;
    std::string fFilter, fGpu;
    std::vector<Process> fRows;
    BRect fCPU, fMemory, fNetwork, fDisks, fHardware, fProcesses, fPauseButton, fSpeedButton, fSearch, fTable;
    void Layout(); void Sort(); void Details();
    void Text(float x, float y, const std::string& text, rgb_color color, float size = 12, bool bold = false);
    void Right(float x, float y, const std::string& text, rgb_color color, float size = 12);
    void FitText(float x, float y, float width, const std::string& text, rgb_color color, float size = 12, bool bold = false);
    void Panel(BRect rect, const std::string& title, const std::string& meta, rgb_color color);
    void Bar(BRect rect, double percent, rgb_color color, bool segmented = false);
    void Graph(BRect rect, const std::deque<Animation::Sample>& history, double maximum, rgb_color color, bool grid = true);
    void DrawCPU(); void DrawMemory(); void DrawNetwork(); void DrawDisks(); void DrawHardware(); void DrawProcesses();
};
// The application owns the sampler and the auxiliary display panel; the
// window is one view of the data and may come and go (AirTop --panel).
class MonitorWindow : public BWindow {
public:
    MonitorWindow(Sampler& sampler, AuxPanel* panel, bool quitsApp); ~MonitorWindow();
    bool QuitRequested() override;
    BHandler* ResolveSpecifier(BMessage*, int32, BMessage*, int32, const char*) override;
    status_t GetSupportedSuites(BMessage*) override;
    void MessageReceived(BMessage* message) override;
    void MenusBeginning() override;
private:
    Sampler& fSampler; Dashboard* fDashboard;
    AuxPanel* fAuxPanel; BMenuItem* fAuxItem = nullptr; bool fQuitsApp;
};
