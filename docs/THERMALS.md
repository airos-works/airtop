# Temperature backends

AirTop prefers the standard Haiku thermal devices, reading their bounded text status with `pread`. The hottest valid reported reading is displayed with its source. On systems with ACPI/PCH rather than CPU sensors, the UI calls this a thermal sensor rather than misidentifying it as CPU die temperature.

The X399's installed Haiku publishes no thermal device even though the CPU has a sensor. AirTop supplies a tightly limited Threadripper 1950X fallback:

1. Require the exact 1950X brand, CPUID family 17h/model 1, and the expected PCI identity `1022:1450` at `00:00.0`.
2. Require that the standard thermal-device scan found no device. This avoids taking the address latch from an active native thermal driver.
3. Open Haiku's existing `/dev/misc/poke` interface. Check all ioctl results. The ABI definitions are isolated in `src/HardwareSensors.cpp`; unsupported systems simply fail this optional backend.
4. Select SMN address `0x59800` through PCI configuration offset `0x60`, read its data at `0x64`, then verify the selector still points at the requested address. No write is made to the SMN data register or hardware operating controls.
5. Decode the temperature field (bits 31–21, units of 0.125 °C), subtract 49 °C when the range-selection bits require it, and subtract the 1950X's 27 °C Tctl offset to report **Tdie**.
6. Reject zero/all-ones register responses and results outside 0–115 °C. A failed read is unavailable, never zero or a stale “live” value.

The standard register-selection transaction is used by [Haiku's AMD thermal driver](https://github.com/haiku/haiku/blob/master/src/add-ons/kernel/drivers/power/amd_thermal/amd_thermal.cpp). The temperature register format and model-specific offset are cross-checked against [Linux's k10temp driver](https://github.com/torvalds/linux/blob/master/drivers/hwmon/k10temp.c). AirTop contains its own small decoder and does not link or copy the Linux driver.

Validation on the target machine includes live changing temperatures in `evidence/thermal-probe.log`, the integrated load samples, and unit checks for ordinary and extended temperature ranges, invalid registers, and out-of-range values. This measures CPU package/die temperature, not a separate temperature for each logical core. Other CPU models use native thermal devices; they are not probed with this fallback.
