#include "../src/DisplayScale.h"
#include <Application.h>
#include <Bitmap.h>
#include <Region.h>
#include <Screen.h>
#include <View.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static void Draw(BBitmap& bitmap, float scale) {
    BView* view = new BView(bitmap.Bounds(), "test", B_FOLLOW_NONE, B_WILL_DRAW);
    bitmap.AddChild(view);
    assert(bitmap.Lock());
    view->SetHighColor(12, 18, 28); view->FillRect(bitmap.Bounds());
    view->SetScale(scale);
    BFont font(be_plain_font); font.SetSize(24 / scale);
    view->SetFont(&font); view->SetHighColor(222, 233, 242);
    view->DrawString("Native 2x", BPoint(10 / scale, 40 / scale));
    view->Sync(); bitmap.Unlock();
}

int main() {
    BApplication app("application/x-vnd.AirOS-AirTop-rendering-test");
    assert(DisplayScale::FromDimensions(7680, 2160, 3840, 1080) == 2);
    assert(DisplayScale::FromDimensions(3840, 2160, 3840, 2160) == 1);
    assert(DisplayScale::FromDimensions(2880, 1620, 1920, 1080) == 1.5f);
    assert(DisplayScale::FromDimensions(7680, 2160, 3840, 2160) == 1);
    assert(DisplayScale::FromDimensions(0, 0, 0, 0) == 1);
    BRect logical(0, 0, 1000, 768);
    assert(DisplayScale::Pixels(logical, 1) == logical);
    assert(DisplayScale::Pixels(logical, 2) == BRect(0, 0, 2001, 1537));
    assert(DisplayScale::Pixels(BRect(7, 11, 9, 15), 2) == BRect(14, 22, 19, 31));
    assert(DisplayScale::Pixels(BRect(0, 0, 100, 10), 1.5f) == BRect(0, 0, 151, 16));

    // Drawing a logical 12px font at 2x must rasterize the same glyphs as a
    // native 24px font, not enlarge a previously rasterized 12px bitmap.
    BBitmap scaled(BRect(0, 0, 239, 79), B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
    BBitmap native(scaled.Bounds(), B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
    assert(scaled.InitCheck() == B_OK && native.InitCheck() == B_OK);
    Draw(scaled, 2); Draw(native, 1);
    assert(scaled.BitsLength() == native.BitsLength());
    assert(memcmp(scaled.Bits(), native.Bits(), native.BitsLength()) == 0);

    // A device-pixel clip in the parent state must stay fixed when the child
    // scales drawing. This is the dashboard's partial repaint path.
    assert(scaled.Lock());
    BView* view = scaled.ChildAt(0);
    view->SetScale(1); view->PushState();
    BRegion clip(BRect(14, 22, 19, 31)); view->ConstrainClippingRegion(&clip);
    view->PushState(); view->SetScale(2); view->SetHighColor(255, 0, 0);
    view->FillRect(BRect(0, 0, 100, 35));
    view->PopState(); view->PopState(); view->Sync(); scaled.Unlock();
    const uint8* bits = static_cast<const uint8*>(scaled.Bits());
    int red = 0;
    for (int y = 0; y < 80; ++y) for (int x = 0; x < 240; ++x) {
        const uint8* p = bits + y * scaled.BytesPerRow() + x * 4;
        if (p[0] == 0 && p[1] == 0 && p[2] == 255) {
            assert(x >= 14 && x <= 19 && y >= 22 && y <= 31); ++red;
        }
    }
    assert(red == 60);
    BScreen screen; display_mode mode{}; assert(screen.GetMode(&mode) == B_OK);
    BRect frame = screen.Frame();
    printf("Rendering: density, inclusive bounds, native glyph rasterization and partial repaint clipping passed\n");
    printf("Display: %ux%u device, %.0fx%.0f logical, %.2fx density\n",
        mode.virtual_width, mode.virtual_height, frame.Width() + 1, frame.Height() + 1,
        DisplayScale::FromDimensions(mode.virtual_width, mode.virtual_height, frame.Width() + 1, frame.Height() + 1));
}
