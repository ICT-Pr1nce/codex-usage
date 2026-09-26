#pragma once
#include "json.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <limits>
using Json = nlohmann::json;
struct Window { bool available=false; double remaining=0; long long reset=0; };
struct Quota { std::array<Window,2> windows{}; };
inline Quota ParseQuota(const Json& result) {
    const Json* bucket=nullptr;
    if(!result.is_object()) throw std::runtime_error("Invalid result");
    auto map=result.find("rateLimitsByLimitId");
    if(map!=result.end() && map->is_object() && !map->empty()) {
        auto b=map->find("codex"); if(b!=map->end() && b->is_object()) bucket=&*b;
    } else {
        auto b=result.find("rateLimits");
        if(b!=result.end() && b->is_object()) {
            auto id=b->find("limitId");
            if(id==b->end() || id->is_null() || *id=="codex") bucket=&*b;
        }
    }
    if(!bucket) throw std::runtime_error("No Codex quota");
    Quota q;
    for(auto key:{"primary","secondary"}) {
        auto w=bucket->find(key); if(w==bucket->end() || !w->is_object()) continue;
        auto duration=w->find("windowDurationMins"),used=w->find("usedPercent");
        if(duration==w->end() || !duration->is_number_integer() || used==w->end() || !used->is_number()) continue;
        auto mins=duration->get<long long>(); int i=mins==300?0:mins==10080?1:-1;
        double percent=used->get<double>(); if(i<0 || !std::isfinite(percent)) continue;
        q.windows[i].available=true; q.windows[i].remaining=std::clamp(100-percent,0.0,100.0);
        auto reset=w->find("resetsAt");
        if(reset!=w->end() && reset->is_number_integer()) q.windows[i].reset=reset->get<long long>();
    }
    return q;
}
// Whole remaining days and hours; round down and refresh from wall-clock time.
inline std::wstring Countdown(long long reset,long long now) {
    if(reset<=0) return L"N/A";
    auto seconds=reset>now?reset-now:0;
    return std::to_wstring(seconds/86400)+L"d "+std::to_wstring((seconds%86400)/3600)+L"h";
}
inline const wchar_t* ItemName(int n) {
    static const wchar_t* names[]={L"5h额度",L"周额度",L"5h刷新时间",L"周额度刷新时间",L"5h两行块",L"周额度两行块"};
    return n>=0 && n<6?names[n]:L"";
}
inline const wchar_t* ItemId(int n) {
    // Preserve the first two IDs so existing layouts survive the upgrade.
    static const wchar_t* ids[]={L"codex_quota_5h_remaining",L"codex_quota_week_remaining",L"codex_quota_5h_reset",L"codex_quota_week_reset",L"codex_quota_5h_block",L"codex_quota_week_block"};
    return n>=0 && n<6?ids[n]:L"";
}

// Display mode never changes the alert threshold, which uses server usage.
enum class Alert { Normal, Warning, Exhausted };
inline Alert QuotaAlert(const Window& w,bool fresh) {
    if(!fresh || !w.available) return Alert::Normal;
    double used=100.0-w.remaining;
    return used>=100.0?Alert::Exhausted:used>80.0?Alert::Warning:Alert::Normal;
}
inline std::wstring Percent(const Window& w,bool showUsed) {
    double value=std::clamp(showUsed?100.0-w.remaining:w.remaining,0.0,100.0);
    return std::to_wstring(static_cast<int>(value))+L"%";
}
