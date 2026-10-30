#pragma once

#include "rhi/device.h"
#include <directx/d3d12.h>
#include <wrl/client.h>
#include <utility>

namespace Skyline::D3D12
{
// 這些具體物件只在後端使用，不作為公開 RHI 合約。
class Shader final : public RHI::Shader
{
public:
    Shader(const RHI::Device& device, RHI::ShaderStage stage, Microsoft::WRL::ComPtr<ID3DBlob> blob)
        : RHI::Shader(device), m_stage(stage), m_blob(std::move(blob)) {}
    RHI::ShaderStage stage() const noexcept override { return m_stage; }
    D3D12_SHADER_BYTECODE bytecode() const noexcept { return {m_blob->GetBufferPointer(), m_blob->GetBufferSize()}; }

private:
    RHI::ShaderStage m_stage;
    Microsoft::WRL::ComPtr<ID3DBlob> m_blob;
};

class BindingLayout final : public RHI::BindingLayout
{
public:
    BindingLayout(const RHI::Device& device, RHI::BindingLayoutDesc desc,
                   Microsoft::WRL::ComPtr<ID3D12RootSignature> layout)
        : RHI::BindingLayout(device), m_desc(std::move(desc)), m_layout(std::move(layout)) {}
    const RHI::BindingLayoutDesc& description() const noexcept override { return m_desc; }
    ID3D12RootSignature* native() const noexcept { return m_layout.Get(); }

private:
    RHI::BindingLayoutDesc m_desc;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_layout;
};

class Pipeline final : public RHI::Pipeline
{
public:
    Pipeline(const RHI::Device& device, RHI::PipelineKind kind,
              Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline, ID3D12RootSignature* layout,
              bool bindings = false, bool vertices = false, RHI::AttachmentLayout attachments = {},
              RHI::PrimitiveType primitive = RHI::PrimitiveType::Triangle)
        : RHI::Pipeline(device), needsBindings(bindings), needsVertices(vertices), attachments(attachments), primitive(primitive),
          m_kind(kind), m_pipeline(std::move(pipeline)), m_layout(layout) {}
    RHI::PipelineKind kind() const noexcept override { return m_kind; }
    ID3D12PipelineState* native() const noexcept { return m_pipeline.Get(); }
    ID3D12RootSignature* layout() const noexcept { return m_layout.Get(); }
    bool needsBindings, needsVertices;
    RHI::AttachmentLayout attachments;
    RHI::PrimitiveType primitive;

private:
    RHI::PipelineKind m_kind;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipeline;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_layout;
};
}
