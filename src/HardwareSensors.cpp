#include "HardwareSensors.h"
#include "Metrics.h"
#include <Drivers.h>
#include <OS.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
// Haiku poke ABI, from headers/private/drivers/poke.h (MIT, Oscar Lesta,
// copyright 2005). Kept here so building needs only the public Haiku SDK.
struct PciConfig {
    uint32 signature;
    uint8 bus, device, function, size, offset;
    uint32 value;
};
constexpr uint32 kReadConfig = B_DEVICE_OP_CODES_END + 5;
constexpr uint32 kWriteConfig = B_DEVICE_OP_CODES_END + 6;
constexpr uint32 kTemperatureRegister = 0x00059800;
static_assert(sizeof(PciConfig) == 16, "Haiku poke ABI layout");
}

bool ReadThreadripperTemperature(const std::string& cpuName, double& celsius) {
#if defined(__x86_64__) || defined(__i386__)
    // Do not probe unfamiliar CPUs or arbitrary PCI functions. This is the
    // exact CPU/root-complex combination verified on the X399 workstation.
    if (cpuName.find("AMD Ryzen Threadripper 1950X ") != 0) return false;
    cpuid_info cpu{};
    if (get_cpuid(&cpu, 1, 0) != B_OK) return false;
    uint32 family = ((cpu.regs.eax >> 8) & 15) + ((cpu.regs.eax >> 20) & 255);
    uint32 model = ((cpu.regs.eax >> 4) & 15) | ((cpu.regs.eax >> 12) & 240);
    if (family != 0x17 || model != 1) return false;
    int fd = open("/dev/misc/poke", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    PciConfig request{};
    request.signature = 'wltp'; request.size = 4;
    bool valid = ioctl(fd, kReadConfig, &request, sizeof(request)) == 0
        && request.value == 0x14501022; // AMD Family 17h root, 00:00.0
    uint32 raw = 0;
    if (valid) {
        // Select the read-only temperature register in the SMN address latch.
        // No sensor, power, clock, fan, PCI command, or SMN data register is
        // ever written. Haiku's amd_thermal driver uses the same transaction.
        request.offset = 0x60; request.value = kTemperatureRegister;
        valid = ioctl(fd, kWriteConfig, &request, sizeof(request)) == 0;
        if (valid) {
            request.offset = 0x64;
            valid = ioctl(fd, kReadConfig, &request, sizeof(request)) == 0;
            raw = request.value;
            // Reject a reading if another SMN user changed the address latch.
            request.offset = 0x60;
            valid = valid && ioctl(fd, kReadConfig, &request, sizeof(request)) == 0
                && request.value == kTemperatureRegister;
        }
    }
    close(fd);
    return valid && Metrics::Threadripper1950XTemperature(raw, celsius);
#else
    (void)cpuName; (void)celsius; return false;
#endif
}
