#pragma once

#include "object.h"
#include "types.h"
#include <string>

namespace Skyline::RHI
{
struct ShaderDesc
{
    std::string filePath;
    std::string entryPoint;
    ShaderStage stage = ShaderStage::Unknown;
    std::string debugName = "Shader";
};

class SKYLINE_API Shader : public DeviceObject
{
public:
    ~Shader() override;
    [[nodiscard]] virtual ShaderStage stage() const noexcept = 0;

protected:
    explicit Shader(const Device& device) noexcept : DeviceObject(device) {}
};
}
