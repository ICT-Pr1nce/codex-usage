#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include "PluginInterface.h"
static void Print(const std::wstring& text) {
    int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), count, nullptr, nullptr);
    std::cout << utf8.c_str();
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    SetConsoleOutputCP(CP_UTF8);
    auto dll = LoadLibraryW(argv[1]);
    if (!dll) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 3; }
    auto entry = reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll, "TMPluginGetInstance"));
    if (!entry) return 4;
    auto p = entry();
    if (argc > 2 && std::wstring(argv[2]) == L"--options") return p->ShowOptionsDialog(nullptr) == ITMPlugin::OR_OPTION_CHANGED ? 0 : 8;
    if (p != entry() || !p->GetItem(0) || !p->GetItem(1) || p->GetItem(-1) || p->GetItem(2)) return 5;
    if (std::wstring(p->GetItem(0)->GetItemId()) == p->GetItem(1)->GetItemId()) return 6;
    bool completed = false;
    unsigned long long slowest = 0;
    for (int i = 0; i < 200; ++i) {
        auto begin = GetTickCount64();
        p->DataRequired();
        auto elapsed = GetTickCount64() - begin;
        if (elapsed > slowest) slowest = elapsed;
        std::wstring a = p->GetItem(0)->GetItemValueText(), b = p->GetItem(1)->GetItemValueText();
        if (a != L"..." && b != L"...") {
            Print(L"5h=" + a + L", week=" + b + L"\n" + p->GetTooltipInfo() + L"\n");
            completed = a != L"ERR" && b != L"ERR";
            break;
        }
        Sleep(250);
    }
    std::cout << "Max DataRequired milliseconds=" << slowest << "\n";
    FreeLibrary(dll);
    return completed ? 0 : 7;
}
