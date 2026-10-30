#pragma once
#include "skyline_api.h"

#include <cstddef>
#include <exception>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>
#include <glog/logging.h>

#define SKYLINE_LOG(severity) LOG(severity)

class SKYLINE_API SkylineDebugger
{
public:
    SkylineDebugger() = delete;

    static void Initialize(const char* applicationName);
    static void Shutdown();
    static void ShowFatalError(const char* title, const std::exception& exception);
    static void ShowFatalError(const char* title, const char* message);

    static void EnableD3D12DebugLayer()
    {
#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;

        if (FAILED(D3D12GetDebugInterface(
                IID_PPV_ARGS(&debugController))))
        {
            SKYLINE_LOG(WARNING)
                << "D3D12 Debug Layer is unavailable.";

            return;
        }

        debugController->EnableDebugLayer();

        SKYLINE_LOG(INFO)
            << "D3D12 Debug Layer enabled.";
#endif
    }

    static void ConfigureD3D12(
        ID3D12Device* device)
    {
#if defined(_DEBUG)
        if (device == nullptr)
            return;

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

        if (FAILED(device->QueryInterface(
                IID_PPV_ARGS(&infoQueue))))
        {
            return;
        }

        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_CORRUPTION,
            TRUE);

        infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_ERROR,
            TRUE);
#endif
    }

    static void LogD3D12Messages(
        ID3D12Device* device)
    {
#if defined(_DEBUG)
        if (device == nullptr)
            return;

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;

        if (FAILED(device->QueryInterface(
                IID_PPV_ARGS(&infoQueue))))
        {
            return;
        }

        const UINT64 messageCount =
            infoQueue->
            GetNumStoredMessagesAllowedByRetrievalFilter();

        for (UINT64 i = 0; i < messageCount; ++i)
        {
            SIZE_T messageSize = 0;

            if (FAILED(infoQueue->GetMessage(
                    i,
                    nullptr,
                    &messageSize)))
            {
                continue;
            }

            std::vector<std::byte> buffer(messageSize);

            auto* message =
                reinterpret_cast<D3D12_MESSAGE*>(
                    buffer.data());

            if (FAILED(infoQueue->GetMessage(
                    i,
                    message,
                    &messageSize)))
            {
                continue;
            }

            switch (message->Severity)
            {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                SKYLINE_LOG(ERROR)
                    << "[D3D12] "
                    << message->pDescription;
                break;

            case D3D12_MESSAGE_SEVERITY_WARNING:
                SKYLINE_LOG(WARNING)
                    << "[D3D12] "
                    << message->pDescription;
                break;

            default:
                SKYLINE_LOG(INFO)
                    << "[D3D12] "
                    << message->pDescription;
                break;
            }
        }

        infoQueue->ClearStoredMessages();
#endif
    }
};
