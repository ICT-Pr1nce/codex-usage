#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include "Rendering.h"
int wmain(int argc,wchar_t** argv){
    const int width=948,height=260;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(dc,bitmap);
    auto background=CreateSolidBrush(RGB(30,30,30));RECT all{0,0,width,height};FillRect(dc,&all,background);
    LOGFONTW font{};font.lfHeight=-18;wcscpy_s(font.lfFaceName,L"Segoe UI");auto handle=CreateFontIndirectW(&font);auto prev=SelectObject(dc,handle);
    const double used[]={80,81,90,91,100};
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(235,235,235));
    for(int i=0;i<5;++i){
        int x=12+i*188;auto title=L"Used "+std::to_wstring(static_cast<int>(used[i]))+L"%";TextOutW(dc,x,10,title.c_str(),static_cast<int>(title.size()));
        for(int mode=0;mode<2;++mode){
            Window w{true,100-used[i],1900000000};DisplayFrame f;f.doubleLine=true;f.alert=QuotaAlert(w,true);f.lines={L"week: "+Percent(w,mode!=0),L"6d 13h"};
            int y=mode?122:43;PaintFrame(dc,x,y,166,62,f);GdiFlush();
            auto expected=f.alert==Alert::Normal?RGB(30,30,30):AlertBackground(f.alert);
            assert(GetPixel(dc,x+160,y+30)==expected);
            assert(GetPixel(dc,x-1,y+30)==RGB(30,30,30));
            bool ink=false;for(int yy=y;yy<y+62;++yy)for(int xx=x;xx<x+166;++xx)if(GetPixel(dc,xx,yy)==FrameText(f))ink=true;assert(ink);
        }
        DisplayFrame single;single.alert=QuotaAlert(Window{true,100-used[i],0},true);single.lines[0]=L"week: "+Percent(Window{true,100-used[i],0},false);PaintFrame(dc,x,207,166,29,single);
        assert(GetPixel(dc,x+160,220)==(single.alert==Alert::Normal?RGB(30,30,30):AlertBackground(single.alert)));
    }
    if(argc>1){GdiFlush();BITMAPFILEHEADER head{};head.bfType=0x4d42;head.bfOffBits=sizeof(head)+sizeof(BITMAPINFOHEADER);head.bfSize=head.bfOffBits+width*height*4;std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<const char*>(&head),sizeof(head));out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));out.write(static_cast<const char*>(pixels),width*height*4);}
    SelectObject(dc,prev);DeleteObject(handle);SelectObject(dc,old);DeleteObject(bitmap);DeleteObject(background);DeleteDC(dc);
    std::cout<<"PASS: exact80 normal, over80 through90 yellow/white, over90 red/white; both modes; single/double GDI pixels\n";
    return 0;
}
