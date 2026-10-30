#pragma once

#include "rhi/device.h"
#include "rhi/command_context.h"
#include <stdexcept>

class GraphicsEngine;
class RenderContext;
class RootSignature;

//初始化渲染階段時所需的共用資源
struct RenderPassInitContext
{
    GraphicsEngine& graphicsEngine;
    RootSignature& rootSignature;
};

// 新初始化路徑明確傳入裝置與中立綁定配置，不依賴全域裝置。
struct RhiRenderPassInitContext
{
    GraphicsEngine& graphicsEngine;
    Skyline::RHI::Device& device;
    Skyline::RHI::BindingLayout& bindingLayout;
};

//執行單一畫格的渲染階段時所需的資料
struct RenderFrameContext
{
    RenderContext& renderContext;
    GraphicsEngine& graphicsEngine;
    std::uint32_t width;
    std::uint32_t height;
};

struct PassInitContext
{
    Skyline::RHI::Device& device;
    Skyline::RHI::AttachmentLayout attachments;
    std::uint32_t width, height;
};
struct PassFrameContext
{
    Skyline::RHI::CommandContext& commands;
    std::uint32_t frameIndex, width, height;
};

//所有渲染階段的共用介面
class RenderPass
{
  public:
    virtual ~RenderPass() = default;

    //建立此渲染階段所需的固定資源與 pipeline 狀態
    [[deprecated("Replaced by neutral Pass initialization. Use init(PassInitContext).")]]
    virtual void init(const RenderPassInitContext& context)
    {
        throw std::logic_error("RenderPass: This pass does not implement legacy initialization.");
    }
    [[deprecated("Replaced by neutral Pass initialization. Use init(PassInitContext).")]]
    virtual void init(const RhiRenderPassInitContext& context)
    {
        throw std::logic_error("RenderPass: This legacy pass does not implement RHI initialization.");
    }

    //將此渲染階段的命令記錄至目前 frame 的 RenderContext
    [[deprecated("Replaced by neutral Pass commands. Use execute(PassFrameContext).")]]
    virtual void execute(const RenderFrameContext& context)
    {
        throw std::logic_error("RenderPass: This pass does not implement legacy commands.");
    }
    virtual void init(const PassInitContext& context)
    {
        throw std::logic_error("RenderPass: This compatibility pass does not implement neutral initialization.");
    }
    virtual void execute(const PassFrameContext& context)
    {
        throw std::logic_error("RenderPass: This compatibility pass does not implement neutral commands.");
    }
};
