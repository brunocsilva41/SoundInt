// Parsing/comparacao SemVer (spec 2.0.0). Pre-release segue a precedencia:
// alpha < beta < rc < (release); identificador numerico < nao numerico.
#include "core/semver.h"

#include <climits>
#include <vector>

namespace soundint {

namespace {

bool isSpace(wchar_t c)
{
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
}

bool isDigit(wchar_t c)
{
    return c >= L'0' && c <= L'9';
}

bool isIdentChar(wchar_t c)
{
    return isDigit(c) || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
           c == L'-';
}

// Le um numero nao vazio em text a partir de pos (sem sinal, sem overflow).
bool parseUint(std::wstring_view text, size_t& pos, int& out)
{
    if (pos >= text.size() || !isDigit(text[pos])) {
        return false;
    }
    long long value = 0;
    while (pos < text.size() && isDigit(text[pos])) {
        value = value * 10 + (text[pos] - L'0');
        if (value > INT_MAX) {
            return false;
        }
        ++pos;
    }
    out = static_cast<int>(value);
    return true;
}

void split(std::wstring_view text, wchar_t sep, std::vector<std::wstring_view>& out)
{
    out.clear();
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == sep) {
            out.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
}

bool allDigits(std::wstring_view s)
{
    if (s.empty()) {
        return false;
    }
    for (wchar_t c : s) {
        if (!isDigit(c)) {
            return false;
        }
    }
    return true;
}

// Zeros a esquerda nao mudam o valor numerico.
std::wstring_view stripZeros(std::wstring_view s)
{
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == L'0') {
        ++i;
    }
    return s.substr(i);
}

// Compara um par de identificadores de pre-release.
int compareIdentifier(std::wstring_view a, std::wstring_view b)
{
    const bool aNum = allDigits(a);
    const bool bNum = allDigits(b);
    if (aNum && bNum) {
        const std::wstring_view za = stripZeros(a);
        const std::wstring_view zb = stripZeros(b);
        if (za.size() != zb.size()) {
            return za.size() < zb.size() ? -1 : 1;
        }
        if (za == zb) {
            return 0;
        }
        return za < zb ? -1 : 1;
    }
    if (aNum != bNum) {
        return aNum ? -1 : 1;  // numerico sempre precede o nao numerico
    }
    if (a == b) {
        return 0;
    }
    return a < b ? -1 : 1;  // ordem lexicografica ASCII
}

}  // namespace

bool Version::parse(std::wstring_view text, Version& out)
{
    // Tolerancia: espacos nas bordas e prefixo "v"/"V" opcional.
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    if (!text.empty() && (text.front() == L'v' || text.front() == L'V')) {
        text.remove_prefix(1);
    }

    Version v;
    size_t pos = 0;
    if (!parseUint(text, pos, v.major)) {
        return false;
    }
    if (pos >= text.size() || text[pos] != L'.') {
        return false;
    }
    ++pos;
    if (!parseUint(text, pos, v.minor)) {
        return false;
    }
    if (pos >= text.size() || text[pos] != L'.') {
        return false;
    }
    ++pos;
    if (!parseUint(text, pos, v.patch)) {
        return false;
    }

    // Pre-release: identificadores separados por ponto ([0-9A-Za-z-]+).
    if (pos < text.size() && text[pos] == L'-') {
        ++pos;
        std::wstring identifier;
        while (true) {
            const size_t start = pos;
            while (pos < text.size() && isIdentChar(text[pos])) {
                ++pos;
            }
            if (pos == start) {
                return false;  // identificador vazio
            }
            if (!identifier.empty()) {
                identifier.push_back(L'.');
            }
            identifier.append(text.substr(start, pos - start));
            if (pos >= text.size() || text[pos] != L'.') {
                break;
            }
            ++pos;
        }
        v.preRelease = std::move(identifier);
    }

    // Build metadata ("+...") e aceita e descartada (sem campo no struct).
    if (pos < text.size() && text[pos] == L'+') {
        ++pos;
        const size_t start = pos;
        while (pos < text.size() && (isIdentChar(text[pos]) || text[pos] == L'.')) {
            ++pos;
        }
        if (pos == start) {
            return false;  // build vazio
        }
    }

    if (pos != text.size()) {
        return false;  // sobrou lixo
    }
    out = v;
    return true;
}

bool Version::parseUtf8(std::string_view text, Version& out)
{
    // Strings de versao sao ASCII por spec; bytes altos viram caracteres
    // invalidos e o parse comum rejeita.
    std::wstring wide;
    wide.reserve(text.size());
    for (unsigned char c : text) {
        wide.push_back(static_cast<wchar_t>(c));
    }
    return parse(wide, out);
}

int Version::compare(const Version& a, const Version& b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }

    const bool aPre = !a.preRelease.empty();
    const bool bPre = !b.preRelease.empty();
    if (!aPre && !bPre) {
        return 0;
    }
    if (!aPre) {
        return 1;  // release estable > pre-release
    }
    if (!bPre) {
        return -1;
    }

    std::vector<std::wstring_view> idsA;
    std::vector<std::wstring_view> idsB;
    split(a.preRelease, L'.', idsA);
    split(b.preRelease, L'.', idsB);
    const size_t count = idsA.size() < idsB.size() ? idsA.size() : idsB.size();
    for (size_t i = 0; i < count; ++i) {
        const int cmp = compareIdentifier(idsA[i], idsB[i]);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (idsA.size() == idsB.size()) {
        return 0;
    }
    return idsA.size() < idsB.size() ? -1 : 1;  // menos identificadores < mais
}

std::wstring Version::toString() const
{
    std::wstring text = std::to_wstring(major);
    text.push_back(L'.');
    text += std::to_wstring(minor);
    text.push_back(L'.');
    text += std::to_wstring(patch);
    if (!preRelease.empty()) {
        text.push_back(L'-');
        text += preRelease;
    }
    return text;
}

}  // namespace soundint
