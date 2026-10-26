# X399 validation — 7 October 2026

Target: `192.168.1.244`, Haiku x86_64 hrev60206+686, AMD Ryzen Threadripper 1950X (32 logical CPUs), about 64 GiB of RAM, NVIDIA GeForce GTX 1070. Tests use real system APIs and the running native application.

- Native GCC 13.3 build with `-Wall -Wextra -Wpedantic`: no warnings in the final build. Link dependencies are the standard Haiku `be`, `bnetapi`, and `network` libraries.
- `make check`: 18 counter, reset, formatting, native sensor parsing, and Threadripper temperature-decoding assertions pass, on both Linux and Haiku.
- `tools/validate-live.py`: 15 real samples with a bounded eight-second CPU workload and 64 MiB allocation. Confirms one-core process CPU exceeds 70%, the allocation appears in resident memory, processes disappear on exit, CPU/core bounds hold, kernel idle time is excluded, two interfaces and four volumes are present, and the physical CPU sensor is available and plausible on every sample. Exact values are in `evidence/load-validation.json`; raw observations are in `evidence/load-samples.jsonl`.
- Actual VNC keyboard and mouse events, verified against the app's read-only `MonitorState` property: case-insensitive filtering, clear filter, memory sorting, reversing sort, pause freezing the displayed sequence, resume advancing it, process navigation/scrolling, process details, Escape closing details, F1 help, Escape closing help, interval changes, minimum sizing, pause/resume buttons, interface cycling, mouse-wheel volume scrolling, and column-header sorting.
- Window frame and sampling interval persist across a full process restart. The original release was tested at one-second sampling; version 1.1 is left at quarter-second sampling with no filter.
- The real screenshots `evidence/minimum.png`, `evidence/dashboard.png`, `evidence/filter.png`, `evidence/details.png`, and `evidence/volumes-scrolled.png` were visually inspected. Minimum size is 1000×790 client pixels; the larger dashboard uses extra height for more processes and volumes.
- Native installation test reads the installed signature and version, then decodes the embedded 206-byte HVIF icon using Haiku's `BIconUtils` (2,417 nontransparent pixels at 64×64). The installed and tested executable hashes match. See `evidence/install-validation.log`.

Installation: `/boot/home/config/non-packaged/apps/AirTop/AirTop`, with a Deskbar Applications symlink and source under `/boot/home/Develop/AirTop`. No system drivers were replaced, no reboot was performed, and no login startup was added.

The first VNC test connection did not deliver its events to the active app. The final tests explicitly activate AirTop and use fresh VNC connections; state assertions establish that input was received. Screenshot-only comparisons were not treated as evidence of pause. An early run also exposed a missing Escape shortcut in native alerts and missing frame-resize notifications; both were fixed and the affected tests rerun.

GPU utilization and physical disk I/O throughput are not provided in v1. GPU identity and mounted-volume capacity are supported. CPU temperature is a real Tdie reading on this X399; see `THERMALS.md` for its narrowly scoped hardware backend.

## Version 1.1: smoother updates

- Timestamped histories preserve a two-minute viewport at every sampling interval. Portable animation tests cover bounded easing, interrupted transitions, both graph edges, no extrapolation, mixed sampling cadence, and bounded history retention. They pass on Linux and Haiku, alongside the existing 18 metric checks.
- Native worker timing test: 22 observations and six process updates in 5.5 seconds at the default 250 ms interval. A bounded one-core workload measured 101.9% process CPU, verifying that process rates use their own elapsed interval. The slowest collection in that run took 19.18 ms.
- The native UI test measured 30.1 drawn frames per second with buffered rendering and quarter-second observations. It also verifies that pause freezes both the sample sequence and animation. The existing input, resize, and alert tests pass with the new renderer. Both normal and minimum-size screenshots were inspected.
- `tools/validate-animation.py` observes graph movement and CPU interpolation between unchanged sample sequences, checks that the graph cursor never moves backward, and verifies that minimizing stops drawing and restoring resumes animation.
- In the short post-change workload capture, AirTop used 2.6–3.3% of one logical CPU; the shared `app_server` used 32.3–34.2% of one CPU, including other desktop work. This machine has 32 logical CPUs. These are workload observations, not isolated rendering benchmarks.
- Build, timing, UI, interpolation, screenshot, and installation evidence is under `evidence/smoothing/`.

## Version 1.1.1: native 2× rendering

The 1.1 drawing buffer was sized in logical pixels, causing its already-rasterized text and graphics to be enlarged by the 2× desktop. The buffer now uses device-pixel dimensions and scales vector/font drawing before rasterization. Full source/destination mapping preserves pixel alignment across partial updates.

- Native display probe: 7680×2160 device pixels, 3840×1080 logical pixels, density 2.00. The running dashboard uses a 2002×1538 buffer for a 1001×769 logical view.
- `make check-rendering` passes: 1×, 1.5×, and 2× geometry, invalid geometry fallback, inclusive rectangle bounds, pixel-for-pixel equivalence between a logical 12-pixel font drawn at 2× and a native 24-pixel font, and device-pixel clipping across a scaled drawing state.
- `tools/validate-hidpi.py` verifies buffer dimensions before and after resize, restores the original window frame, checks logical mouse coordinates, pause, and filter input, and measures 29.9 frames per second at 2×.
- The dashboard screenshot was inspected for layout and repaint defects. Haiku's screenshot API returns a downsampled logical-resolution image, so the native font test and buffer dimensions provide the density verification. A hardware KVM capture timed out and was not used as evidence.
- Build, rendering, UI, and installation evidence is under `evidence/hidpi/`.
