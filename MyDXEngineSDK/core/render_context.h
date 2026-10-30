#pragma once

#include <directx/d3d12.h>
#include <stdexcept>
#include <vector>
#include <wrl/client.h>

#include "index_buffer.h"
#include "pipeline_state.h"
#include "root_signature.h"
#include "vertex_buffer.h"
#include "rhi/resource.h"

class ConstantBuffer;
class DescriptorHeap;
class RenderTarget;
class Texture;

// 封裝 ID3D12GraphicsCommandList 並集中管理常用的 Graphics 與 Compute 指令
class RenderContext
{
  public:
    // 第二階段先提供命令轉接，完整中立 CommandContext 於後續階段導入。
    SKYLINE_API void setGraphicsPipeline(const Skyline::RHI::Pipeline& pipeline);
    SKYLINE_API void setComputePipeline(const Skyline::RHI::Pipeline& pipeline);
    SKYLINE_API void setRootSignature(const Skyline::RHI::BindingLayout& layout);
    SKYLINE_API void setBindingSet(const Skyline::RHI::BindingSet& set, const Skyline::RHI::Pipeline& pipeline);
    SKYLINE_API void transitionBuffer(const Skyline::RHI::Buffer& buffer,
        Skyline::RHI::ResourceState before, Skyline::RHI::ResourceState after);
    SKYLINE_API void storageBarrier(const Skyline::RHI::Buffer& buffer);

    void setGraphicsPipeline(const PipelineState& pipeline)
    {
        m_commandList->SetGraphicsRootSignature(pipeline.getBindingLayout());
        m_commandList->SetPipelineState(pipeline.get());
    }

    void setComputePipeline(const PipelineState& pipeline)
    {
        m_commandList->SetComputeRootSignature(pipeline.getBindingLayout());
        m_commandList->SetPipelineState(pipeline.get());
    }

    // 初始化並保存外部傳入的 Graphics Command List
    void init(ID3D12GraphicsCommandList10* commandList)
    {
        // Command List 不可為空指標
        if (commandList == nullptr)
            throw std::invalid_argument("RenderContext: A command list is required.");

        m_commandList = commandList;
    }

    // 將 Vertex Buffer 綁定到 Input Assembler 的 Slot 0
    void setVertexBuffer(const VertexBuffer& vb)
    {
        m_commandList->IASetVertexBuffers(0, 1, &vb.getView());
    }

    // 將 Index Buffer 綁定到 Input Assembler
    void setIndexBuffer(const IndexBuffer& ib)
    {
        m_commandList->IASetIndexBuffer(&ib.getView());
    }

    // 設定 Input Assembler 使用的 Primitive Topology
    void setPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
    {
        m_commandList->IASetPrimitiveTopology(topology);
    }

    // 更換目前 RenderContext 使用的 Graphics Command List
    void SetCommandList(ID3D12GraphicsCommandList10* commandList)
    {
        m_commandList = commandList;
    }

    // 同時設定 Viewport 與對應大小的 Scissor Rect
    void setViewportAndScissor(D3D12_VIEWPORT& viewport)
    {
        D3D12_RECT scissorRect;

        // 將 Scissor Rect 下邊界設定成 Viewport 高度
        scissorRect.bottom = static_cast<LONG>(viewport.Height);

        scissorRect.top = 0;
        scissorRect.left = 0;

        // 將 Scissor Rect 右邊界設定成 Viewport 寬度
        scissorRect.right = static_cast<LONG>(viewport.Width);

        // 設定 Rasterizer 使用的 Viewport
        m_commandList->RSSetViewports(1, &viewport);

        // 設定 Rasterizer 使用的 Scissor Rect
        m_commandList->RSSetScissorRects(1, &scissorRect);

        // 保存目前使用的 Viewport
        m_currentViewport = viewport;
    }

    // 取得目前保存的 Viewport
    [[nodiscard]]
    D3D12_VIEWPORT getViewport() const
    {
        return m_currentViewport;
    }

    // 單獨設定 Rasterizer 使用的 Scissor Rect
    void setScissorRect(D3D12_RECT& rect)
    {
        m_commandList->RSSetScissorRects(1, &rect);
    }

    // 使用原始 ID3D12RootSignature 指標設定 Graphics Root Signature
    void setRootSignature(ID3D12RootSignature* rootSignature)
    {
        m_commandList->SetGraphicsRootSignature(rootSignature);
    }

    // 使用 RootSignature 包裝類別設定 Graphics Root Signature
    void setRootSignature(RootSignature& rootSignature)
    {
        m_commandList->SetGraphicsRootSignature(rootSignature.get());
    }

    // 使用原始 ID3D12RootSignature 指標設定 Compute Root Signature
    void setComputeRootSignature(ID3D12RootSignature* rootSignature)
    {
        m_commandList->SetComputeRootSignature(rootSignature);
    }

    // 使用 RootSignature 包裝類別設定 Compute Root Signature
    void setComputeRootSignature(RootSignature& rootSignature)
    {
        m_commandList->SetComputeRootSignature(rootSignature.get());
    }

    // 使用原始 ID3D12PipelineState 指標設定目前的 Pipeline State
    void setPipelineState(ID3D12PipelineState* pipelineState)
    {
        m_commandList->SetPipelineState(pipelineState);
    }

    // 使用 PipelineState 包裝類別設定目前的 Pipeline State
    void setPipelineState(PipelineState& piplineState)
    {
        m_commandList->SetPipelineState(piplineState.get());
    }

    // 將 Constant Buffer 的 GPU 位址設定到指定的 Graphics Root Parameter
    void setGraphicsRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address);
    }

    // 綁定一個 Descriptor Heap 到目前的 Command List
    void setDescriptorHeap(ID3D12DescriptorHeap* descHeap)
    {
        m_descriptorHeaps[0] = descHeap;

        m_commandList->SetDescriptorHeaps(1, m_descriptorHeaps);
    }

    // 使用 DescriptorHeap 包裝類別設定 Descriptor Heap
    void setDescriptorHeap(DescriptorHeap& descHeap);

    // 設定 Compute Pipeline 使用的 Descriptor Heap
    void setComputeDescriptorHeap(DescriptorHeap& descHeap);

    // 一次將多個 Descriptor Heap 綁定到 Command List
    void setDescriptorHeaps(int numDescriptorHeap, const DescriptorHeap* descHeaps[]);

    // 將 Constant Buffer 指標保存到指定的 b register 對應位置
    void setConstantBuffer(int registerNo, ConstantBuffer& cb)
    {
        // 確認 register 編號沒有超出可保存的 Constant Buffer 數量
        if (registerNo < MAX_CONSTANT_BUFFER)
        {
            m_constantBuffers[registerNo] = &cb;
        }
        else
        {
            // 超出允許範圍時直接終止程式
            std::abort();
        }
    }

    // 將 Texture 指標保存到指定的 t register 對應位置
    void setShaderResource(int registerNo, Texture& texture)
    {
        // 確認 register 編號沒有超出可保存的 Shader Resource 數量
        if (registerNo < MAX_SHADER_RESOURCE)
        {
            m_shaderResources[registerNo] = &texture;
        }
        else
        {
            // 超出允許範圍時直接終止程式
            std::abort();
        }
    }

    // 同時設定多個 Render Target
    void setRenderTargets(UINT numRT, RenderTarget* renderTargets[]);

    // 設定單一 Render Target 與 Depth Stencil
    void setRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
    {
        m_commandList->OMSetRenderTargets(1, &rtvHandle,

                                          // 表示傳入的 RTV Descriptor 不視為連續排列
                                          FALSE,

                                          &dsvHandle);
    }

    // 插入 Resource Barrier 以通知 GPU Resource 的使用狀態或存取關係發生變化
    void resourceBarrier(D3D12_RESOURCE_BARRIER& barrier)
    {
        m_commandList->ResourceBarrier(1, &barrier);
    }

    // 關閉已完成指令錄製的 Command List
    void close()
    {
        m_commandList->Close();
    }

    // Reset Command List 並清除上一輪暫時保存的 GPU Resource
    void reset(ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipelineState)
    {
        m_commandList->Reset(commandAllocator, pipelineState);

        // 清除暫時保存的 GPU Resource
        m_scratchResourceList.clear();
    }

    // 使用 Index Buffer 繪製一個 Instance
    void drawIndexed(UINT indexCount)
    {
        m_commandList->DrawIndexedInstanced(
            // 設定要使用的 Index 數量
            indexCount,

            // 固定繪製一個 Instance
            1,

            // Index Buffer 從第 0 個 Index 開始
            0,

            // 不對 Vertex Index 額外增加偏移
            0,

            // Instance ID 從 0 開始
            0);
    }

    // 使用 Index Buffer 一次繪製多個 Instance
    void drawIndexedInstanced(UINT indexCount, UINT numInstance)
    {
        m_commandList->DrawIndexedInstanced(
            // 設定每個 Instance 使用的 Index 數量
            indexCount,

            // 設定要繪製的 Instance 數量
            numInstance,

            // Index Buffer 從第 0 個 Index 開始
            0,

            // 不對 Vertex Index 額外增加偏移
            0,

            // Instance ID 從 0 開始
            0);
    }

    // 將 Descriptor Heap 中的 GPU Handle 綁定到指定的 Graphics Descriptor Table
    void setGraphicsRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        m_commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
    }

    // 將 Constant Buffer 的 GPU 位址設定到指定的 Compute Root Parameter
    void setComputeRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
    {
        m_commandList->SetComputeRootConstantBufferView(rootParameterIndex, address);
    }

    // 將 Descriptor Heap 中的 GPU Handle 綁定到指定的 Compute Descriptor Table
    void setComputeRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        m_commandList->SetComputeRootDescriptorTable(rootParameterIndex, handle);
    }

    // Dispatch 指定數量的 Compute Shader Thread Group
    void dispatch(UINT x, UINT y, UINT z)
    {
        m_commandList->Dispatch(x, y, z);
    }

    // 不使用 Index Buffer 繪製指定數量的 Vertex 並固定為一個 Instance
    void draw(UINT vertexCount)
    {
        m_commandList->DrawInstanced(
            // 設定要繪製的 Vertex 數量
            vertexCount,

            // 固定繪製一個 Instance
            1,

            // Vertex Buffer 從第 0 個 Vertex 開始
            0,

            // Instance ID 從 0 開始
            0);
    }

  private:
    // 定義最多可暫存四個 Descriptor Heap
    enum
    {
        MAX_DESCRIPTOR_HEAP = 4
    };

    // 定義最多可保存八個 Constant Buffer
    enum
    {
        MAX_CONSTANT_BUFFER = 8
    };

    // 定義最多可保存十六個 Shader Resource
    enum
    {
        MAX_SHADER_RESOURCE = 16
    };

    // 保存目前使用的 Viewport
    D3D12_VIEWPORT m_currentViewport;

    // 保存目前使用的 Graphics Command List，生命週期由外部管理
    ID3D12GraphicsCommandList10* m_commandList;

    // 保存目前準備綁定到 Command List 的 Descriptor Heap
    ID3D12DescriptorHeap* m_descriptorHeaps[MAX_DESCRIPTOR_HEAP] = {nullptr};

    // 保存對應 b0～b7 的 Constant Buffer 物件指標
    ConstantBuffer* m_constantBuffers[MAX_CONSTANT_BUFFER] = {nullptr};

    // 保存對應 t0～t15 的 Texture 或 Shader Resource 物件指標
    Texture* m_shaderResources[MAX_SHADER_RESOURCE] = {nullptr};

    // 暫時持有 GPU Resource 以避免在 GPU 執行完成前被過早釋放
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_scratchResourceList;
};
