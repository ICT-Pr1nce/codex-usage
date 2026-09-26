#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <thread>
#include <iostream>
#include <filesystem>
#include "PluginInterface.h"
static HWND FindOwn(const wchar_t* title) {
    struct Search { const wchar_t* title; HWND result; } search{title, nullptr};
    EnumWindows([](HWND h, LPARAM data)->BOOL {
        auto& s = *reinterpret_cast<Search*>(data);
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        wchar_t text[128]{}; GetWindowTextW(h, text, 128);
        if (pid == GetCurrentProcessId() && std::wstring(text) == s.title) { s.result = h; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.result;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    SetEnvironmentVariableW(L"APPDATA", argv[2]);
    auto dll = LoadLibraryW(argv[1]);
    if (!dll) return 3;
    auto entry = reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll, "TMPluginGetInstance"));
    if (!entry) return 4;
    auto p = entry();
    for (int test = 0; test < 4; ++test) {
        std::cout << "settings case " << test << std::endl;
        bool success = true;
        std::thread driver([&] {
            HWND dialog = nullptr;
            for (int i = 0; i < 200 && !dialog; ++i) { dialog = FindOwn(L"Codex Quota 设置"); Sleep(25); }
            if (!dialog) ExitProcess(20);
            std::cout << "dialog found" << std::endl;
            ShowWindow(dialog, SW_HIDE);
            // Validate all actionable controls fit in the client area.
            RECT area{}; GetClientRect(dialog, &area);
            for (int id : {100, 101, 102, IDOK, IDCANCEL}) {
                HWND child = GetDlgItem(dialog, id);
                RECT r{}; GetWindowRect(child, &r); MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
                if (!child || r.left < 0 || r.top < 0 || r.right > area.right || r.bottom > area.bottom) success = false;
            }
            const bool minutes = test % 2 != 0;
            std::cout << "layout checked" << std::endl;
            SendDlgItemMessageW(dialog, 101, CB_SETCURSEL, minutes ? 1 : 0, 0);
            if (test >= 2) {
                SetDlgItemTextW(dialog, 100, minutes ? L"31" : L"4");
                PostMessageW(dialog, WM_COMMAND, IDOK, 0);
                HWND warning = nullptr;
                for (int i = 0; i < 200 && !warning; ++i) { warning = FindOwn(L"查询间隔"); Sleep(25); }
                if (!warning) ExitProcess(21);
                std::cout << "warning found" << std::endl;
                PostMessageW(warning, WM_CLOSE, 0, 0);
                for (int i = 0; i < 200 && IsWindow(warning); ++i) Sleep(25);
                if (IsWindow(warning)) ExitProcess(22);
            }
            SetDlgItemTextW(dialog, 100, minutes ? L"30" : L"5");
            PostMessageW(dialog, WM_COMMAND, IDOK, 0);
            std::cout << "save posted" << std::endl;
        });
        auto result = p->ShowOptionsDialog(nullptr);
        std::cout << "dialog returned" << std::endl;
        driver.join();
        auto ini = std::filesystem::path(argv[2]) / L"TrafficMonitor/CodexQuota.ini";
        UINT seconds = GetPrivateProfileIntW(L"CodexQuota", L"RefreshSeconds", 0, ini.c_str());
        if (!success || result != ITMPlugin::OR_OPTION_CHANGED || seconds != (test % 2 ? 1800u : 5u)) return 10 + test;
    }
    FreeLibrary(dll);
    std::cout << "PASS: settings controls fit; 5 seconds / 30 minutes saved; 4 seconds / 31 minutes rejected\n";
}
