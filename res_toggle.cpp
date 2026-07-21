// res_toggle.cpp — tiny Windows system-tray app that toggles the primary
// display between the current resolution and a chosen resolution.
//
// Left-click the tray icon  -> toggle between chosen resolution and the
//                              resolution that was active at startup.
// Right-click the tray icon -> menu to pick a resolution as the "chosen"
//                              one, or quit.
//
// The chosen resolution is persisted to res_toggle.ini next to the exe.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <algorithm>

#include "resource.h"

#pragma comment(lib, "Shlwapi.lib")

// ---------------------------------------------------------------------------
// Constants / globals
// ---------------------------------------------------------------------------
static const wchar_t* kWndClass = L"ResToggleHiddenWindow";
static const UINT     WM_TRAYICON = WM_APP + 1;
static const UINT     kTrayId = 1;

// Menu command id ranges.
static const UINT IDM_QUIT = 1000;
static const UINT IDM_TOGGLE = 1001;
static const UINT IDM_ABOUT = 1002;
static const UINT IDM_MODE_FIRST = 2000;  // resolution list starts here

struct Resolution {
    DWORD w = 0;
    DWORD h = 0;
    bool operator==(const Resolution& o) const { return w == o.w && h == o.h; }
};

static HWND        g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HICON       g_iconA = nullptr;   // icon shown while on the "base" res
static HICON       g_iconB = nullptr;   // icon shown while on the "chosen" res

static Resolution  g_baseRes;    // resolution captured at startup
static Resolution  g_chosenRes;  // the target we toggle to
static std::vector<Resolution> g_available;  // distinct modes for the menu

static wchar_t g_iniPath[MAX_PATH] = {};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static Resolution GetCurrentResolution() {
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    Resolution r;
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
        r.w = dm.dmPelsWidth;
        r.h = dm.dmPelsHeight;
    }
    return r;
}

// Enumerate distinct width x height modes supported by the primary display.
static void EnumerateModes() {
    g_available.clear();
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    for (int i = 0; EnumDisplaySettingsW(nullptr, i, &dm); ++i) {
        Resolution r{ dm.dmPelsWidth, dm.dmPelsHeight };
        if (std::find(g_available.begin(), g_available.end(), r) == g_available.end())
            g_available.push_back(r);
    }
    std::sort(g_available.begin(), g_available.end(),
              [](const Resolution& a, const Resolution& b) {
                  if (a.w != b.w) return a.w > b.w;
                  return a.h > b.h;
              });
}

// Apply a resolution to the primary display. Returns true on success.
static bool ApplyResolution(const Resolution& r) {
    if (r.w == 0 || r.h == 0) return false;

    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = r.w;
    dm.dmPelsHeight = r.h;
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    LONG rc = ChangeDisplaySettingsW(&dm, CDS_UPDATEREGISTRY);
    return rc == DISP_CHANGE_SUCCESSFUL;
}

static void ResolveIniPath() {
    GetModuleFileNameW(nullptr, g_iniPath, MAX_PATH);
    PathRemoveFileSpecW(g_iniPath);
    PathAppendW(g_iniPath, L"res_toggle.ini");
}

static void SaveChosen() {
    wchar_t buf[64];
    wsprintfW(buf, L"%lu", g_chosenRes.w);
    WritePrivateProfileStringW(L"chosen", L"width", buf, g_iniPath);
    wsprintfW(buf, L"%lu", g_chosenRes.h);
    WritePrivateProfileStringW(L"chosen", L"height", buf, g_iniPath);
}

static void LoadChosen() {
    DWORD w = GetPrivateProfileIntW(L"chosen", L"width", 0, g_iniPath);
    DWORD h = GetPrivateProfileIntW(L"chosen", L"height", 0, g_iniPath);
    g_chosenRes = { w, h };
}

// ---------------------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------------------
static bool CurrentlyOnChosen() {
    return g_chosenRes.w != 0 && GetCurrentResolution() == g_chosenRes;
}

static void UpdateTrayTip() {
    Resolution cur = GetCurrentResolution();
    HICON icon = CurrentlyOnChosen() ? g_iconB : g_iconA;
    g_nid.hIcon = icon;

    if (g_chosenRes.w == 0) {
        wsprintfW(g_nid.szTip,
                  L"Res Toggle — %lux%lu\nRight-click to pick a target",
                  cur.w, cur.h);
    } else {
        wsprintfW(g_nid.szTip,
                  L"Res Toggle — now %lux%lu\nClick: toggle %lux%lu <-> %lux%lu",
                  cur.w, cur.h,
                  g_baseRes.w, g_baseRes.h,
                  g_chosenRes.w, g_chosenRes.h);
    }
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AddTrayIcon() {
    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = kTrayId;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_iconA;
    lstrcpynW(g_nid.szTip, L"Res Toggle", ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    UpdateTrayTip();
}

static void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

// ---------------------------------------------------------------------------
// Core action
// ---------------------------------------------------------------------------
static void Toggle() {
    if (g_chosenRes.w == 0) {
        MessageBoxW(g_hwnd,
                    L"No target resolution chosen yet.\n\n"
                    L"Right-click the tray icon and pick one first.",
                    L"Res Toggle", MB_OK | MB_ICONINFORMATION);
        return;
    }

    Resolution target = CurrentlyOnChosen() ? g_baseRes : g_chosenRes;
    if (!ApplyResolution(target)) {
        wchar_t msg[128];
        wsprintfW(msg, L"Could not switch to %lux%lu.", target.w, target.h);
        MessageBoxW(g_hwnd, msg, L"Res Toggle", MB_OK | MB_ICONERROR);
    }
    UpdateTrayTip();
}

static void ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_TOGGLE, L"Toggle now");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Submenu of available resolutions to choose the target.
    HMENU sub = CreatePopupMenu();
    for (size_t i = 0; i < g_available.size(); ++i) {
        const Resolution& r = g_available[i];
        wchar_t label[64];
        wsprintfW(label, L"%lu x %lu", r.w, r.h);
        UINT flags = MF_STRING;
        if (r == g_chosenRes) flags |= MF_CHECKED;
        AppendMenuW(sub, flags, IDM_MODE_FIRST + (UINT)i, label);
    }
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, L"Set target resolution");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit");

    // Required so the menu closes if the user clicks elsewhere.
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                              pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(sub);
    DestroyMenu(menu);

    if (cmd == IDM_QUIT) {
        DestroyWindow(g_hwnd);
    } else if (cmd == IDM_TOGGLE) {
        Toggle();
    } else if (cmd == IDM_ABOUT) {
        MessageBoxW(g_hwnd,
                    L"Res Toggle\n\n"
                    L"Left-click the tray icon to toggle between your target "
                    L"resolution and the resolution active at launch.\n\n"
                    L"Right-click to choose a target resolution.",
                    L"About Res Toggle", MB_OK | MB_ICONINFORMATION);
    } else if (cmd >= IDM_MODE_FIRST &&
               cmd < IDM_MODE_FIRST + g_available.size()) {
        g_chosenRes = g_available[cmd - IDM_MODE_FIRST];
        // Re-capture the base as the resolution active right now, so the
        // toggle is meaningful even if the user picked the current res.
        g_baseRes = GetCurrentResolution();
        if (g_baseRes == g_chosenRes) {
            // Fall back to something distinct so toggling has an effect.
            for (const Resolution& r : g_available) {
                if (!(r == g_chosenRes)) { g_baseRes = r; break; }
            }
        }
        SaveChosen();
        UpdateTrayTip();
    }
}

// ---------------------------------------------------------------------------
// Window proc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_LBUTTONUP) {
                Toggle();
            } else if (LOWORD(lp) == WM_RBUTTONUP ||
                       LOWORD(lp) == WM_CONTEXTMENU) {
                ShowContextMenu();
            }
            return 0;

        case WM_DISPLAYCHANGE:
            UpdateTrayTip();
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Single-instance guard so the tray doesn't fill up with duplicates.
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"ResToggleSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    ResolveIniPath();
    EnumerateModes();
    g_baseRes = GetCurrentResolution();
    LoadChosen();

    // Icons: two from resources so we can visually distinguish states.
    g_iconA = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_ICON_BASE));
    g_iconB = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_ICON_CHOSEN));
    if (!g_iconA) g_iconA = LoadIconW(nullptr, IDI_APPLICATION);
    if (!g_iconB) g_iconB = g_iconA;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kWndClass;
    RegisterClassW(&wc);

    // Message-only window: no taskbar button, just an event sink.
    g_hwnd = CreateWindowExW(0, kWndClass, L"Res Toggle",
                             0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;

    AddTrayIcon();

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (mtx) { ReleaseMutex(mtx); CloseHandle(mtx); }
    return 0;
}
