#include "Quota.h"
#include <cassert>
#include <iostream>
int main() {
    auto window = [](double used, int mins) { return Json{{"usedPercent", used}, {"windowDurationMins", mins}, {"resetsAt", 1900000000}}; };
    Json legacy = {{"rateLimits", {{"primary", window(20, 300)}, {"secondary", window(73, 10080)}}}};
    auto q = ParseQuota(legacy);
    assert(q.windows[0].available && q.windows[0].remaining == 80);
    assert(q.windows[1].available && q.windows[1].remaining == 27);
    // Actual account shape: primary can be the weekly window, secondary null.
    Json weekly = {{"rateLimitsByLimitId", {{"codex", {{"primary", window(21, 10080)}, {"secondary", nullptr}}}}}};
    q = ParseQuota(weekly);
    assert(!q.windows[0].available && q.windows[1].remaining == 79);
    auto combined = legacy;
    combined["rateLimitsByLimitId"] = weekly["rateLimitsByLimitId"];
    assert(!ParseQuota(combined).windows[0].available);
    legacy["rateLimits"]["primary"] = window(0, 300);
    legacy["rateLimits"]["secondary"] = window(100, 10080);
    q = ParseQuota(legacy);
    assert(q.windows[0].remaining == 100 && q.windows[1].remaining == 0);
    legacy["rateLimits"]["primary"]["usedPercent"] = nullptr;
    assert(!ParseQuota(legacy).windows[0].available);
    legacy["rateLimits"]["primary"] = window(10, 15);
    assert(!ParseQuota(legacy).windows[0].available);
    legacy["rateLimits"]["primary"] = window(120, 300);
    assert(ParseQuota(legacy).windows[0].remaining == 0);
    legacy["rateLimits"]["primary"] = window(-5, 300);
    assert(ParseQuota(legacy).windows[0].remaining == 100);
    combined["rateLimitsByLimitId"] = {{"other", {{"primary", window(10, 300)}}}};
    bool rejected = false;
    try { ParseQuota(combined); } catch (...) { rejected = true; }
    assert(rejected);
    std::cout << "PASS: durations, weekly-only, map precedence, missing/null, 0/100, clamping, other buckets\n";
}
