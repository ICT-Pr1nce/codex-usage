#pragma once
#include <windows.h>
#include "Quota.h"
struct DisplayFrame {
    std::array<std::wstring,2> lines;
    bool doubleLine=false;
    Alert alert=Alert::Normal;
    COLORREF normalText=RGB(240,240,240);
};
inline COLORREF AlertBackground(Alert a) { return a==Alert::Critical?RGB(196,43,28):RGB(255,193,7); }
inline COLORREF FrameText(const DisplayFrame& f) { return f.alert!=Alert::Normal?RGB(255,255,255):f.normalText; }
inline void PaintFrame(HDC dc,int x,int y,int w,int h,const DisplayFrame& frame) {
    if(!dc || w<=0 || h<=1)return;
    int saved=SaveDC(dc);if(!saved)return;
    IntersectClipRect(dc,x,y,x+w,y+h);
    if(frame.alert!=Alert::Normal) {
        HBRUSH brush=CreateSolidBrush(AlertBackground(frame.alert));
        auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
        int radius=std::min(8,std::min(w,h));RoundRect(dc,x,y,x+w,y+h,radius,radius);
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(brush);
    }
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,FrameText(frame));
    LOGFONTW font{};auto current=GetCurrentObject(dc,OBJ_FONT);
    if(!GetObjectW(current,sizeof(font),&font)){wcscpy_s(font.lfFaceName,L"Segoe UI");font.lfHeight=-12;}
    int row=frame.doubleLine?h/2:h;
    int padding=std::max(2,MulDiv(5,GetDeviceCaps(dc,LOGPIXELSX),96));
    font.lfHeight=-std::max(1,std::min(static_cast<int>(std::abs(font.lfHeight)),row-1));font.lfWidth=0;
    HFONT adjusted=CreateFontIndirectW(&font);if(adjusted)SelectObject(dc,adjusted);
    RECT top{x+padding,y,x+w-padding,y+row};
    DrawTextW(dc,frame.lines[0].c_str(),-1,&top,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
    if(frame.doubleLine){RECT bottom{x+padding,y+row,x+w-padding,y+h};DrawTextW(dc,frame.lines[1].c_str(),-1,&bottom,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);}
    RestoreDC(dc,saved);if(adjusted)DeleteObject(adjusted);
}
