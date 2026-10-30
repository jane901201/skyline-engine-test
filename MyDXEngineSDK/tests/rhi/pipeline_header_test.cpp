#include "rhi/pipeline.h"
#include "rhi/device.h"
#include "rhi/shader.h"
#include "rhi/binding.h"
#include "rhi/resource.h"
#include "rhi/command_context.h"
#include "rhi/frame.h"
#include "render_pass/render_pass.h"
#include <type_traits>

// 公開 RHI 標頭須能獨立編譯，不可間接載入 D3D12 或 Windows 標頭。
#if defined(__d3d12_h__) || defined(_WINDOWS_) || defined(__dxgi_h__)
#error RHI headers must not depend on native graphics or platform headers.
#endif

static_assert(Skyline::RHI::MaxColorAttachments == 8);

class NeutralOnlyPass final : public RenderPass
{
public:
    void init(const PassInitContext&) override {}
    void execute(const PassFrameContext&) override {}
};
static_assert(!std::is_abstract_v<NeutralOnlyPass>);
