// Cached and direct panel rendering must agree at every tested scale.
// The header clock is cleared in both images because it can tick between draws.
#include <Application.h>
#include <Bitmap.h>
#include <View.h>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/Sampler.h"
#define private public
#include "../src/AuxPanel.h"
#undef private
#include "../src/Theme.h"

static void require(bool value,const char* message) {
    if (!value) { fprintf(stderr,"PANEL_CACHE_FAIL %s\n",message);exit(1); }
}
static void seed(AuxPanel& panel, int scene) {
    Snapshot& s = panel.fData; s = Snapshot{};
    panel.fFrameTime = 200000000; panel.fGraphTime = 199750000;
    s.time = 200000000; s.valid = scene != 0; s.cpuValid = scene != 0;
    s.host = scene == 0 ? "" : "raspberrypi";
    s.error = scene == 0 ? "waiting for the first sample" : "";
    s.cpuName = "ARM Cortex-A72 1.8 GHz — deterministic panel fixture";
    s.cpu = scene * 21.75; s.temperature = scene * 18.3;
    s.hasTemperature = scene >= 2; s.sensor = "/dev/power/rpi_firmware/0";
    s.frequency = 1800000000; s.threads = 127 + scene; s.teams = 31 + scene;
    s.totalMemory = 4ULL << 30; s.availableMemory = (4 - scene % 5) * (1ULL << 29);
    s.down = scene * 98317; s.up = scene * 7823;
    size_t cores = scene == 0 ? 0 : scene == 4 ? 16 : scene == 5 ? 9 : 4;
    panel.fCPUValue.Target(s.cpu, panel.fFrameTime, 0);
    panel.fTemperatureValue.Target(s.temperature, panel.fFrameTime, 0);
    panel.fNetworkScale.Target(524288, panel.fFrameTime, 0);
    s.cores.resize(cores); panel.fCoreValues.resize(cores);
    for (size_t i=0; i<cores; ++i) {
        s.cores[i] = (i * 23 + scene * 31) % 101;
        panel.fCoreValues[i].Target(s.cores[i], panel.fFrameTime, 0);
    }
    if (scene > 0) {
        for (int i=0; i<500; ++i) {
            bigtime_t t=75000000 + i * 250000;
            s.cpuHistory.push_back({t,double((i * 13 + scene * 7) % 101)});
            s.downHistory.push_back({t,double((i * 4837 + scene * 1000) % 500000)});
            s.upHistory.push_back({t,double((i * 117 + scene * 1500) % 20000)});
        }
        for (int i=0; i<3; ++i) { Network n; n.name=i==1 ? "usb_rndis_very_long_interface_name" : "ethernet";n.address="192.168.1.213";n.link=i!=2;s.networks.push_back(n); }
    }
}

static void draw(AuxPanel& panel) {
    panel.Render(panel.fFrameTime);
    panel.fView->SetHighColor(Theme::bg);
    panel.fView->FillRect(BRect(0,0,640,40));
    panel.fView->Sync();
}

int main() {
    BApplication app("application/x-vnd.AirOS-panel-cache-test");
    Sampler sampler;
    const int sizes[][2]={{640,480},{320,240},{960,720},{1001,751},{800,480},{640,560}};
    const char* skips[]={nullptr,"all","header","cpu","net","temp","present",nullptr};
    int cases=0;
    for (const auto& size : sizes) {
        BRect bounds(0,0,size[0]-1,size[1]-1);
        BBitmap direct(bounds,B_BITMAP_ACCEPTS_VIEWS,B_RGB32);
        BBitmap cached(bounds,B_BITMAP_ACCEPTS_VIEWS,B_RGB32);
        require(direct.InitCheck()==B_OK && cached.InitCheck()==B_OK,"bitmaps");
        require(direct.Lock() && cached.Lock(),"bitmap locks");
        memset(direct.Bits(),0xa5,direct.BitsLength());memset(cached.Bits(),0xa5,cached.BitsLength());
        AuxPanel a(sampler),b(sampler);
        a.fBitmap=&direct;b.fBitmap=&cached;
        a.fView=new BView(bounds,"direct",B_FOLLOW_NONE,0);
        b.fView=new BView(bounds,"cached",B_FOLLOW_NONE,0);
        direct.AddChild(a.fView);cached.AddChild(b.fView);
        a.fScale=b.fScale=std::min(size[0]/640.f,size[1]/480.f);
        // This is the same direct path used after a failed cache allocation.
        a.fBackgroundAttempted=true;
        for (const char* skip : skips) {
            if (skip) setenv("AIRTOP_PANEL_SKIP",skip,1);else unsetenv("AIRTOP_PANEL_SKIP");
            for (int scene=0;scene<6;++scene) {
                seed(a,scene);seed(b,scene);draw(a);draw(b);
                require(direct.BitsLength()==cached.BitsLength(),"equal bitmap lengths");
                if (memcmp(direct.Bits(),cached.Bits(),direct.BitsLength())!=0) {
                    fprintf(stderr,"difference at %dx%d scene=%d skip=%s\n",size[0],size[1],scene,skip?skip:"none");
                    require(false,"pixel equality");
                }
                require(!a.fBackground,"direct fallback does not allocate a cache");
                if (!skip) {
                    require(b.fBackground!=nullptr,"background created");
                    require(!(b.fBackground->Flags()&B_BITMAP_ACCEPTS_VIEWS),"cache needs no drawing window");
                }
                ++cases;
            }
        }
        cached.Unlock();direct.Unlock();
    }
    unsetenv("AIRTOP_PANEL_SKIP");
    printf("PANEL_CACHE_PASS cases=%d: exact pixels, scaling, letterboxing, changing data, fallback and debug modes\n",cases);
    return 0;
}
