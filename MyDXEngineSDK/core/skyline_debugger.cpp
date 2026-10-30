#include "skyline_debugger.h"
#include <stdexcept>

#include <Windows.h>

void SkylineDebugger::Initialize(const char* applicationName)
{
    if (applicationName == nullptr || applicationName[0] == '\0')
    {
        throw std::invalid_argument("SkylineDebugger: Application name is required.");
    }

    google::InitGoogleLogging(applicationName);
    FLAGS_logtostderr = true;

    // 必須在建立 D3D12 裝置之前啟用除錯層。
    EnableD3D12DebugLayer();

    SKYLINE_LOG(INFO) << "[SkylineDebugger] Debugging initialized for " << applicationName << '.';
}

void SkylineDebugger::Shutdown()
{
    SKYLINE_LOG(INFO) << "[SkylineDebugger] Shutting down debugging.";

    google::ShutdownGoogleLogging();
}

void SkylineDebugger::ShowFatalError(const char* title, const std::exception& exception)
{
    ShowFatalError(title, exception.what());
}

void SkylineDebugger::ShowFatalError(const char* title, const char* message)
{
    const char* resolvedTitle = title != nullptr ? title : "SkylineEngine fatal error";
    const char* resolvedMessage = message != nullptr ? message : "An unknown fatal error occurred.";

    SKYLINE_LOG(ERROR) << '[' << resolvedTitle << "] " << resolvedMessage;
    MessageBoxA(nullptr, resolvedMessage, resolvedTitle, MB_OK | MB_ICONERROR);
}
