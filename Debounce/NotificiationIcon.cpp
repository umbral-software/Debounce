#include "NotificiationIcon.hpp"

#include "Debounce.hpp"

#include <shellapi.h>

#include <array>
#include <cassert>
#include <mutex>
#include <string>

// Note: this is stashed in a BOOL (i.e. int32_t) when returning from TrackPopupMenu despite being a UINT_PTR in InsertMenu{,Item}
// If using the WM_COMMAND message, it's smuggled into a single WORD (i.e. uint16_t)
static constexpr WORD MENU_CLOSE_ID = 0xDEAD;
static const LPSTR DDORES_MOOUSE_ICON_ID = MAKEINTRESOURCEA(2212);

static std::once_flag CLASS_INIT_FLAG;

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

class NotificationIconClass {
    friend class NotificationIcon;

public:
    NotificationIconClass();
    NotificationIconClass(const NotificationIconClass&) = delete;
    NotificationIconClass(NotificationIconClass&& other) noexcept = delete;
    ~NotificationIconClass();

    NotificationIconClass& operator=(const NotificationIconClass&) = delete;
    NotificationIconClass& operator=(NotificationIconClass&& other) noexcept = delete;

private:
    void init();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInstance;
    bool is_rtl;
    ATOM atom;
    HICON mouseIcon;
    HMENU hMenu, hSubMenu;
} NotificationIcon::_class;

NotificationIconClass::NotificationIconClass()
    :hInstance(nullptr)
    ,is_rtl(false)
    ,atom(INVALID_ATOM)
    ,mouseIcon(nullptr)
    ,hMenu(nullptr), hSubMenu(nullptr)
{

}

NotificationIconClass::~NotificationIconClass() {
    if (hSubMenu) DestroyMenu(hSubMenu);
    if (hMenu) DestroyMenu(hMenu);
    if (mouseIcon) DestroyIcon(mouseIcon);
    if (atom) UnregisterClassA(reinterpret_cast<LPCSTR>(atom), hInstance);
}

void NotificationIconClass::init()
{
    hInstance = GetModuleHandleA(nullptr);
    is_rtl = GetSystemMetrics(SM_MENUDROPALIGNMENT);

    const WNDCLASSEXA wndClass{
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = NotificationIconClass::WindowProc,
        .hInstance = hInstance,
        .lpszClassName = "Debounce"
    };
    atom = RegisterClassExA(&wndClass);

    const auto resourceDLL = LoadLibraryExA("Ddores.dll", nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_SEARCH_SYSTEM32);
    assert(resourceDLL);

    mouseIcon = static_cast<HICON>(LoadImageA(resourceDLL, DDORES_MOOUSE_ICON_ID, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    FreeLibrary(resourceDLL);

    hSubMenu = CreatePopupMenu();
    for (const auto delayMs : DELAY_VALUES_MS) {
        const std::string text = std::to_string(delayMs) + "ms";

        const MENUITEMINFOA mi = {
            .cbSize = sizeof(MENUITEMINFOA),
            .fMask = MIIM_FTYPE | MIIM_STRING | MIIM_CHECKMARKS | MIIM_ID,
            .fType = static_cast<UINT>(MFT_RADIOCHECK | (is_rtl ? MFT_RIGHTORDER : 0)),
            .wID = delayMs,
            .dwTypeData = const_cast<LPSTR>(text.c_str())
        };
        InsertMenuItemA(hSubMenu, static_cast<UINT>(-1), false, &mi);
    }

    hMenu = CreatePopupMenu();
    InsertMenuA(hMenu, static_cast<UINT>(-1), MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu), "Delay");
    InsertMenuA(hMenu, static_cast<UINT>(-1), MF_SEPARATOR, 0, nullptr);
    const MENUITEMINFOA mi = {
        .cbSize = sizeof(MENUITEMINFOA),
        .fMask = MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING | MIIM_ID,
        .fType = static_cast<UINT>(is_rtl ? MFT_RIGHTORDER : 0),
        .wID = MENU_CLOSE_ID,
        .dwTypeData = const_cast<LPSTR>("Close"),
        .hbmpItem = HBMMENU_MBAR_CLOSE
    };
    InsertMenuItemA(hMenu, static_cast<UINT>(-1), false, &mi);
}

LRESULT CALLBACK NotificationIconClass::WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<NotificationIconClass*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));

    switch (Msg) {
    case WM_CREATE: {
        const auto createParams = reinterpret_cast<LPCREATESTRUCTA>(lParam);
        SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createParams->lpCreateParams));
        break;
    }
    case WM_USER:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
            const auto debounceDelayMs = GetDebounceDelay();
            for (const auto delayMs : DELAY_VALUES_MS) {
                const MENUITEMINFOA mi = {
                    .cbSize = sizeof(MENUITEMINFOA),
                    .fMask = MIIM_STATE,
                    .fState = static_cast<UINT>(
                        (debounceDelayMs == delayMs ? MFS_CHECKED | MFS_DISABLED : 0)
                        | (DEFAULT_DEBOUNCE_THRESHOLD_MS == delayMs ? MFS_DEFAULT : 0))
                };
                SetMenuItemInfoA(self->hSubMenu, delayMs, false, &mi);
            }

            SetForegroundWindow(hWnd);
            const auto result = TrackPopupMenu(self->hMenu,
                TPM_NONOTIFY | TPM_RETURNCMD | (self->is_rtl ? TPM_RIGHTALIGN | TPM_HORNEGANIMATION : TPM_LEFTALIGN | TPM_HORPOSANIMATION),
                LOWORD(wParam), HIWORD(wParam),
                0, hWnd, nullptr);

            if (MENU_CLOSE_ID == result) {
                PostQuitMessage(0);
            }
            else if (result > 0) {
                SetDebounceDelay(result);
            }
            break;
        }
        break;
    }
    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

NotificationIcon::NotificationIcon()
{
    std::call_once(CLASS_INIT_FLAG, &NotificationIconClass::init, _class);

    _hWnd = CreateWindowExA(0, reinterpret_cast<LPCSTR>(_class.atom), "Debounce Message-Only Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, nullptr, _class.hInstance, &_class);

    const NOTIFYICONDATAA notifyIconData{
        .cbSize = sizeof(NOTIFYICONDATAA),
        .hWnd = _hWnd,
        .uID = 0,
        .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
        .uCallbackMessage = WM_USER,
        .hIcon = _class.mouseIcon,
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
}

NotificationIcon::~NotificationIcon()
{
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

    return *this;
}

HWND NotificationIcon::window() const noexcept {
    return _hWnd;
}
