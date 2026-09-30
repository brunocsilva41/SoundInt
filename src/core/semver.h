// ============================================================================
// CONTRACT — parsing/comparacao SemVer (1.2.3-beta.1). Track C implementa,
// Track F (updater) consome.
// ============================================================================
#pragma once

#include <string>
#include <string_view>

namespace soundint {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::wstring preRelease;  // vazio se release estable

    static bool parse(std::wstring_view text, Version& out);
    static bool parseUtf8(std::string_view text, Version& out);

    // <0 se a<b, 0 se iguais, >0 se a>b. Regra SemVer com precedencia de
    // pre-release (beta < estable).
    static int compare(const Version& a, const Version& b);

    std::wstring toString() const;
};

}  // namespace soundint
