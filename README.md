# AirTop

A native Air/OS and Haiku system monitor with a dark, btop-inspired dashboard. Written in C++17 using Haiku's Interface Kit, Storage Kit, Network Kit, and kernel APIs. No browser, Qt, SDL, shell polling, or third-party runtime is needed.

## What it monitors

- Aggregate CPU load, every logical CPU, mean reported clock, thread count, and two minutes of history.
- Physical memory in use and available, cached pages, block cache, swap, and page faults per second.
- Receive/send rates and histories across non-loopback network interfaces; click the network panel for each interface's address, link state, lifetime byte totals, and error count.
- Processes (Haiku teams), with CPU, resident memory, and thread counts. Filter by name, command, or ID; sort every column; open details for command, virtual memory, UID, and priority.
- Every mounted volume with meaningful capacity, including network shares: filesystem, capacity, free space, percent used, and read-only status. Scroll the panel for more volumes. Volume capacity refreshes every five seconds; physical disk I/O throughput is not exposed by the standard API used in v1.
- CPU die temperature on the X399 / Threadripper 1950X through its existing Haiku hardware-access driver, with the model-specific Tctl offset removed.
- Thermal devices under `/dev/power`, `/dev/sensor`, and `/dev/sensors`, including the standard AMD, ACPI, and PCH thermal driver text format. The hottest valid sensor is displayed with its source; devices are rediscovered every 30 seconds.
- Display adapter identity from `BScreen`. GPU utilization is explicitly unavailable in v1: the standard display API provides no utilization counter.

**GPU limitation:** the GPU is identified as an NVIDIA GeForce GTX 1070 / GP104-A, but GPU utilization is not available in v1. CPU temperature is live on the X399.

The normal temperature backend uses published thermal devices. Where none exist, a narrowly gated fallback supports the verified AMD Threadripper 1950X (family 17h, model 1, PCI root 1022:1450 at 00:00.0). It accesses SMN temperature register 0x59800 through the existing `/dev/misc/poke` driver. Only the SMN address latch is selected; it never writes temperature controls, SMN data, clocks, fans, or power settings. Unknown hardware and failed reads remain unavailable. No drivers are installed or replaced. See [thermal implementation notes](docs/THERMALS.md).

## Build and install on Haiku

```sh
make -j4
make check
make install
/boot/home/config/non-packaged/apps/AirTop/AirTop
```

Requires Haiku's C++ development tools and standard `libbe`, `libbnetapi`, and `libnetwork`. The included HVIF icon and resources are attached with `rc` and `xres`. `make install` installs per user and adds **AirTop** under **Deskbar → Applications**. It does not add login startup. The native source tree is also deployed to `/boot/home/Develop/AirTop` on the X399.

From this Linux workspace, `tools/deploy.sh` transfers source, builds, tests, and installs on the X399 using the existing SSH configuration. Set `AIRTOP_SSH_CONFIG` and `AIRTOP_HOST` for another machine. Credentials are not stored in this project.

The counter/format/thermal parser and animation checks can run on Linux with `make check`. On Haiku, `make check-cadence` verifies collection timing and process CPU accounting under a bounded one-core workload. Regenerate the original vector icon using `python3 tools/make-icon.py`.

## Auxiliary display (DSI panel, USB LCD)

On a machine with an auxiliary display (`/dev/auxdisplay`, for example the
Raspberry Pi 4's DSI connector with the `rpi_dsi` driver) AirTop shows its
processor graph with the core loads, the network graph and the temperature
there, through the Device Kit's `BAuxDisplay`. The panel starts with the
window and can be switched off and on from the View menu.

`AirTop --panel` runs only the panel, without a window: the Raspberry Pi
image starts it that way from the UserBootscript. Launching AirTop again
opens the window beside it (single launch); closing the window keeps the
panel running. It needs a Haiku with `BAuxDisplay` in libdevice (air/OS
from 2026-10-08); the panel renders at five frames a second (about one percent of a Raspberry Pi 4) into the
display's own buffers.

When only the auxiliary panel is running, AirTop collects system totals and
graph samples without collecting the dashboard’s per-process table. Opening
the dashboard resumes process collection; closing it stops that extra work.
A plain relaunch opens the dashboard again. `--sample` continues to include
full process data.

The panel caches its fixed background, rounded borders, titles and captions.
Changing text, values and graphs still render on every frame. At 640×480 the cache holds 1,228,800
bytes of pixels and needs no extra drawing window or thread. Turning off the
panel deletes the cache object; Haiku may retain its bitmap-pool space for
reuse. If allocation fails, drawing continues through the direct path. `make check-panel` compares both paths across scaling, changing data
and the diagnostic `AIRTOP_PANEL_SKIP` modes.

## Controls

| Input | Action |
|---|---|
| Space / Pause button | Freeze or resume the display |
| `/` / filter field | Search names, commands, and team IDs |
| Escape | Clear filter |
| C / M / I / N / T | Sort by CPU / memory / PID / name / threads |
| Same sort key again / column heading | Reverse sorting |
| Arrow keys / Page Up / Page Down / Home / End | Browse processes |
| Enter / double-click process | Process details |
| Mouse wheel | Scroll processes, or volumes when over that panel |
| + / - / interval button | Cycle 0.25, 0.5, 1, and 2-second sampling |
| Click network panel | Next interface |
| F1 / View menu | Help and metric definitions |
| View menu | Show / hide the auxiliary display panel |
| Alt-Q | Quit (Haiku Command-Q shortcut) |

Read-only state is available through `hey AirTop get MonitorState of Window 0` for automation and testing.

Window bounds and sampling interval are saved in `~/config/settings/AirTop_settings`. CPU, memory, and network counters default to quarter-second collection; existing saved intervals are preserved. Process and temperature collection runs about once per second (or at the selected interval when slower), and volume capacity every five seconds. Collection runs on a worker thread so filesystem probes do not block UI input. A delayed collector is indicated in the footer.

The display animates at approximately 30 frames per second with buffered drawing. CPU/core values and bars ease between observations; graphs scroll continuously across a fixed two-minute timeline. The graph's advancing edge follows one sampling interval behind collection to interpolate between actual observations, without extrapolating future measurements. Rounded curves stay within adjacent observations. Sampling continues while paused, but animation stops; resuming shows current data and recent history. Animation also stops while minimized or on another workspace.

The drawing buffer follows the desktop's pixel density. On the X399 at 2×, text and graphics are rasterized into a buffer twice the logical width and height, then copied at native device resolution. Window layout and mouse coordinates remain logical. Density is checked once per second, including while paused, and changing density or resizing recreates the buffer. `make check-rendering` on Haiku verifies density detection, buffer bounds, native font rasterization, and partial repaint clipping.

## Metric semantics

CPU uses differences of active time between monotonic timestamps. The aggregate is normalized over enabled CPUs; each process reports 100% per core and may exceed 100%. Kernel idle threads are excluded from the kernel team's CPU calculation. Newly observed teams start at zero until a second observation; team start times prevent PID reuse from creating spikes. Counter resets and disappearing interfaces do not produce negative rates or unsigned underflow.

Memory in use is total physical memory minus the kernel's `free_memory` (available memory), matching Haiku's `vmstat`. Cache counters are informational and can overlap available memory; do not add all of them together. Process memory sums resident areas, which can include shared pages; it is not unique/private memory and should not be summed across teams.

Network histories aggregate all non-loopback interfaces; the bottom two lines describe the selected interface. Traffic generated by VNC or SSH is real network traffic and therefore appears in these counters. Disk capacity includes mounted local and network filesystems; virtual filesystems with zero capacity are omitted.

The initial observation establishes a baseline. Rates and CPU percentages become meaningful with the second sample. Sampling failure retains the last observation with a delayed/waiting state instead of inventing values. Thermal readings outside -50 to 150 °C or malformed/nonfinite readings are rejected.

## Diagnostic sampling

```sh
AirTop --sample 5 > samples.jsonl
AirTop --panel
```

This collects the same backend counters at one-second intervals without opening a window. Each line is JSON with CPU/core loads, memory, networks, volumes, temperatures (null when unavailable), process data, and collection time. Sample count is bounded to 1–3600.

## Uninstall

Quit AirTop, then remove `~/config/non-packaged/apps/AirTop` and the `~/config/non-packaged/data/deskbar/menu/Applications/AirTop` symlink. Optionally remove `~/config/settings/AirTop_settings`. Other applications and system drivers are unaffected.
