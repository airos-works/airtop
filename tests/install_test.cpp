#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <File.h>
#include <IconUtils.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc, char** argv) {
    assert(argc == 2);
    BApplication app("application/x-vnd.AirOS-AirTop-install-test");
    BFile file(argv[1], B_READ_ONLY); assert(file.InitCheck() == B_OK);
    BAppFileInfo info(&file); char signature[B_MIME_TYPE_LENGTH] = {};
    assert(info.GetSignature(signature) == B_OK);
    assert(strcmp(signature, "application/x-vnd.AirOS-AirTop") == 0);
    version_info version{}; assert(info.GetVersionInfo(&version, B_APP_VERSION_KIND) == B_OK);
    assert(version.major == 1 && version.middle == 1 && version.minor == 1);
    uint8* data = nullptr; size_t size = 0;
    assert(info.GetIcon(&data, &size) == B_OK && size > 0);
    BBitmap icon(BRect(0, 0, 63, 63), B_RGBA32);
    assert(icon.InitCheck() == B_OK && BIconUtils::GetVectorIcon(data, size, &icon) == B_OK);
    free(data);
    int pixels = 0; const uint8* bits = static_cast<uint8*>(icon.Bits());
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x)
        if (bits[y * icon.BytesPerRow() + x * 4 + 3]) ++pixels;
    assert(pixels > 1000 && pixels < 4096);
    printf("Installed signature/version/icon: PASS (%zu-byte HVIF, %d visible pixels)\n", size, pixels);
}
