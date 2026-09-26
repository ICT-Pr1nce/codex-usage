#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <mutex>
#include <ctime>
#include <vector>
#include "PluginInterface.h"
#include "Quota.h"

namespace fs = std::filesystem;
static HMODULE moduleHandle;
struct Handle {
    HANDLE h = nullptr;
    ~Handle() { close(); }
    void close() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); h = nullptr; }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
static std::wstring Env(const wchar_t* name) {
    wchar_t b[32768]{};
    DWORD n = GetEnvironmentVariableW(name, b, 32768);
    return n && n < 32768 ? std::wstring(b, n) : L"";
}
static fs::path ModuleDir() {
    wchar_t b[32768]{};
    GetModuleFileNameW(moduleHandle, b, 32768);
    return fs::path(b).parent_path();
}
static fs::path UserConfig() {
    return fs::path(Env(L"APPDATA")) / L"TrafficMonitor/CodexQuota.ini";
}
static fs::path ActiveConfig() {
    std::error_code ec;
    auto p = UserConfig();
    return fs::is_regular_file(p, ec) ? p : ModuleDir() / L"CodexQuota.ini";
}
struct Options {
    unsigned seconds;
    std::wstring path;
    bool saved = false;
};
static INT_PTR CALLBACK OptionsProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    auto options = reinterpret_cast<Options*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        options = reinterpret_cast<Options*>(lp);
        SetWindowLongPtrW(dialog, DWLP_USER, lp);
        const int dpi = static_cast<int>(GetDpiForWindow(dialog));
        SetWindowPos(dialog, nullptr, 0, 0, MulDiv(470, dpi, 96), MulDiv(300, dpi, 96), SWP_NOMOVE | SWP_NOZORDER);
        auto add = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
            HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, MulDiv(x,dpi,96), MulDiv(y,dpi,96), MulDiv(w,dpi,96), MulDiv(h,dpi,96), dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), moduleHandle, nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            return c;
        };
        add(L"STATIC", L"查询间隔（5 秒至 30 分钟）", 0, 16, 14, 340, 24, 0);
        bool minutes = options->seconds % 60 == 0;
        auto value = std::to_wstring(minutes ? options->seconds / 60 : options->seconds);
        add(L"EDIT", value.c_str(), WS_BORDER | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 16, 44, 145, 27, 100);
        auto combo = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, 176, 44, 170, 100, 101);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"秒"));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"分钟"));
        SendMessageW(combo, CB_SETCURSEL, minutes ? 1 : 0, 0);
        add(L"STATIC", L"Codex 程序路径（留空自动检测）", 0, 16, 87, 400, 24, 0);
        add(L"EDIT", options->path.c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 16, 116, 420, 27, 102);
        add(L"STATIC", L"使用 Codex 已登录账号查询真实额度，无需填写额度。", 0, 16, 159, 420, 42, 0);
        add(L"BUTTON", L"保存", WS_TABSTOP | BS_DEFPUSHBUTTON, 242, 211, 90, 30, IDOK);
        add(L"BUTTON", L"取消", WS_TABSTOP, 346, 211, 90, 30, IDCANCEL);
        SetFocus(GetDlgItem(dialog, 100));
        return FALSE;
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK && options) {
        BOOL valid = FALSE;
        UINT amount = GetDlgItemInt(dialog, 100, &valid, FALSE);
        bool minutes = SendDlgItemMessageW(dialog, 101, CB_GETCURSEL, 0, 0) == 1;
        unsigned long long seconds = static_cast<unsigned long long>(amount) * (minutes ? 60 : 1);
        if (!valid || seconds < 5 || seconds > 1800) {
            MessageBoxW(dialog, L"请输入 5–1800 秒，或 1–30 分钟。", L"查询间隔", MB_OK | MB_ICONWARNING);
            return TRUE;
        }
        wchar_t path[32768]{}; GetDlgItemTextW(dialog, 102, path, 32768);
        if (*path) {
            std::error_code ec;
            fs::path p(path);
            if (!p.is_absolute() || p.extension() != L".exe" || !fs::is_regular_file(p, ec)) {
                MessageBoxW(dialog, L"请选择实际存在的 codex.exe 绝对路径，或留空自动检测。", L"程序路径", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
        }
        auto config = UserConfig();
        std::error_code ec; fs::create_directories(config.parent_path(), ec);
        auto value = std::to_wstring(seconds);
        if (ec || !WritePrivateProfileStringW(L"CodexQuota", L"CodexPath", path, config.c_str()) ||
            !WritePrivateProfileStringW(L"CodexQuota", L"RefreshSeconds", value.c_str(), config.c_str())) {
            MessageBoxW(dialog, L"配置保存失败，请检查用户配置目录权限。", L"Codex Quota", MB_OK | MB_ICONERROR);
            return TRUE;
        }
        options->seconds = static_cast<unsigned>(seconds); options->saved = true;
        EndDialog(dialog, IDOK); return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        EndDialog(dialog, IDCANCEL); return TRUE;
    }
    return FALSE;
}
static std::wstring Date(long long stamp) {
    if (stamp <= 0) return L"未提供";
    __time64_t t = stamp;
    tm local{};
    if (_localtime64_s(&local, &t)) return L"未提供";
    wchar_t b[80]{};
    wcsftime(b, 80, L"%Y-%m-%d %H:%M:%S", &local);
    return b;
}
class Plugin;
class Item : public IPluginItem {
    Plugin& owner;
    int index;
public:
    Item(Plugin& p, int i) : owner(p), index(i) {}
    const wchar_t* GetItemName() const override { return index ? L"Codex 周剩余额度" : L"Codex 5h 剩余额度"; }
    const wchar_t* GetItemId() const override { return index ? L"codex_quota_week_remaining" : L"codex_quota_5h_remaining"; }
    const wchar_t* GetItemLableText() const override { return index ? L"Codex 周: " : L"Codex 5h: "; }
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override { return L"100.0%"; }
};
class Plugin : public ITMPlugin {
    std::mutex lock;
    Item five{*this, 0}, week{*this, 1};
    Handle input, output, process, job;
    std::string pending;
    Quota quota;
    bool good = false;
    int stage = 0;
    ULONGLONG deadline = 0, nextPoll = 0, fetchedTick = 0;
    long long fetchedTime = 0;
    unsigned interval = 60;
    std::wstring status = L"等待首次查询", executable;
    void Stop() {
        // No worker thread or waiting in DLL teardown. The job owns all children.
        job.close();
        if (process.h) TerminateProcess(process.h, 0);
        input.close(); output.close(); process.close();
        stage = 0; pending.clear();
    }
    void Fail(const wchar_t* why) {
        good = false; quota = {}; status = why;
        Stop(); nextPoll = GetTickCount64() + static_cast<ULONGLONG>(interval) * 1000;
    }
    fs::path FindCodex() {
        auto ini = ActiveConfig();
        wchar_t configured[32768]{};
        GetPrivateProfileStringW(L"CodexQuota", L"CodexPath", L"", configured, 32768, ini.c_str());
        interval = std::clamp(GetPrivateProfileIntW(L"CodexQuota", L"RefreshSeconds", 60, ini.c_str()), 5u, 1800u);
        std::error_code ec;
        if (*configured) {
            fs::path p(configured);
            if (p.is_absolute() && p.extension() == L".exe" && fs::is_regular_file(p, ec)) return p;
            return {};
        }
        // Desktop install first. Never search the TrafficMonitor working directory.
        fs::path desktop = fs::path(Env(L"LOCALAPPDATA")) / L"Programs/OpenAI/Codex/bin/codex.exe";
        if (desktop.is_absolute() && fs::is_regular_file(desktop, ec)) return desktop;
        auto paths = Env(L"PATH");
        for (size_t start = 0; start < paths.size();) {
            auto end = paths.find(L';', start);
            auto part = paths.substr(start, end == std::wstring::npos ? end : end - start);
            if (part.size() >= 2 && part.front() == L'"' && part.back() == L'"') part = part.substr(1, part.size()-2);
            fs::path p = fs::path(part) / L"codex.exe";
            if (p.is_absolute() && fs::is_regular_file(p, ec)) return p;
            if (end == std::wstring::npos) break;
            start = end + 1;
        }
        // npm distributions contain a native executable behind codex.cmd.
        fs::path npm = fs::path(Env(L"APPDATA")) / L"npm/node_modules/@openai";
        if (fs::is_directory(npm, ec)) {
            size_t visited = 0;
            for (fs::recursive_directory_iterator it(npm, fs::directory_options::skip_permission_denied, ec), end;
                 it != end && !ec && visited++ < 3000; it.increment(ec)) {
                if (it.depth() > 8) { it.disable_recursion_pending(); continue; }
                if (it->path().filename() == L"codex.exe" && it->is_regular_file(ec)) return it->path();
            }
        }
        return {};
    }
    bool Send(const std::string& line) {
        DWORD written = 0;
        return WriteFile(input.h, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) && written == line.size();
    }
    void Start() {
        auto exe = FindCodex();
        if (exe.empty()) { Fail(L"找不到 codex.exe；请安装 Codex，或在 CodexQuota.ini 设置 CodexPath"); return; }
        executable = exe.wstring();
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        Handle childInput, childOutput, childError;
        if (!CreatePipe(&childInput.h, &input.h, &sa, 0) || !CreatePipe(&output.h, &childOutput.h, &sa, 0)) {
            Fail(L"无法创建通信管道"); return;
        }
        SetHandleInformation(input.h, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(output.h, HANDLE_FLAG_INHERIT, 0);
        childError.h = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        if (childError.h == INVALID_HANDLE_VALUE) { Fail(L"无法初始化进程输出"); return; }
        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof(si);
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.StartupInfo.wShowWindow = SW_HIDE;
        si.StartupInfo.hStdInput = childInput.h; si.StartupInfo.hStdOutput = childOutput.h; si.StartupInfo.hStdError = childError.h;
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<unsigned char> attributes(bytes);
        si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes)) { Fail(L"无法初始化进程属性"); return; }
        HANDLE inherited[] = {childInput.h, childOutput.h, childError.h};
        if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(si.lpAttributeList); Fail(L"无法设置进程句柄"); return;
        }
        job.h = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.h || !SetInformationJobObject(job.h, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            DeleteProcThreadAttributeList(si.lpAttributeList); Fail(L"无法创建查询进程容器"); return;
        }
        std::wstring command = L"\"" + executable + L"\" app-server";
        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, exe.parent_path().c_str(), &si.StartupInfo, &pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        if (!ok) { Fail(L"无法启动 codex.exe"); return; }
        process.h = pi.hProcess;
        Handle mainThread; mainThread.h = pi.hThread;
        if (!AssignProcessToJobObject(job.h, process.h)) { Fail(L"无法管理查询进程"); return; }
        if (ResumeThread(mainThread.h) == static_cast<DWORD>(-1)) { Fail(L"无法运行查询进程"); return; }
        stage = 1; deadline = GetTickCount64() + 45000;
        status = L"正在查询官方账号额度";
        if (!Send("{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"trafficmonitor_codex_quota\",\"version\":\"1.0.0\"}}}\n")) Fail(L"初始化通信失败");
    }
    void Receive(const Json& message) {
        if (!message.is_object()) return;
        auto id = message.find("id");
        if (id == message.end() || !id->is_number_integer()) return;
        int wanted = stage == 1 ? 1 : 2;
        if (*id != wanted) return;
        if (message.contains("error")) {
            Fail(stage == 1 ? L"Codex 初始化失败，请更新 Codex" : L"额度查询失败：请检查 ChatGPT 登录状态、网络及账号权限（API Key 登录不支持）"); return;
        }
        auto result = message.find("result");
        if (result == message.end()) { Fail(L"账号接口响应无效"); return; }
        if (stage == 1) {
            stage = 2;
            if (!Send("{\"method\":\"initialized\",\"params\":{}}\n{\"id\":2,\"method\":\"account/rateLimits/read\"}\n")) Fail(L"查询通信失败");
        } else {
            quota = ParseQuota(*result);
            fetchedTick = GetTickCount64(); fetchedTime = _time64(nullptr); good = true;
            status = L"真实账号数据 · account/rateLimits/read";
            Stop(); nextPoll = fetchedTick + static_cast<ULONGLONG>(interval) * 1000;
        }
    }
public:
    ~Plugin() { Stop(); }
    IPluginItem* GetItem(int index) override { return index == 0 ? &five : index == 1 ? &week : nullptr; }
    void DataRequired() override {
        std::lock_guard<std::mutex> guard(lock);
        try {
            auto now = GetTickCount64();
            if (!stage) { if (now >= nextPoll) Start(); return; }
            if (now >= deadline) { Fail(L"查询超时；请检查网络或 Codex 登录状态"); return; }
            // Read only bytes already buffered: never block TrafficMonitor on the network.
            for (int iteration = 0; iteration < 16 && stage; ++iteration) {
                DWORD available = 0;
                if (!PeekNamedPipe(output.h, nullptr, 0, nullptr, &available, nullptr)) { Fail(L"Codex 查询进程已断开"); return; }
                if (!available) {
                    if (WaitForSingleObject(process.h, 0) == WAIT_OBJECT_0) Fail(L"Codex 查询进程已退出");
                    break;
                }
                char buffer[8192]; DWORD count = 0;
                if (!ReadFile(output.h, buffer, std::min<DWORD>(available, sizeof(buffer)), &count, nullptr) || !count) { Fail(L"读取查询结果失败"); return; }
                pending.append(buffer, count);
                if (pending.size() > 1024 * 1024) { Fail(L"接口响应超出大小限制"); return; }
                size_t end;
                while (stage && (end = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, end); pending.erase(0, end + 1);
                    if (!line.empty()) Receive(Json::parse(line));
                }
            }
        } catch (...) { Fail(L"额度响应格式不受支持；请检查或更新 Codex"); }
    }
    std::wstring Value(int index) {
        std::lock_guard<std::mutex> guard(lock);
        if (!good) return stage ? L"..." : nextPoll ? L"ERR" : L"...";
        if (GetTickCount64() - fetchedTick > static_cast<ULONGLONG>(interval) * 1000 + 45000) return L"STALE";
        const auto& w = quota.windows[index];
        if (!w.available) return L"N/A";
        if (w.reset > 0 && _time64(nullptr) >= w.reset) return L"...";
        wchar_t b[32]{}; swprintf_s(b, L"%.1f%%", w.remaining); return b;
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch(index) {
        case TMI_NAME: return L"Codex Quota";
        case TMI_DESCRIPTION: return L"Codex 真实账号额度：5 小时、周剩余百分比，独立显示。";
        case TMI_AUTHOR: return L"CodexQuota contributors";
        case TMI_COPYRIGHT: return L"MIT License";
        case TMI_VERSION: return L"1.0.0";
        case TMI_URL: return L"https://learn.chatgpt.com/docs/app-server";
        default: return L"";
        }
    }
    const wchar_t* GetTooltipInfo() override {
        thread_local std::wstring text;
        std::lock_guard<std::mutex> guard(lock);
        text = L"Codex Quota\n" + status;
        if (good) {
            text += L"\n上次成功查询：" + Date(fetchedTime);
            if (GetTickCount64() - fetchedTick > static_cast<ULONGLONG>(interval) * 1000 + 45000) text += L"\n数据已过期，等待重新查询";
            for (int i = 0; i < 2; ++i) {
                text += i ? L"\n周额度：" : L"\n5 小时额度：";
                const auto& w = quota.windows[i];
                if (!w.available) text += L"接口未返回此窗口（N/A，不等于 0% 或 100%）";
                else {
                    wchar_t b[64]{}; swprintf_s(b, L"剩余 %.1f%%；重置：", w.remaining);
                    text += b; text += Date(w.reset);
                    if (w.reset > 0 && _time64(nullptr) >= w.reset) text += L"（等待服务器更新）";
                }
            }
        }
        text += L"\n刷新间隔：" + std::to_wstring(interval) + L" 秒；重置时间为本地时间。";
        return text.c_str();
    }
    int GetCommandCount() override { return 1; }
    const wchar_t* GetCommandName(int index) override { return index == 0 ? L"立即刷新 Codex 额度" : nullptr; }
    void OnPluginCommand(int index, void*, void*) override {
        if (index != 0) return;
        std::lock_guard<std::mutex> guard(lock);
        nextPoll = 0;
    }
    OptionReturn ShowOptionsDialog(void* parent) override {
        Options options{};
        auto ini = ActiveConfig();
        options.seconds = std::clamp(GetPrivateProfileIntW(L"CodexQuota", L"RefreshSeconds", 60, ini.c_str()), 5u, 1800u);
        wchar_t path[32768]{};
        GetPrivateProfileStringW(L"CodexQuota", L"CodexPath", L"", path, 32768, ini.c_str());
        options.path = path;
        // Empty dialog template; controls are created in WM_INITDIALOG.
        alignas(DWORD) unsigned char memory[512]{};
        auto dialog = reinterpret_cast<DLGTEMPLATE*>(memory);
        dialog->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
        dialog->cx = 250; dialog->cy = 150;
        auto tail = reinterpret_cast<WORD*>(memory + sizeof(DLGTEMPLATE));
        *tail++ = 0; *tail++ = 0;
        const wchar_t title[] = L"Codex Quota 设置";
        memcpy(tail, title, sizeof(title));
        DialogBoxIndirectParamW(moduleHandle, dialog, static_cast<HWND>(parent), OptionsProc, reinterpret_cast<LPARAM>(&options));
        if (options.saved) {
            std::lock_guard<std::mutex> guard(lock);
            interval = options.seconds; Stop(); nextPoll = 0;
            return OR_OPTION_CHANGED;
        }
        return OR_OPTION_UNCHANGED;
    }
};
const wchar_t* Item::GetItemValueText() const {
    thread_local std::wstring values[2];
    values[index] = owner.Value(index); return values[index].c_str();
}
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    static Plugin instance;
    return &instance;
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) moduleHandle = module;
    return TRUE;
}
