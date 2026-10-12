// ============================================================================
// Main.cpp
// ----------------------------------------------------------------------------
// Windows 프로그램의 진입점.
//
// 초기화 실패 시 단순한 공통 메시지가 아니라
// Application이 기록한 "실패 단계 + 상세 원인"을 표시한다.
// ============================================================================

#include "Core/Application.h"

#include <Windows.h>

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int)
{
    Application application;

    if (!application.Initialize(hInstance))
    {
        // 진단 버전에서는 정확한 실패 단계가 이 팝업에 표시된다.
        MessageBoxW(
            nullptr,
            application.GetLastErrorMessage().c_str(),
            L"DX Framework Initialization Error",
            MB_OK | MB_ICONERROR);

        return -1;
    }

    return application.Run();
}
