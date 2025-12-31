#include "Debounce.hpp"
#include "NotificiationIcon.hpp"

#include <stdexcept>

static constexpr char DELAYMS_KEY_NAME[] = "delayMs";
static constexpr char SETTINGS_KEY_NAME[] = "Software\\Umbral Software\\Debounce";

// LowLevelMouseProc has no way to stash a user data pointer with it. So these have to be global.
// MSLLHOOKSTRUCT::dwExtraInfo is for device-specific info, not user data afaict.
static DWORD DEBOUNCE_THRESHOLD_MS;
static DWORD LAST_LBUTTON_EVENT, LAST_RBUTTON_EVENT;

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        const auto& hookStruct = *reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            if (hookStruct.time < LAST_LBUTTON_EVENT + DEBOUNCE_THRESHOLD_MS) {
                return 1;
            }
            LAST_LBUTTON_EVENT = hookStruct.time;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            if (hookStruct.time < LAST_RBUTTON_EVENT + DEBOUNCE_THRESHOLD_MS) {
                return 1;
            }
            LAST_RBUTTON_EVENT = hookStruct.time;
            break;
        default:
            break;
        }
    
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

DWORD GetDebounceDelay() noexcept {
    return DEBOUNCE_THRESHOLD_MS;
}

void SetDebounceDelay(DWORD delayMs) {
    DEBOUNCE_THRESHOLD_MS = delayMs;
    if (RegSetKeyValueA(HKEY_CURRENT_USER, SETTINGS_KEY_NAME, DELAYMS_KEY_NAME, REG_DWORD, &delayMs, sizeof(delayMs))) {
        throw std::runtime_error("Unable to save Delay config");
    }    
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

    DWORD delayMs;
    DWORD delayMsSz = sizeof(delayMs);
    if (!RegGetValueA(HKEY_CURRENT_USER, SETTINGS_KEY_NAME, DELAYMS_KEY_NAME, RRF_RT_DWORD, nullptr, &delayMs, &delayMsSz)) {
        DEBOUNCE_THRESHOLD_MS = delayMs;
    } else {
        DEBOUNCE_THRESHOLD_MS = DEFAULT_DEBOUNCE_THRESHOLD_MS;
    }

    NotificationIcon ni;

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) MessageBoxA(nullptr, "Debounce failed to set process priority to high. You may experience additional input delay.", "Failed to set Priority", MB_ICONWARNING);
    const auto hHook = SetWindowsHookExA(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    if (!hHook) {
        MessageBoxA(nullptr, "Debounce failed to register hook.", "Hook failed", MB_ICONERROR);
        return -3;
    }

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0 && WM_QUIT != msg.message) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    UnhookWindowsHookEx(hHook);
    if (WM_QUIT == msg.message) {
        return static_cast<int>(msg.wParam);
    }
    else
    {
        return -4;
    }
}
