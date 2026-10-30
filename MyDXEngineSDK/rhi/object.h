#pragma once

#include "core/skyline_api.h"

namespace Skyline::RHI
{
class Device;

class SKYLINE_API DeviceObject
{
public:
    virtual ~DeviceObject();
    DeviceObject(const DeviceObject&) = delete;
    DeviceObject& operator=(const DeviceObject&) = delete;
    [[nodiscard]] const Device& device() const noexcept { return m_device; }

protected:
    explicit DeviceObject(const Device& device) noexcept : m_device(device) {}

private:
    const Device& m_device;
};
}
