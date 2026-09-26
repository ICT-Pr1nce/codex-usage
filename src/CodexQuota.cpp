#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <mutex>
#include <ctime>
#include <vector>
#include "PluginInterface.h"
#include "Quota.h"
namespace fs=std::filesystem;
static HMODULE moduleHandle;
struct Handle {
    HANDLE h=nullptr;
    Handle()=default; Handle(const Handle&)=delete;
    ~Handle(){ close(); }
    void close(){ if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h); h=nullptr; }
};
static std::wstring Env(const wchar_t* key){ wchar_t b[32768]{}; DWORD n=GetEnvironmentVariableW(key,b,32768);return n && n<32768?std::wstring(b,n):L""; }
static fs::path ModuleDir(){ wchar_t b[32768]{};GetModuleFileNameW(moduleHandle,b,32768);return fs::path(b).parent_path(); }
static fs::path UserConfig(){ return fs::path(Env(L"APPDATA"))/L"TrafficMonitor/CodexQuota.ini"; }
static fs::path ActiveConfig(){ std::error_code ec;auto p=UserConfig();return fs::is_regular_file(p,ec)?p:ModuleDir()/L"CodexQuota.ini"; }
static unsigned Refresh(){ return std::clamp(GetPrivateProfileIntW(L"CodexQuota",L"RefreshSeconds",60,ActiveConfig().c_str()),5u,1800u); }
static std::wstring ConfigPath(){ wchar_t p[32768]{};GetPrivateProfileStringW(L"CodexQuota",L"CodexPath",L"",p,32768,ActiveConfig().c_str());return p; }
static std::wstring Date(long long stamp){
    if(stamp<=0) return L"未提供";
    __time64_t t=stamp;tm local{};if(_localtime64_s(&local,&t)) return L"未提供";
    wchar_t b[80]{};wcsftime(b,80,L"%Y-%m-%d %H:%M:%S",&local);return b;
}
struct Options { unsigned seconds=60;std::wstring path;bool saved=false; };
static INT_PTR CALLBACK OptionsProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto o=reinterpret_cast<Options*>(GetWindowLongPtrW(dialog,DWLP_USER));
    if(message==WM_INITDIALOG){
        o=reinterpret_cast<Options*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);
        int dpi=static_cast<int>(GetDpiForWindow(dialog));
        SetWindowPos(dialog,nullptr,0,0,MulDiv(490,dpi,96),MulDiv(335,dpi,96),SWP_NOMOVE|SWP_NOZORDER);
        auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){
            auto c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,MulDiv(x,dpi,96),MulDiv(y,dpi,96),MulDiv(w,dpi,96),MulDiv(h,dpi,96),dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),moduleHandle,nullptr);
            SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return c;
        };
        add(L"STATIC",L"查询间隔（5 秒至 30 分钟）",0,16,14,400,24,0);
        bool mins=o->seconds%60==0;auto value=std::to_wstring(mins?o->seconds/60:o->seconds);
        add(L"EDIT",value.c_str(),WS_BORDER|WS_TABSTOP|ES_NUMBER,16,44,145,27,100);
        auto combo=add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,176,44,170,100,101);
        SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"秒"));SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"分钟"));SendMessageW(combo,CB_SETCURSEL,mins?1:0,0);
        add(L"STATIC",L"Codex 程序路径（留空自动检测）",0,16,87,430,24,0);
        add(L"EDIT",o->path.c_str(),WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,16,116,440,27,102);
        add(L"STATIC",L"6 个显示项在 TrafficMonitor 的显示设置中选择。\n刷新时间表示额度重置倒计时（xd xh）。\nTrafficMonitor 1.86 使用组合块时请开启水平排列。",0,16,160,440,66,0);
        add(L"BUTTON",L"保存",WS_TABSTOP|BS_DEFPUSHBUTTON,256,248,90,30,IDOK);
        add(L"BUTTON",L"取消",WS_TABSTOP,360,248,90,30,IDCANCEL);
        SetFocus(GetDlgItem(dialog,100));return FALSE;
    }
    if(message==WM_COMMAND && LOWORD(wp)==IDOK && o){
        BOOL valid=FALSE;UINT amount=GetDlgItemInt(dialog,100,&valid,FALSE);
        bool mins=SendDlgItemMessageW(dialog,101,CB_GETCURSEL,0,0)==1;
        auto seconds=static_cast<unsigned long long>(amount)*(mins?60:1);
        if(!valid || seconds<5 || seconds>1800){ MessageBoxW(dialog,L"请输入 5–1800 秒，或 1–30 分钟。",L"查询间隔",MB_OK|MB_ICONWARNING);return TRUE; }
        wchar_t path[32768]{};GetDlgItemTextW(dialog,102,path,32768);
        if(*path){std::error_code ec;fs::path p(path);if(!p.is_absolute() || p.extension()!=L".exe" || !fs::is_regular_file(p,ec)){MessageBoxW(dialog,L"请输入实际 codex.exe 的绝对路径，或留空。",L"程序路径",MB_OK|MB_ICONWARNING);return TRUE;}}
        auto config=UserConfig();std::error_code ec;fs::create_directories(config.parent_path(),ec);
        auto value=std::to_wstring(seconds);
        if(ec || !WritePrivateProfileStringW(L"CodexQuota",L"CodexPath",path,config.c_str()) || !WritePrivateProfileStringW(L"CodexQuota",L"RefreshSeconds",value.c_str(),config.c_str())){MessageBoxW(dialog,L"配置保存失败，请检查配置目录权限。",L"Codex Quota",MB_OK|MB_ICONERROR);return TRUE;}
        o->seconds=static_cast<unsigned>(seconds);o->saved=true;EndDialog(dialog,IDOK);return TRUE;
    }
    if(message==WM_CLOSE || (message==WM_COMMAND && LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}
class Plugin;
class Item:public IPluginItem {
    Plugin& owner;int index;
public:
    Item(Plugin& p,int n):owner(p),index(n){}
    const wchar_t* GetItemName()const override{return ItemName(index);}
    const wchar_t* GetItemId()const override{return ItemId(index);}
    const wchar_t* GetItemLableText()const override{return index==0?L"5h: ":index==1?L"week: ":index==2?L"5h reset: ":index==3?L"week reset: ":L"";}
    const wchar_t* GetItemValueText()const override;
    const wchar_t* GetItemValueSampleText()const override{return index<2?L"100.0%":index<4?L"99d 23h":L"week 100.0%";}
    bool IsCustomDraw()const override{return index>=4;}
    int IsDoubleLineExclusive()const override{return index>=4?1:0;}
    int GetItemWidth()const override{return index>=4?106:0;}
    int GetItemWidthEx(void* dc)const override;
    void DrawItem(void* dc,int x,int y,int w,int h,bool dark)override;
};
class Plugin:public ITMPlugin {
    std::mutex lock;
    Item items[6]={{*this,0},{*this,1},{*this,2},{*this,3},{*this,4},{*this,5}};
    Handle input,output,process,job;
    Quota quota;bool good=false;int stage=0;
    std::string pending;
    unsigned interval=60;
    ULONGLONG deadline=0,nextPoll=0,fetchedTick=0;
    long long fetchedTime=0;
    std::wstring status=L"等待首次查询";
    COLORREF textColor=RGB(0,0,0);bool hasColor=false;
    void Stop(){job.close();if(process.h)TerminateProcess(process.h,0);input.close();output.close();process.close();stage=0;pending.clear();}
    void Fail(const wchar_t* why){good=false;quota={};status=why;Stop();nextPoll=GetTickCount64()+static_cast<ULONGLONG>(interval)*1000;}
    fs::path FindCodex(){
        interval=Refresh();auto configured=ConfigPath();std::error_code ec;
        if(!configured.empty()){fs::path p(configured);return p.is_absolute() && p.extension()==L".exe" && fs::is_regular_file(p,ec)?p:fs::path{};}
        fs::path desktop=fs::path(Env(L"LOCALAPPDATA"))/L"Programs/OpenAI/Codex/bin/codex.exe";
        if(desktop.is_absolute() && fs::is_regular_file(desktop,ec))return desktop;
        auto paths=Env(L"PATH");
        for(size_t start=0;start<paths.size();){auto end=paths.find(L';',start);auto part=paths.substr(start,end==std::wstring::npos?end:end-start);if(part.size()>1 && part.front()==L'"' && part.back()==L'"')part=part.substr(1,part.size()-2);auto p=fs::path(part)/L"codex.exe";if(p.is_absolute() && fs::is_regular_file(p,ec))return p;if(end==std::wstring::npos)break;start=end+1;}
        auto npm=fs::path(Env(L"APPDATA"))/L"npm/node_modules/@openai";
        if(fs::is_directory(npm,ec)){size_t visited=0;for(fs::recursive_directory_iterator it(npm,fs::directory_options::skip_permission_denied,ec),end;it!=end && !ec && visited++<3000;it.increment(ec)){if(it.depth()>8){it.disable_recursion_pending();continue;}if(it->path().filename()==L"codex.exe" && it->is_regular_file(ec))return it->path();}}
        return {};
    }
    bool Send(const std::string& line){DWORD count=0;return WriteFile(input.h,line.data(),static_cast<DWORD>(line.size()),&count,nullptr) && count==line.size();}
    void Start(){
        auto exe=FindCodex();if(exe.empty()){Fail(L"找不到 codex.exe；请在插件选项中设置程序路径");return;}
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};Handle childInput,childOutput,childError;
        if(!CreatePipe(&childInput.h,&input.h,&sa,0) || !CreatePipe(&output.h,&childOutput.h,&sa,0)){Fail(L"无法创建通信管道");return;}
        SetHandleInformation(input.h,HANDLE_FLAG_INHERIT,0);SetHandleInformation(output.h,HANDLE_FLAG_INHERIT,0);
        childError.h=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        if(childError.h==INVALID_HANDLE_VALUE){Fail(L"无法初始化进程输出");return;}
        STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.StartupInfo.wShowWindow=SW_HIDE;
        si.StartupInfo.hStdInput=childInput.h;si.StartupInfo.hStdOutput=childOutput.h;si.StartupInfo.hStdError=childError.h;
        SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> storage(bytes);si.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&bytes)){Fail(L"无法初始化进程属性");return;}
        HANDLE inherited[]={childInput.h,childOutput.h,childError.h};
        if(!UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){DeleteProcThreadAttributeList(si.lpAttributeList);Fail(L"无法设置通信句柄");return;}
        job.h=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!job.h || !SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){DeleteProcThreadAttributeList(si.lpAttributeList);Fail(L"无法管理查询进程");return;}
        std::wstring command=L"\""+exe.wstring()+L"\" app-server";PROCESS_INFORMATION pi{};
        BOOL ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,exe.parent_path().c_str(),&si.StartupInfo,&pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        if(!ok){Fail(L"无法启动 codex.exe");return;}
        process.h=pi.hProcess;Handle thread;thread.h=pi.hThread;
        if(!AssignProcessToJobObject(job.h,process.h) || ResumeThread(thread.h)==static_cast<DWORD>(-1)){Fail(L"无法运行查询进程");return;}
        stage=1;deadline=GetTickCount64()+45000;status=L"正在查询官方账号额度";
        if(!Send("{\"id\":1,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"trafficmonitor_codex_quota\",\"version\":\"1.1.0\"}}}\n"))Fail(L"初始化通信失败");
    }
    void Receive(const Json& message){
        if(!message.is_object())return;
        auto id=message.find("id");if(id==message.end() || !id->is_number_integer() || *id!=(stage==1?1:2))return;
        if(message.contains("error")){Fail(L"查询失败；请检查 ChatGPT 登录、网络及账号权限（API Key 不支持）");return;}
        auto result=message.find("result");if(result==message.end()){Fail(L"账号接口响应无效");return;}
        if(stage==1){stage=2;if(!Send("{\"method\":\"initialized\",\"params\":{}}\n{\"id\":2,\"method\":\"account/rateLimits/read\"}\n"))Fail(L"查询通信失败");}
        else{quota=ParseQuota(*result);fetchedTick=GetTickCount64();fetchedTime=_time64(nullptr);good=true;status=L"真实账号数据 · account/rateLimits/read";Stop();nextPoll=fetchedTick+static_cast<ULONGLONG>(interval)*1000;}
    }
    std::wstring ValueLocked(int window,bool reset){
        if(!good)return stage?L"...":nextPoll?L"ERR":L"...";
        if(GetTickCount64()-fetchedTick>static_cast<ULONGLONG>(interval)*1000+45000)return L"STALE";
        const auto& w=quota.windows[window];if(!w.available)return L"N/A";
        if(reset)return Countdown(w.reset,_time64(nullptr));
        if(w.reset>0 && _time64(nullptr)>=w.reset)return L"...";
        wchar_t b[32]{};swprintf_s(b,L"%.1f%%",w.remaining);return b;
    }
public:
    ~Plugin(){Stop();}
    IPluginItem* GetItem(int n)override{return n>=0 && n<6?&items[n]:nullptr;}
    void DataRequired()override{
        std::lock_guard<std::mutex> guard(lock);
        try{
            auto now=GetTickCount64();if(!stage){if(now>=nextPoll)Start();return;}
            if(now>=deadline){Fail(L"查询超时；请检查网络或 Codex 登录");return;}
            for(int i=0;i<16 && stage;++i){
                DWORD available=0;if(!PeekNamedPipe(output.h,nullptr,0,nullptr,&available,nullptr)){Fail(L"Codex 查询进程断开");return;}
                if(!available){if(WaitForSingleObject(process.h,0)==WAIT_OBJECT_0)Fail(L"Codex 查询进程退出");break;}
                char buffer[8192];DWORD count=0;if(!ReadFile(output.h,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr)||!count){Fail(L"读取响应失败");return;}
                pending.append(buffer,count);if(pending.size()>1024*1024){Fail(L"响应超出大小限制");return;}
                size_t end;while(stage && (end=pending.find('\n'))!=std::string::npos){auto line=pending.substr(0,end);pending.erase(0,end+1);if(!line.empty())Receive(Json::parse(line));}
            }
        }catch(...){Fail(L"额度响应格式不受支持，请检查或更新 Codex");}
    }
    std::wstring Value(int n){std::lock_guard<std::mutex> g(lock);return ValueLocked(n%2,n>=2 && n<4);}
    std::array<std::wstring,2> Block(int window){std::lock_guard<std::mutex> g(lock);return {std::wstring(window?L"week ":L"5h ")+ValueLocked(window,false),ValueLocked(window,true)};}
    COLORREF Color(bool dark){std::lock_guard<std::mutex> g(lock);return hasColor?textColor:dark?RGB(240,240,240):RGB(20,20,20);}
    void OnExtenedInfo(ExtendedInfoIndex index,const wchar_t* data)override{if(index==EI_VALUE_TEXT_COLOR && data){std::lock_guard<std::mutex> g(lock);textColor=wcstoul(data,nullptr,10);hasColor=true;}}
    const wchar_t* GetInfo(PluginInfoIndex n)override{switch(n){case TMI_NAME:return L"Codex Quota";case TMI_DESCRIPTION:return L"真实额度、重置倒计时：四个单行项和两个双行块";case TMI_AUTHOR:return L"CodexQuota contributors";case TMI_COPYRIGHT:return L"MIT (original code)";case TMI_VERSION:return L"1.1.0";case TMI_URL:return L"https://learn.chatgpt.com/docs/app-server";default:return L"";}}
    const wchar_t* GetTooltipInfo()override{
        thread_local std::wstring text;std::lock_guard<std::mutex> g(lock);text=L"Codex Quota\n"+status;
        if(good){text+=L"\n上次查询："+Date(fetchedTime);for(int i=0;i<2;++i){text+=i?L"\nweek：":L"\n5h：";text+=ValueLocked(i,false);text+=L"；重置倒计时："+ValueLocked(i,true);if(quota.windows[i].available){text+=L"\n重置时刻："+Date(quota.windows[i].reset);if(quota.windows[i].reset>0 && _time64(nullptr)>=quota.windows[i].reset)text+=L"（等待服务器更新）";}else text+=L"（接口未返回此窗口）";}}
        text+=L"\n查询间隔："+std::to_wstring(interval)+L" 秒。倒计时按整天、整小时向下取整；不足 1 小时显示 0d 0h。\n刷新时间指额度重置时间，并非插件下次查询时间。";return text.c_str();
    }
    int GetCommandCount()override{return 1;}
    const wchar_t* GetCommandName(int n)override{return n==0?L"立即刷新 Codex 额度":nullptr;}
    void OnPluginCommand(int n,void*,void*)override{if(n==0){std::lock_guard<std::mutex> g(lock);nextPoll=0;}}
    OptionReturn ShowOptionsDialog(void* parent)override{
        Options options;options.seconds=Refresh();options.path=ConfigPath();
        alignas(DWORD) unsigned char memory[512]{};auto dialog=reinterpret_cast<DLGTEMPLATE*>(memory);
        dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;dialog->cx=260;dialog->cy=170;
        auto tail=reinterpret_cast<WORD*>(memory+sizeof(DLGTEMPLATE));*tail++=0;*tail++=0;const wchar_t title[]=L"Codex Quota 设置";memcpy(tail,title,sizeof(title));
        DialogBoxIndirectParamW(moduleHandle,dialog,static_cast<HWND>(parent),OptionsProc,reinterpret_cast<LPARAM>(&options));
        if(options.saved){std::lock_guard<std::mutex> g(lock);interval=options.seconds;Stop();nextPoll=0;return OR_OPTION_CHANGED;}return OR_OPTION_UNCHANGED;
    }
};
const wchar_t* Item::GetItemValueText()const{thread_local std::wstring values[6];if(index>=4){auto lines=owner.Block(index-4);values[index]=lines[0]+L"\n"+lines[1];}else values[index]=owner.Value(index);return values[index].c_str();}
int Item::GetItemWidthEx(void* dc)const{
    if(index<4 || !dc)return 0;SIZE size{};const wchar_t* sample=L"week 100.0%";GetTextExtentPoint32W(static_cast<HDC>(dc),sample,static_cast<int>(wcslen(sample)),&size);return size.cx+MulDiv(12,GetDeviceCaps(static_cast<HDC>(dc),LOGPIXELSX),96);
}
void Item::DrawItem(void* context,int x,int y,int w,int h,bool dark){
    if(index<4 || !context || w<=0 || h<=1)return;
    HDC dc=static_cast<HDC>(context);int saved=SaveDC(dc);if(!saved)return;
    IntersectClipRect(dc,x,y,x+w,y+h);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,owner.Color(dark));
    LOGFONTW font{};auto current=GetCurrentObject(dc,OBJ_FONT);if(!GetObjectW(current,sizeof(font),&font)){wcscpy_s(font.lfFaceName,L"Segoe UI");font.lfHeight=-12;}
    int row=h/2;int padding=std::max(1,MulDiv(2,GetDeviceCaps(dc,LOGPIXELSX),96));
    // Fit the font to the actual rectangle, including older hosts without exclusive double-line support.
    font.lfHeight=-std::max(1,std::min(static_cast<int>(std::abs(font.lfHeight)),row-1));font.lfWidth=0;
    HFONT adjusted=CreateFontIndirectW(&font);if(adjusted)SelectObject(dc,adjusted);
    auto lines=owner.Block(index-4);
    RECT top{x+padding,y,x+w-padding,y+row},bottom{x+padding,y+row,x+w-padding,y+h};
    DrawTextW(dc,lines[0].c_str(),-1,&top,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
    DrawTextW(dc,lines[1].c_str(),-1,&bottom,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
    RestoreDC(dc,saved);if(adjusted)DeleteObject(adjusted);
}
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance(){static Plugin instance;return &instance;}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)moduleHandle=h;return TRUE;}
