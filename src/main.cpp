#include "Dashboard.h"
#include "AuxPanel.h"
#include <Application.h>
#include <Messenger.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

// The application owns the sampler and the auxiliary display panel (a DSI
// panel on a Raspberry Pi, a USB LCD). Started plainly it opens its window
// and, where there is an auxiliary display, the panel. `AirTop --panel`
// runs the panel alone, as a service; launching AirTop again then opens the
// window (single launch), and closing that window keeps the panel going.
class AirTop : public BApplication {
public:
    AirTop(bool panelOnly) : BApplication("application/x-vnd.AirOS-AirTop"), fPanelOnly(panelOnly), fSampler(false) {}
    ~AirTop() override { delete fPanel; fSampler.Stop(); }
    void ReadyToRun() override {
        fSampler.Start();
        fPanel = new AuxPanel(fSampler);
        bool panel = fPanel->Start();
        if (!fPanelOnly) { ShowWindow(); return; }
        if (!panel) { fprintf(stderr, "AirTop: no auxiliary display\n"); PostMessage(B_QUIT_REQUESTED); return; }
        printf("AirTop: %s, %ldx%ld\n", fPanel->DisplayName(), (long)fPanel->Width(), (long)fPanel->Height());
    }
    // The launch's own arguments arrive here too: only a plain relaunch
    // (no arguments, or none we know) opens the window beside the panel.
    void ArgvReceived(int32 argc, char** argv) override {
        for (int32 i = 1; i < argc; ++i) if (strcmp(argv[i], "--panel") == 0 || strcmp(argv[i], "--sample") == 0) return;
        ShowWindow();
    }
    void RefsReceived(BMessage*) override { ShowWindow(); }
    void MessageReceived(BMessage* message) override {
        if (message->what == B_SILENT_RELAUNCH) ShowWindow();
        else BApplication::MessageReceived(message);
    }
    void ShowWindow() {
        if (fWindow.IsValid() && fWindow.SendMessage(kShowMonitor) == B_OK) return;
        fWindow = BMessenger(new MonitorWindow(fSampler, fPanel, !fPanelOnly));
    }
private:
    bool fPanelOnly; Sampler fSampler; AuxPanel* fPanel = nullptr; BMessenger fWindow;
};
static void OnSignal(int) { if (be_app) be_app->PostMessage(B_QUIT_REQUESTED); }
int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--sample") == 0) {
        int count = argc > 2 ? atoi(argv[2]) : 2;
        if (count < 1 || count > 3600) { fprintf(stderr, "Sample count must be 1–3600\n"); return 2; }
        Sampler sampler;
        for (int i = 0; i < count; ++i) { auto snapshot = sampler.Sample(); PrintSnapshot(snapshot); if (!snapshot.valid) return 1; if (i + 1 < count) snooze(1000000); }
        return 0;
    }
    bool panelOnly = argc > 1 && strcmp(argv[1], "--panel") == 0;
    if (argc > 1 && !panelOnly) { printf("AirTop 1.2.0 — native Haiku system monitor\nUsage: AirTop [--sample N | --panel]\n  --panel  only the auxiliary display (DSI panel, USB LCD), no window\n"); return strcmp(argv[1], "--help") ? 2 : 0; }
    signal(SIGINT, OnSignal); signal(SIGTERM, OnSignal);
    AirTop app(panelOnly); app.Run(); return 0;
}
