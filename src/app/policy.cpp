// ============================================================================
// Politica pura de decisao — implementacao (sem Win32; ver policy.h).
// ============================================================================
#include "app/policy.h"

#include <chrono>

namespace soundint::policy {
namespace {

constexpr TimeMs kNever = 0;  // sentinela de "nunca aconteceu"

wchar_t lowerAscii(wchar_t ch)
{
    if (ch >= L'A' && ch <= L'Z') {
        return static_cast<wchar_t>(ch - L'A' + L'a');
    }
    return ch;
}

}  // namespace

TimeMs now()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool iEqual(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (lowerAscii(a[i]) != lowerAscii(b[i])) {
            return false;
        }
    }
    return true;
}

bool iContains(const std::wstring& haystack, const std::wstring& needle)
{
    if (needle.empty() || needle.size() > haystack.size()) {
        return false;
    }
    const size_t limit = haystack.size() - needle.size();
    for (size_t start = 0; start <= limit; ++start) {
        size_t pos = 0;
        while (pos < needle.size() &&
               lowerAscii(haystack[start + pos]) == lowerAscii(needle[pos])) {
            ++pos;
        }
        if (pos == needle.size()) {
            return true;
        }
    }
    return false;
}

bool isSystemSounds(const SessionInfo& session)
{
    // Contrato: pid 0 = sons do sistema.
    return session.systemSounds || session.pid == 0;
}

bool hasRule(const std::optional<AppRule>& rule)
{
    return rule.has_value() && rule->enabled;
}

bool popupEnabled(bool setting)
{
    return setting;
}

TimeMs cooldownRemaining(TimeMs nowMs, TimeMs lastMs, TimeMs cooldownMs)
{
    if (lastMs == kNever || cooldownMs <= 0) {
        return 0;
    }
    const TimeMs remaining = (lastMs + cooldownMs) - nowMs;
    return remaining > 0 ? remaining : 0;
}

Decision decideSession(const SessionInfo& session,
                       const std::optional<AppRule>& rule,
                       bool popupSetting,
                       TimeMs nowMs,
                       TimeMs lastPopupMs,
                       TimeMs cooldownMs)
{
    if (isSystemSounds(session)) {
        return Decision::Ignore;
    }
    if (hasRule(rule)) {
        return Decision::ApplyRuleSilently;
    }
    if (!popupEnabled(popupSetting)) {
        return Decision::Ignore;
    }
    if (cooldownRemaining(nowMs, lastPopupMs, cooldownMs) > 0) {
        return Decision::SuppressCooldown;
    }
    return Decision::AskPopup;
}

Decision decideDevice(bool arrivalMatched,
                      bool popupSetting,
                      TimeMs nowMs,
                      TimeMs lastPopupMs,
                      TimeMs cooldownMs)
{
    if (arrivalMatched) {
        return Decision::Ignore;
    }
    if (!popupEnabled(popupSetting)) {
        return Decision::Ignore;
    }
    if (cooldownRemaining(nowMs, lastPopupMs, cooldownMs) > 0) {
        return Decision::SuppressCooldown;
    }
    return Decision::AskPopup;
}

const ArrivalRule* matchArrival(const std::vector<ArrivalRule>& rules,
                                const std::wstring& friendlyName)
{
    for (const ArrivalRule& rule : rules) {
        if (!rule.enabled) {
            continue;
        }
        if (iContains(friendlyName, rule.deviceNamePattern)) {
            return &rule;
        }
    }
    return nullptr;
}

// --- Debouncer ---------------------------------------------------------------

TimeMs Debouncer::poke(const std::wstring& key, TimeMs nowMs)
{
    const TimeMs deadline = nowMs + intervalMs_;
    deadlines_[key] = deadline;
    return deadline;
}

TimeMs Debouncer::remaining(const std::wstring& key, TimeMs nowMs) const
{
    const auto it = deadlines_.find(key);
    if (it == deadlines_.end()) {
        return 0;
    }
    const TimeMs left = it->second - nowMs;
    return left > 0 ? left : 0;
}

bool Debouncer::consumeDue(const std::wstring& key, TimeMs nowMs)
{
    const auto it = deadlines_.find(key);
    if (it == deadlines_.end()) {
        return false;
    }
    if (it->second > nowMs) {
        return false;  // janela ainda aberta
    }
    deadlines_.erase(it);
    return true;
}

bool Debouncer::has(const std::wstring& key) const
{
    return deadlines_.find(key) != deadlines_.end();
}

void Debouncer::erase(const std::wstring& key)
{
    deadlines_.erase(key);
}

void Debouncer::clear()
{
    deadlines_.clear();
}

// --- CooldownTracker ---------------------------------------------------------

TimeMs CooldownTracker::last(const std::wstring& key) const
{
    const auto it = last_.find(key);
    return it == last_.end() ? kNever : it->second;
}

TimeMs CooldownTracker::remaining(const std::wstring& key, TimeMs nowMs) const
{
    return cooldownRemaining(nowMs, last(key), cooldownMs_);
}

bool CooldownTracker::tryAcquire(const std::wstring& key, TimeMs nowMs)
{
    if (remaining(key, nowMs) > 0) {
        return false;
    }
    last_[key] = nowMs;
    return true;
}

void CooldownTracker::mark(const std::wstring& key, TimeMs nowMs)
{
    last_[key] = nowMs;
}

void CooldownTracker::erase(const std::wstring& key)
{
    last_.erase(key);
}

void CooldownTracker::clear()
{
    last_.clear();
}

}  // namespace soundint::policy
