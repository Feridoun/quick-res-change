// res_toggle.cpp — tiny Windows system-tray app that switches the primary
// display between a usual resolution, a secondary one and an optional third.
//
// Left-click the tray icon  -> cycle usual -> secondary -> (third) -> usual.
// Right-click the tray icon -> menu with Settings (pick the resolutions from
//                              drop-downs), About and Quit.
//
// Settings are persisted to res_toggle.ini next to the exe.

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
// Modern (themed) look for the settings dialog's controls.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---------------------------------------------------------------------------
// Constants / globals
// ---------------------------------------------------------------------------
static const wchar_t* kWndClass = L"ResToggleHiddenWindow";
static const UINT     WM_TRAYICON = WM_APP + 1;
static const UINT     kTrayId = 1;

// Menu command ids.
static const UINT IDM_QUIT = 1000;
static const UINT IDM_TOGGLE = 1001;
static const UINT IDM_ABOUT = 1002;
static const UINT IDM_SETTINGS = 1003;

struct Resolution {
    DWORD w = 0;
    DWORD h = 0;
    bool operator==(const Resolution& o) const { return w == o.w && h == o.h; }
};

static HWND        g_hwnd = nullptr;
static HINSTANCE   g_hinst = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HICON       g_iconA = nullptr;   // icon shown while on the usual res
static HICON       g_iconB = nullptr;   // icon shown while on any other res

static Resolution  g_primaryRes;     // the usual resolution
static Resolution  g_secondaryRes;   // the one we normally switch to
static Resolution  g_thirdRes;       // optional extra stop in the cycle
static bool        g_useThird = false;
static bool        g_settingsOpen = false;
static std::vector<Resolution> g_available;  // distinct modes for the dialog

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

static void SaveRes(const wchar_t* key, const Resolution& r) {
    wchar_t buf[64];
    wsprintfW(buf, L"%lux%lu", r.w, r.h);
    WritePrivateProfileStringW(L"settings", key, buf, g_iniPath);
}

static Resolution LoadRes(const wchar_t* key) {
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"settings", key, L"", buf, ARRAYSIZE(buf), g_iniPath);
    Resolution r;
    wchar_t* x = wcschr(buf, L'x');
    if (x) {
        r.w = wcstoul(buf, nullptr, 10);
        r.h = wcstoul(x + 1, nullptr, 10);
    }
    return r;
}

static void SaveSettings() {
    SaveRes(L"primary", g_primaryRes);
    SaveRes(L"secondary", g_secondaryRes);
    SaveRes(L"third", g_thirdRes);
    WritePrivateProfileStringW(L"settings", L"use_third",
                               g_useThird ? L"1" : L"0", g_iniPath);
}

static void LoadSettings() {
    g_primaryRes = LoadRes(L"primary");
    g_secondaryRes = LoadRes(L"secondary");
    g_thirdRes = LoadRes(L"third");
    g_useThird = GetPrivateProfileIntW(L"settings", L"use_third", 0, g_iniPath) != 0;

    // Migrate the old single-target format: [chosen] width/height.
    if (g_secondaryRes.w == 0) {
        g_secondaryRes.w = GetPrivateProfileIntW(L"chosen", L"width", 0, g_iniPath);
        g_secondaryRes.h = GetPrivateProfileIntW(L"chosen", L"height", 0, g_iniPath);
    }
    // First run: assume the resolution we launched at is the usual one.
    if (g_primaryRes.w == 0) g_primaryRes = GetCurrentResolution();
    if (g_secondaryRes == g_primaryRes) g_secondaryRes = {};
}

static bool IsConfigured() {
    return g_primaryRes.w != 0 && g_secondaryRes.w != 0;
}

// The resolutions left-click cycles through, in order.
static std::vector<Resolution> CycleList() {
    std::vector<Resolution> list{ g_primaryRes, g_secondaryRes };
    if (g_useThird && g_thirdRes.w != 0) list.push_back(g_thirdRes);
    return list;
}

// ---------------------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------------------
static void UpdateTrayTip() {
    Resolution cur = GetCurrentResolution();
    g_nid.hIcon = (cur == g_primaryRes) ? g_iconA : g_iconB;

    if (!IsConfigured()) {
        wsprintfW(g_nid.szTip,
                  L"Res Toggle — %lux%lu\nRight-click > Settings to set up",
                  cur.w, cur.h);
    } else if (g_useThird && g_thirdRes.w != 0) {
        wsprintfW(g_nid.szTip,
                  L"Res Toggle — now %lux%lu\nClick: %lux%lu > %lux%lu > %lux%lu",
                  cur.w, cur.h,
                  g_primaryRes.w, g_primaryRes.h,
                  g_secondaryRes.w, g_secondaryRes.h,
                  g_thirdRes.w, g_thirdRes.h);
    } else {
        wsprintfW(g_nid.szTip,
                  L"Res Toggle — now %lux%lu\nClick: toggle %lux%lu <-> %lux%lu",
                  cur.w, cur.h,
                  g_primaryRes.w, g_primaryRes.h,
                  g_secondaryRes.w, g_secondaryRes.h);
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
// Settings dialog
// ---------------------------------------------------------------------------
static void FillCombo(HWND dlg, int id, const Resolution& sel) {
    HWND combo = GetDlgItem(dlg, id);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selIdx = -1;
    for (size_t i = 0; i < g_available.size(); ++i) {
        const Resolution& r = g_available[i];
        wchar_t label[64];
        wsprintfW(label, L"%lu x %lu", r.w, r.h);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label);
        if (r == sel) selIdx = (int)i;
    }
    SendMessageW(combo, CB_SETCURSEL, selIdx, 0);
}

// Returns the selected resolution, or a zero one if nothing is selected.
static Resolution ComboSelection(HWND dlg, int id) {
    LRESULT idx = SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0);
    if (idx == CB_ERR || (size_t)idx >= g_available.size()) return {};
    return g_available[idx];
}

static INT_PTR CALLBACK SettingsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG:
            SendMessageW(dlg, WM_SETICON, ICON_BIG, (LPARAM)g_iconB);
            SendMessageW(dlg, WM_SETICON, ICON_SMALL, (LPARAM)g_iconB);
            FillCombo(dlg, IDC_PRIMARY, g_primaryRes);
            FillCombo(dlg, IDC_SECONDARY, g_secondaryRes);
            FillCombo(dlg, IDC_THIRD, g_thirdRes);
            CheckDlgButton(dlg, IDC_USE_THIRD, g_useThird ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(dlg, IDC_THIRD), g_useThird);
            SetForegroundWindow(dlg);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_USE_THIRD:
                    EnableWindow(GetDlgItem(dlg, IDC_THIRD),
                                 IsDlgButtonChecked(dlg, IDC_USE_THIRD) == BST_CHECKED);
                    return TRUE;

                case IDOK: {
                    Resolution p = ComboSelection(dlg, IDC_PRIMARY);
                    Resolution s = ComboSelection(dlg, IDC_SECONDARY);
                    Resolution t = ComboSelection(dlg, IDC_THIRD);
                    bool useThird = IsDlgButtonChecked(dlg, IDC_USE_THIRD) == BST_CHECKED;

                    const wchar_t* err = nullptr;
                    if (p.w == 0 || s.w == 0)
                        err = L"Please choose both a usual and a secondary resolution.";
                    else if (p == s)
                        err = L"The secondary resolution must differ from the usual one.";
                    else if (useThird && t.w == 0)
                        err = L"Please choose a third resolution, or untick the box.";
                    else if (useThird && (t == p || t == s))
                        err = L"The third resolution must differ from the other two.";
                    if (err) {
                        MessageBoxW(dlg, err, L"Res Toggle", MB_OK | MB_ICONWARNING);
                        return TRUE;
                    }

                    g_primaryRes = p;
                    g_secondaryRes = s;
                    if (t.w != 0) g_thirdRes = t;  // keep the old pick if left blank
                    g_useThird = useThird;
                    SaveSettings();
                    EndDialog(dlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(dlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

static void ShowSettings() {
    if (g_settingsOpen) return;  // the tray still gets clicks while it's up
    g_settingsOpen = true;
    EnumerateModes();  // pick up any change in the display's supported modes
    DialogBoxParamW(g_hinst, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr,
                    SettingsProc, 0);
    g_settingsOpen = false;
    UpdateTrayTip();
}

// ---------------------------------------------------------------------------
// Core action
// ---------------------------------------------------------------------------
static void Toggle() {
    if (!IsConfigured()) {
        ShowSettings();
        return;
    }

    // Advance to the next entry in the cycle; if the current resolution
    // isn't one of ours, go back to the usual one.
    std::vector<Resolution> list = CycleList();
    auto it = std::find(list.begin(), list.end(), GetCurrentResolution());
    Resolution target = (it == list.end())
                            ? list[0]
                            : list[(size_t)(it - list.begin() + 1) % list.size()];
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
    AppendMenuW(menu, MF_STRING, IDM_TOGGLE, L"Switch now");
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit");

    // Required so the menu closes if the user clicks elsewhere.
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                              pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(menu);

    if (cmd == IDM_QUIT) {
        DestroyWindow(g_hwnd);
    } else if (cmd == IDM_TOGGLE) {
        Toggle();
    } else if (cmd == IDM_SETTINGS) {
        ShowSettings();
    } else if (cmd == IDM_ABOUT) {
        MessageBoxW(g_hwnd,
                    L"Res Toggle\n\n"
                    L"Left-click the tray icon to cycle between your usual "
                    L"resolution, your secondary one and (optionally) a third.\n\n"
                    L"Right-click > Settings to choose them.",
                    L"About Res Toggle", MB_OK | MB_ICONINFORMATION);
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

    g_hinst = hInst;
    ResolveIniPath();
    EnumerateModes();
    LoadSettings();

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
