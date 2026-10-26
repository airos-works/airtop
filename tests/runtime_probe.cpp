#include <Application.h>
#include <Entry.h>
#include <Path.h>
#include <Roster.h>
#include <cassert>
#include <cstdio>
#include <cstring>
int main() {
    BApplication app("application/x-vnd.AirOS-AirTop-runtime-probe");
    app_info info{};
    assert(be_roster->GetAppInfo("application/x-vnd.AirOS-AirTop", &info) == B_OK);
    BEntry executable(&info.ref, true); BPath path;
    assert(executable.GetPath(&path) == B_OK);
    assert(strcmp(path.Path(), "/boot/home/config/non-packaged/apps/AirTop/AirTop") == 0);
    printf("Running installed executable: %s (team %ld)\n", path.Path(), (long)info.team);
}
