#include "device.h"
#include "command_context.h"
#include "frame.h"

namespace Skyline::RHI
{
Device::~Device() = default;
DeviceObject::~DeviceObject() = default;
Shader::~Shader() = default;
BindingLayout::~BindingLayout() = default;
Pipeline::~Pipeline() = default;
Buffer::~Buffer() = default;
BufferView::~BufferView() = default;
BindingSet::~BindingSet() = default;
Texture::~Texture() = default;
TextureView::~TextureView() = default;
CommandContext::~CommandContext() = default;
Frame::~Frame() = default;
}
