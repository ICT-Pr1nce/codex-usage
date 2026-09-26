#pragma once
#include "json.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
using Json = nlohmann::json;
struct Window {
    bool available = false;
    double remaining = 0;
    long long reset = 0;
};
struct Quota {
    std::array<Window, 2> windows{};
    std::string plan;
};
// Match durations, never assume primary means five hours.
inline Quota ParseQuota(const Json& result) {
    const Json* bucket = nullptr;
    if (!result.is_object()) throw std::runtime_error("Invalid result");
    auto map = result.find("rateLimitsByLimitId");
    if (map != result.end() && map->is_object() && !map->empty()) {
        auto codex = map->find("codex");
        if (codex != map->end() && codex->is_object()) bucket = &*codex;
        // Other model buckets must not be presented as the general Codex quota.
    } else {
        auto legacy = result.find("rateLimits");
        if (legacy != result.end() && legacy->is_object()) {
            auto id = legacy->find("limitId");
            if (id == legacy->end() || id->is_null() || *id == "codex") bucket = &*legacy;
        }
    }
    if (!bucket) throw std::runtime_error("Codex bucket unavailable");
    Quota out;
    auto plan = bucket->find("planType");
    if (plan != bucket->end() && plan->is_string()) out.plan = plan->get<std::string>();
    for (auto name : {"primary", "secondary"}) {
        auto w = bucket->find(name);
        if (w == bucket->end() || !w->is_object()) continue;
        auto duration = w->find("windowDurationMins");
        auto used = w->find("usedPercent");
        if (duration == w->end() || !duration->is_number_integer() ||
            used == w->end() || !used->is_number()) continue;
        const auto mins = duration->get<long long>();
        const int index = mins == 300 ? 0 : mins == 10080 ? 1 : -1;
        const double percent = used->get<double>();
        if (index < 0 || !std::isfinite(percent)) continue;
        auto& dest = out.windows[index];
        dest.available = true;
        dest.remaining = std::clamp(100.0 - percent, 0.0, 100.0);
        auto reset = w->find("resetsAt");
        if (reset != w->end() && reset->is_number_integer()) dest.reset = reset->get<long long>();
    }
    return out;
}
