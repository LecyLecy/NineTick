#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#include "resource.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

namespace {
constexpr wchar_t kClassName[] = L"NineTickOverlay";
constexpr wchar_t kAppName[] = L"NineTick";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT_PTR kUpdateTimer = 1;
constexpr UINT kTrayId = 1;
constexpr int kOverlayWidth = 130;
constexpr int kOverlayHeight = 36;
constexpr int kEdgePadding = 12;

enum Command : UINT { kRestartTimer = 1001, kPositionTopLeft = 1011, kPositionTopRight = 1012,
                      kPositionBottomLeft = 1013, kPositionBottomRight = 1014, kToggleStartup = 1020, kQuit = 1030 };
enum class Position : DWORD { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 };

HINSTANCE gInstance = nullptr;
HWND gWindow = nullptr;
NOTIFYICONDATAW gTray{};
DWORD gStartedAt = 0;
Position gPosition = Position::BottomLeft;
wchar_t gTimeText[16] = L"00.00.00";

template <size_t N>
void CopyText(wchar_t (&destination)[N], const wchar_t* source) {
    wcsncpy(destination, source, N - 1);
    destination[N - 1] = L'\0';
}

void RegistrySetDword(const wchar_t* valueName, DWORD value) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\NineTick", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }
}

DWORD RegistryGetDword(const wchar_t* valueName, DWORD fallback) {
    HKEY key{};
    DWORD value = fallback, size = sizeof(value), type = REG_DWORD;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\NineTick", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        if (RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS || type != REG_DWORD) value = fallback;
        RegCloseKey(key);
    }
    return value;
}

bool IsStartupEnabled() {
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    const LONG result = RegQueryValueExW(key, kAppName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void SetStartupEnabled(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    if (enabled) {
        wchar_t exePath[MAX_PATH]{}, quotedPath[MAX_PATH + 4]{};
        GetModuleFileNameW(nullptr, exePath, COUNT_OF(exePath));
        lstrcpyW(quotedPath, L"\"");
        lstrcatW(quotedPath, exePath);
        lstrcatW(quotedPath, L"\"");
        RegSetValueExW(key, kAppName, 0, REG_SZ, reinterpret_cast<const BYTE*>(quotedPath), static_cast<DWORD>((wcslen(quotedPath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }
    RegCloseKey(key);
}

void MoveToSavedPosition() {
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = workArea.left + kEdgePadding;
    int y = workArea.bottom - kOverlayHeight - kEdgePadding;
    if (gPosition == Position::TopRight || gPosition == Position::BottomRight) x = workArea.right - kOverlayWidth - kEdgePadding;
    if (gPosition == Position::TopLeft || gPosition == Position::TopRight) y = workArea.top + kEdgePadding;
    SetWindowPos(gWindow, HWND_TOPMOST, x, y, kOverlayWidth, kOverlayHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ResetTimer() {
    gStartedAt = GetTickCount();
    CopyText(gTimeText, L"00.00.00");
    InvalidateRect(gWindow, nullptr, FALSE);
}

void UpdateTimerText() {
    const DWORD totalSeconds = (GetTickCount() - gStartedAt) / 1000;
    wchar_t updated[16]{};
    wsprintfW(updated, L"%02d.%02d.%02d", static_cast<int>(totalSeconds / 3600), static_cast<int>((totalSeconds / 60) % 60), static_cast<int>(totalSeconds % 60));
    if (wcscmp(updated, gTimeText) != 0) {
        CopyText(gTimeText, updated);
        InvalidateRect(gWindow, nullptr, FALSE);
    }
}

void AddTrayIcon(HWND window) {
    gTray = {};
    gTray.cbSize = sizeof(gTray);
    gTray.hWnd = window;
    gTray.uID = kTrayId;
    gTray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    gTray.uCallbackMessage = kTrayMessage;
    gTray.hIcon = LoadIconW(gInstance, MAKEINTRESOURCEW(IDI_NINETICK));
    CopyText(gTray.szTip, L"NineTick — running timer");
    Shell_NotifyIconW(NIM_ADD, &gTray);
}

void RemoveTrayIcon() { if (gTray.hWnd) Shell_NotifyIconW(NIM_DELETE, &gTray); }

void ShowTrayMenu(HWND window) {
    HMENU menu = CreatePopupMenu();
    HMENU positionMenu = CreatePopupMenu();
    const UINT checkedPosition = static_cast<UINT>(gPosition) + kPositionTopLeft;
    AppendMenuW(menu, MF_STRING, kRestartTimer, L"Restart timer");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(positionMenu, MF_STRING | (checkedPosition == kPositionTopLeft ? MF_CHECKED : 0), kPositionTopLeft, L"Top left");
    AppendMenuW(positionMenu, MF_STRING | (checkedPosition == kPositionTopRight ? MF_CHECKED : 0), kPositionTopRight, L"Top right");
    AppendMenuW(positionMenu, MF_STRING | (checkedPosition == kPositionBottomLeft ? MF_CHECKED : 0), kPositionBottomLeft, L"Bottom left");
    AppendMenuW(positionMenu, MF_STRING | (checkedPosition == kPositionBottomRight ? MF_CHECKED : 0), kPositionBottomRight, L"Bottom right");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(positionMenu), L"Timer position");
    AppendMenuW(menu, MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0), kToggleStartup, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kQuit, L"Quit NineTick");
    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, window, nullptr);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST: return HTCLIENT; // The overlay is intentionally not draggable.
    case WM_TIMER: if (wParam == kUpdateTimer) UpdateTimerText(); return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); RECT client{}; GetClientRect(window, &client);
        HBRUSH background = CreateSolidBrush(RGB(16, 24, 40)); FillRect(dc, &client, background); DeleteObject(background);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(245, 185, 66));
        HFONT font = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HGDIOBJ oldFont = SelectObject(dc, font); TextOutW(dc, 11, 7, gTimeText, static_cast<int>(wcslen(gTimeText))); SelectObject(dc, oldFont); DeleteObject(font);
        EndPaint(window, &paint); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kRestartTimer: ResetTimer(); break;
        case kPositionTopLeft: case kPositionTopRight: case kPositionBottomLeft: case kPositionBottomRight:
            gPosition = static_cast<Position>(LOWORD(wParam) - kPositionTopLeft); RegistrySetDword(L"Position", static_cast<DWORD>(gPosition)); MoveToSavedPosition(); break;
        case kToggleStartup: SetStartupEnabled(!IsStartupEnabled()); break;
        case kQuit: DestroyWindow(window); break;
        } return 0;
    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_LBUTTONUP) ShowTrayMenu(window);
        return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: MoveToSavedPosition(); return 0;
    case WM_DESTROY: KillTimer(window, kUpdateTimer); RemoveTrayIcon(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    gInstance = instance;
    HANDLE instanceLock = CreateMutexW(nullptr, TRUE, L"Local\\NineTickSingleInstance");
    if (!instanceLock || GetLastError() == ERROR_ALREADY_EXISTS) { if (instanceLock) CloseHandle(instanceLock); return 0; }
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass); windowClass.lpfnWndProc = WindowProc; windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_NINETICK)); windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW); windowClass.lpszClassName = kClassName;
    if (!RegisterClassExW(&windowClass)) return 1;
    gPosition = static_cast<Position>(RegistryGetDword(L"Position", static_cast<DWORD>(Position::BottomLeft)));
    if (static_cast<DWORD>(gPosition) > static_cast<DWORD>(Position::BottomRight)) gPosition = Position::BottomLeft;
    gStartedAt = GetTickCount();
    gWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, kClassName, kAppName, WS_POPUP, 0, 0, kOverlayWidth, kOverlayHeight, nullptr, nullptr, instance, nullptr);
    if (!gWindow) return 1;
    AddTrayIcon(gWindow); MoveToSavedPosition(); SetTimer(gWindow, kUpdateTimer, 250, nullptr);
    if (!IsStartupEnabled()) SetStartupEnabled(true); // Enabled by default; tray menu can disable it.
    MSG message{}; while (GetMessageW(&message, nullptr, 0, 0)) { TranslateMessage(&message); DispatchMessageW(&message); }
    CloseHandle(instanceLock); return 0;
}
