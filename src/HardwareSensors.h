#pragma once
#include <string>
// Narrow fallback for the X399's first-generation Threadripper. Generic
// machines use published thermal devices in Sampler instead.
bool ReadThreadripperTemperature(const std::string& cpuName, double& celsius);
