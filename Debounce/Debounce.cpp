#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <string>

// Note: this is stashed in a BOOL (i.e. int32_t) when returning from TrackPopupMenu despite being a UINT_PTR in InsertMenu{,Item}
// If using the WM_COMMAND message, it's smuggled into a single WORD (i.e. uint16_t)
static constexpr WORD MENU_CLOSE_ID = 0xDEAD;
static constexpr DWORD DEFAULT_DEBOUNCE_THRESHOLD_MS = 10;

// LowLevelMouseProc has no way to stash a user data pointer with it. So these have to be global.
// MSLLHOOKSTRUCT::dwExtraInfo is for device-specific info, not user data afaict.
static DWORD DEBOUNCE_THRESHOLD_MS = DEFAULT_DEBOUNCE_THRESHOLD_MS;
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

static BOOL InsertDelayMenuOption(HMENU hMenu, UINT delayMs, BOOL rtl) {
    const std::string text = std::to_string(delayMs) + "ms";

    const MENUITEMINFOA mi = {
        .cbSize = sizeof(MENUITEMINFOA),
        .fMask = MIIM_FTYPE | MIIM_STRING | MIIM_CHECKMARKS | MIIM_STATE | MIIM_ID,
        .fType = static_cast<UINT>(MFT_RADIOCHECK | (rtl ? MFT_RIGHTORDER : 0)),
        .fState = static_cast<UINT>(
            (DEBOUNCE_THRESHOLD_MS == delayMs ? MFS_CHECKED | MFS_DISABLED : 0)
            | (DEFAULT_DEBOUNCE_THRESHOLD_MS == delayMs ? MFS_DEFAULT : 0)),
        .wID = delayMs,
        .dwTypeData = const_cast<LPSTR>(text.c_str())
    };
    return InsertMenuItemA(hMenu, 0, true, &mi);
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg) {
    case WM_USER:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
            const auto is_rtl = GetSystemMetrics(SM_MENUDROPALIGNMENT);

            const auto hSubmenu = CreatePopupMenu();
            InsertDelayMenuOption(hSubmenu, 1, is_rtl);
            InsertDelayMenuOption(hSubmenu, 3, is_rtl);
            InsertDelayMenuOption(hSubmenu, 5, is_rtl);
            InsertDelayMenuOption(hSubmenu, 10, is_rtl);
            InsertDelayMenuOption(hSubmenu, 15, is_rtl);
            InsertDelayMenuOption(hSubmenu, 20, is_rtl);
            InsertDelayMenuOption(hSubmenu, 25, is_rtl);
            InsertDelayMenuOption(hSubmenu, 50, is_rtl);
            InsertDelayMenuOption(hSubmenu, 100, is_rtl);

            const auto hMenu = CreatePopupMenu();
            InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_STRING, MENU_CLOSE_ID, "Close");
            InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
            InsertMenuA(hMenu, 0, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(hSubmenu), "Delay");

            SetForegroundWindow(hWnd);
            const auto result = TrackPopupMenu(hMenu,
                TPM_NONOTIFY | TPM_RETURNCMD| (is_rtl ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION),
                LOWORD(wParam), HIWORD(wParam),
                0, hWnd, nullptr);

            DestroyMenu(hMenu);
            DestroyMenu(hSubmenu);

            if (MENU_CLOSE_ID == result) {
                PostQuitMessage(0);
            }
            else if (result > 0) {
                DEBOUNCE_THRESHOLD_MS = result;
            }
            break;
        }
        break;
    }
    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

_Use_decl_annotations_
int wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    const WNDCLASSEXA wndClass{
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = WindowProc,
        .hInstance = hInstance,
        .lpszClassName = "Debounce"
    };
    const auto wndClassAtom = RegisterClassExA(&wndClass);
    const auto hwnd = CreateWindowExA(0, reinterpret_cast<LPCSTR>(wndClassAtom), "Debounce Message-Only Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, nullptr, hInstance, nullptr);

    const auto mutex = CreateMutexExA(nullptr, "Local\\uk.umbral.debounce", CREATE_MUTEX_INITIAL_OWNER, SYNCHRONIZE);
    if (!mutex) {
        MessageBoxA(hwnd, "Could not determine if another instance of Debounce is already running.", "Could not create mutex", MB_ICONERROR);
        return -1;
    }

    switch (WaitForSingleObject(mutex, 0)) {
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
        break;
    case WAIT_TIMEOUT:
        MessageBoxA(hwnd, "An existing instance of Debounce is already running", "Duplicate Instance", MB_ICONWARNING);
        return 1;
    default:
        MessageBoxA(hwnd, "Could not determine if another instance of Debounce is already running.", "Could not wait for mutex", MB_ICONERROR);
        return -2;
    }

    const auto resourceDLL = LoadLibraryExA("Ddores.dll", nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_SEARCH_SYSTEM32);
    
    const NOTIFYICONDATAA notifyIconData{
        .cbSize = sizeof(NOTIFYICONDATAA),
        .hWnd = hwnd,
        .uID = 0,
        .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
        .uCallbackMessage = WM_USER,
        .hIcon = static_cast<HICON>(LoadImageA(resourceDLL, MAKEINTRESOURCEA(2212), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED)),
        .szTip = "Debounce is running...",
        .uVersion = NOTIFYICON_VERSION_4,
    };
    Shell_NotifyIconA(NIM_ADD, const_cast<PNOTIFYICONDATAA>(&notifyIconData));
    Shell_NotifyIconA(NIM_SETVERSION, const_cast<PNOTIFYICONDATAA>(&notifyIconData));

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) MessageBoxA(hwnd, "Debounce failed to set process priority to high. You may experience additional input delay.", "Failed to set Priority", MB_ICONWARNING);
    if (!SetWindowsHookExA(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0)) {
        MessageBoxA(hwnd, "Debounce failed to register hook.", "Hook failed", MB_ICONERROR);
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
