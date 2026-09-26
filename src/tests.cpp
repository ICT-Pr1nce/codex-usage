#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <set>
#include <iostream>
#include <fstream>
#include <thread>
#include "Quota.h"
#include "PluginInterface.h"
static void Print(const std::wstring& s){int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,nullptr,0,nullptr,nullptr);std::string b(n,0);WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,b.data(),n,nullptr,nullptr);std::cout<<b.c_str()<<std::endl;}
static void Render(ITMPlugin* p,const wchar_t* file){
    const int w=600,h=210;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bmp);
    RECT all{0,0,w,h};FillRect(dc,&all,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    LOGFONTW lf{};lf.lfHeight=-17;wcscpy_s(lf.lfFaceName,L"Segoe UI");auto font=CreateFontIndirectW(&lf);auto prev=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
    for(int n=0;n<4;++n){auto item=p->GetItem(n);auto value=std::wstring(item->GetItemLableText())+item->GetItemValueText();int x=n%2?300:18,y=12+(n/2)*32;TextOutW(dc,x,y,value.c_str(),static_cast<int>(value.size()));}
    p->GetItem(4)->DrawItem(dc,18,100,240,54,false);p->GetItem(5)->DrawItem(dc,300,100,240,54,false);
    // Ensure every block paints ink on both rows and stays inside its assigned rectangle.
    for(int n=4;n<6;++n)for(int height:{18,28,40,60}){
        FillRect(dc,&all,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        p->GetItem(n)->DrawItem(dc,10,10,200,height,false);GdiFlush();auto pixels=static_cast<unsigned*>(bits);bool top=false,bottom=false;
        for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if((pixels[yy*w+xx]&0xffffff)!=0xffffff){assert(xx>=10 && xx<210 && yy>=10 && yy<10+height);if(yy<10+height/2)top=true;else bottom=true;}
        assert(top && bottom);
    }
    FillRect(dc,&all,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    for(int n=0;n<4;++n){auto item=p->GetItem(n);auto value=std::wstring(item->GetItemLableText())+item->GetItemValueText();int x=n%2?300:18,y=12+(n/2)*32;TextOutW(dc,x,y,value.c_str(),static_cast<int>(value.size()));}
    p->GetItem(4)->DrawItem(dc,18,100,240,54,false);p->GetItem(5)->DrawItem(dc,300,100,240,54,false);GdiFlush();
    if(file){BITMAPFILEHEADER head{};head.bfType=0x4d42;head.bfOffBits=sizeof(head)+sizeof(BITMAPINFOHEADER);head.bfSize=head.bfOffBits+w*h*4;std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(&head),sizeof(head));out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));out.write(static_cast<const char*>(bits),w*h*4);}
    SelectObject(dc,prev);DeleteObject(font);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
static HWND OwnWindow(const wchar_t* title){struct S{const wchar_t* t;HWND h;}s{title,nullptr};EnumWindows([](HWND h,LPARAM data)->BOOL{auto& s=*reinterpret_cast<S*>(data);DWORD pid=0;GetWindowThreadProcessId(h,&pid);wchar_t b[128]{};GetWindowTextW(h,b,128);if(pid==GetCurrentProcessId() && std::wstring(b)==s.t){s.h=h;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&s));return s.h;}
static void Settings(ITMPlugin* p){
    for(int test=0;test<4;++test){
        std::thread driver([&]{HWND dialog=nullptr;for(int i=0;i<200 && !dialog;++i){dialog=OwnWindow(L"Codex Quota 设置");Sleep(25);}if(!dialog)ExitProcess(20);ShowWindow(dialog,SW_HIDE);
            RECT area{};GetClientRect(dialog,&area);for(int id:{100,101,102,IDOK,IDCANCEL}){auto child=GetDlgItem(dialog,id);RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,dialog,reinterpret_cast<POINT*>(&r),2);assert(child && r.right<=area.right && r.bottom<=area.bottom);}
            bool mins=test%2;SendDlgItemMessageW(dialog,101,CB_SETCURSEL,mins?1:0,0);
            if(test>=2){SetDlgItemTextW(dialog,100,mins?L"31":L"4");PostMessageW(dialog,WM_COMMAND,IDOK,0);HWND warning=nullptr;for(int i=0;i<200 && !warning;++i){warning=OwnWindow(L"查询间隔");Sleep(25);}if(!warning)ExitProcess(21);PostMessageW(warning,WM_CLOSE,0,0);for(int i=0;i<200 && IsWindow(warning);++i)Sleep(25);if(IsWindow(warning))ExitProcess(22);}
            SetDlgItemTextW(dialog,100,mins?L"30":L"5");PostMessageW(dialog,WM_COMMAND,IDOK,0);
        });
        auto result=p->ShowOptionsDialog(nullptr);driver.join();assert(result==ITMPlugin::OR_OPTION_CHANGED);
        wchar_t env[32768]{};GetEnvironmentVariableW(L"APPDATA",env,32768);auto ini=std::wstring(env)+L"/TrafficMonitor/CodexQuota.ini";assert(GetPrivateProfileIntW(L"CodexQuota",L"RefreshSeconds",0,ini.c_str())==(test%2?1800u:5u));
    }
    std::cout<<"PASS settings: seconds/minutes, range, layout and persistence\n";
}
int wmain(int argc,wchar_t** argv){
    if(argc<2)return 2;
    assert(Countdown(0,100)==L"N/A");assert(Countdown(100,100)==L"0d 0h");assert(Countdown(99,100)==L"0d 0h");assert(Countdown(3699,100)==L"0d 0h");assert(Countdown(3700,100)==L"0d 1h");assert(Countdown(100+86400+23*3600,100)==L"1d 23h");assert(Countdown(100+7*86400,100)==L"7d 0h");
    auto window=[](double used,int mins){return Json{{"usedPercent",used},{"windowDurationMins",mins},{"resetsAt",1900000000}};};
    Json j={{"rateLimits",{{"primary",window(20,300)},{"secondary",window(73,10080)}}}};auto q=ParseQuota(j);assert(q.windows[0].remaining==80 && q.windows[1].remaining==27);
    j["rateLimitsByLimitId"]={{"codex",{{"primary",window(21,10080)},{"secondary",nullptr}}}};q=ParseQuota(j);assert(!q.windows[0].available && q.windows[1].remaining==79);
    j["rateLimitsByLimitId"]={{"other",{{"primary",window(10,300)}}}};bool rejected=false;try{ParseQuota(j);}catch(...){rejected=true;}assert(rejected);
    auto dll=LoadLibraryW(argv[1]);if(!dll){std::cerr<<"LoadLibrary failed "<<GetLastError()<<std::endl;return 3;}auto entry=reinterpret_cast<ITMPlugin*(*)()>(GetProcAddress(dll,"TMPluginGetInstance"));assert(entry);auto p=entry();assert(p==entry() && !p->GetItem(-1) && !p->GetItem(6));
    std::set<std::wstring> ids;for(int n=0;n<6;++n){auto item=p->GetItem(n);assert(item && std::wstring(item->GetItemName())==ItemName(n));ids.insert(item->GetItemId());assert(item->IsCustomDraw()==(n>=4));assert(item->IsDoubleLineExclusive()==(n>=4));}assert(ids.size()==6);
    if(argc>2 && std::wstring(argv[2])==L"--settings"){assert(argc==4);SetEnvironmentVariableW(L"APPDATA",argv[3]);Settings(p);}
    else if(argc>2 && std::wstring(argv[2])==L"--live"){
        bool ready=false;for(int i=0;i<200;++i){p->DataRequired();if(std::wstring(p->GetItem(0)->GetItemValueText())!=L"..." && std::wstring(p->GetItem(1)->GetItemValueText())!=L"..."){ready=true;break;}Sleep(250);}assert(ready);
        for(int n=0;n<6;++n)Print(std::wstring(p->GetItem(n)->GetItemName())+L" = "+p->GetItem(n)->GetItemValueText());
        assert(std::wstring(p->GetItem(0)->GetItemValueText())!=L"ERR");Print(p->GetTooltipInfo());
        auto weekBlock=std::wstring(p->GetItem(5)->GetItemValueText());assert(weekBlock==L"week "+std::wstring(p->GetItem(1)->GetItemValueText())+L"\n"+p->GetItem(3)->GetItemValueText());
    }
    Render(p,argc>3 && std::wstring(argv[2])==L"--live"?argv[3]:nullptr);
    FreeLibrary(dll);std::cout<<"PASS: six ABI items, stable IDs, four single/two double, countdown, quota mapping, render clipping at 18/28/40/60px\n";
}
