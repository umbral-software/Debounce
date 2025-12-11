#include "NotificiationIcon.hpp"

// LowLevelMouseProc has no way to stash a user data pointer with it. So these have to be global.
// MSLLHOOKSTRUCT::dwExtraInfo is for device-specific info, not user data afaict.
DWORD DEBOUNCE_THRESHOLD_MS = DEFAULT_DEBOUNCE_THRESHOLD_MS;
static DWORD LAST_LBUTTON_UP, LAST_RBUTTON_UP;

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        const auto& hookStruct = *reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        switch (wParam) {
        case WM_LBUTTONDOWN:
            if (hookStruct.time < LAST_LBUTTON_UP + DEBOUNCE_THRESHOLD_MS) {
                return 1;
            }
            break;
        case WM_LBUTTONUP:
            LAST_LBUTTON_UP = hookStruct.time;
            break;
        case WM_RBUTTONDOWN:
            if (hookStruct.time < LAST_RBUTTON_UP + DEBOUNCE_THRESHOLD_MS) {
                return 1;
            }
            break;
        case WM_RBUTTONUP:
            LAST_RBUTTON_UP = hookStruct.time;
            break;
        default:
            break;
        }
    
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

_Use_decl_annotations_
int wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    const auto mutex = CreateMutexExA(nullptr, "Local\\uk.umbral.debounce", CREATE_MUTEX_INITIAL_OWNER, SYNCHRONIZE);
    if (!mutex) {
        MessageBoxA(nullptr, "Could not determine if another instance of Debounce is already running.", "Could not create mutex", MB_ICONERROR);
        return -1;
    }

    switch (WaitForSingleObject(mutex, 0)) {
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
        break;
    case WAIT_TIMEOUT:
        MessageBoxA(nullptr, "An existing instance of Debounce is already running", "Duplicate Instance", MB_ICONWARNING);
        return 1;
    default:
        MessageBoxA(nullptr, "Could not determine if another instance of Debounce is already running.", "Could not wait for mutex", MB_ICONERROR);
        return -2;
    }

    NotificationIcon ni;

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) MessageBoxA(nullptr, "Debounce failed to set process priority to high. You may experience additional input delay.", "Failed to set Priority", MB_ICONWARNING);
    if (!SetWindowsHookExA(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0)) {
        MessageBoxA(nullptr, "Debounce failed to register hook.", "Hook failed", MB_ICONERROR);
        return -3;
    }

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (WM_QUIT == msg.message) return static_cast<int>(msg.wParam);

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return -4;
}
