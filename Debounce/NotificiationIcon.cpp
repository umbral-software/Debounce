#include "NotificiationIcon.hpp"

#include <shellapi.h>

#include <array>
#include <string>

// Note: this is stashed in a BOOL (i.e. int32_t) when returning from TrackPopupMenu despite being a UINT_PTR in InsertMenu{,Item}
// If using the WM_COMMAND message, it's smuggled into a single WORD (i.e. uint16_t)
static constexpr WORD MENU_CLOSE_ID = 0xDEAD;
static const LPSTR DDORES_MOOUSE_ICON_ID = MAKEINTRESOURCEA(2212);

static constexpr auto DELAY_VALUES_MS = std::array{
    1u,
    3u,
    5u,
    10u,
    15u,
    20u,
    25u,
    50u,
    100u
};

HINSTANCE NotificationIcon::_classHInstance;
bool NotificationIcon::_is_rtl;
ATOM NotificationIcon::_classAtom;
HMODULE NotificationIcon::_resourceDLL;
HMENU NotificationIcon::_hMenu, NotificationIcon::_hSubMenu;
std::once_flag NotificationIcon::_classInitFlag;

NotificationIcon::NotificationIcon()
{
    std::call_once(_classInitFlag, NotificationIcon::InitClass);

    _hWnd = CreateWindowExA(0, reinterpret_cast<LPCSTR>(_classAtom), "Debounce Message-Only Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, nullptr, _classHInstance, nullptr);

    const NOTIFYICONDATAA notifyIconData{
        .cbSize = sizeof(NOTIFYICONDATAA),
        .hWnd = _hWnd,
        .uID = 0,
        .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
        .uCallbackMessage = WM_USER,
        .hIcon = _mouseIcon,
        .szTip = "Debounce is running...",
        .uVersion = NOTIFYICON_VERSION_4,
    };
    Shell_NotifyIconA(NIM_ADD, const_cast<PNOTIFYICONDATAA>(&notifyIconData));
    Shell_NotifyIconA(NIM_SETVERSION, const_cast<PNOTIFYICONDATAA>(&notifyIconData));
}

NotificationIcon::NotificationIcon(NotificationIcon&& other) noexcept
{
    _hWnd = other._hWnd;
    other._hWnd = nullptr;

    _hMenu = other._hMenu;
    other._hMenu = nullptr;

    _hSubMenu = other._hSubMenu;
    other._hSubMenu = nullptr;
}

NotificationIcon::~NotificationIcon()
{
    if (_hMenu) DestroyMenu(_hMenu);
    if (_hSubMenu) DestroyMenu(_hSubMenu);

    if (_hWnd) {
        const NOTIFYICONDATAA notifyIconData{
            .cbSize = sizeof(NOTIFYICONDATAA),
            .hWnd = _hWnd,
            .uID = 0,
        };
        Shell_NotifyIconA(NIM_DELETE, const_cast<PNOTIFYICONDATAA>(&notifyIconData));
        DestroyWindow(_hWnd);
    }
}

NotificationIcon& NotificationIcon::operator=(NotificationIcon&& other) noexcept
{
    _hWnd = other._hWnd;
    other._hWnd = nullptr;

    _hMenu = other._hMenu;
    other._hMenu = nullptr;

    _hSubMenu = other._hSubMenu;
    other._hSubMenu = nullptr;

    return *this;
}

HWND NotificationIcon::window() const noexcept {
    return _hWnd;
}

void NotificationIcon::InitClass()
{
    _classHInstance = GetModuleHandleA(nullptr);
    _is_rtl = GetSystemMetrics(SM_MENUDROPALIGNMENT);

    const WNDCLASSEXA wndClass{
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = NotificationIcon::WindowProc,
        .hInstance = _classHInstance,
        .lpszClassName = "Debounce"
    };
    _classAtom = RegisterClassExA(&wndClass);

    _resourceDLL = LoadLibraryExA("Ddores.dll", nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_SEARCH_SYSTEM32);
    _mouseIcon = static_cast<HICON>(LoadImageA(_resourceDLL, DDORES_MOOUSE_ICON_ID, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED)),

    _hSubMenu = CreatePopupMenu();
    for (const auto delayMs : DELAY_VALUES_MS) {
        const std::string text = std::to_string(delayMs) + "ms";

        const MENUITEMINFOA mi = {
            .cbSize = sizeof(MENUITEMINFOA),
            .fMask = MIIM_FTYPE | MIIM_STRING | MIIM_CHECKMARKS | MIIM_ID,
            .fType = static_cast<UINT>(MFT_RADIOCHECK | (_is_rtl ? MFT_RIGHTORDER : 0)),
            .wID = delayMs,
            .dwTypeData = const_cast<LPSTR>(text.c_str())
        };
        InsertMenuItemA(_hSubMenu, 0, true, &mi);
    }

    _hMenu = CreatePopupMenu();
    InsertMenuA(_hMenu, 0, MF_BYPOSITION | MF_STRING, MENU_CLOSE_ID, "Close");
    InsertMenuA(_hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuA(_hMenu, 0, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(_hSubMenu), "Delay");
}

LRESULT CALLBACK NotificationIcon::WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg) {
    case WM_USER:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
            for (const auto delayMs : DELAY_VALUES_MS) {
                const MENUITEMINFOA mi = {
                    .cbSize = sizeof(MENUITEMINFOA),
                    .fMask = MIIM_STATE,
                    .fState = static_cast<UINT>(
                        (DEBOUNCE_THRESHOLD_MS == delayMs ? MFS_CHECKED | MFS_DISABLED : 0)
                        | (DEFAULT_DEBOUNCE_THRESHOLD_MS == delayMs ? MFS_DEFAULT : 0))
                };
                SetMenuItemInfoA(_hSubMenu, delayMs, false, &mi);
            }

            SetForegroundWindow(hWnd);
            const auto result = TrackPopupMenu(_hMenu,
                TPM_NONOTIFY | TPM_RETURNCMD | (_is_rtl ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION),
                LOWORD(wParam), HIWORD(wParam),
                0, hWnd, nullptr);

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