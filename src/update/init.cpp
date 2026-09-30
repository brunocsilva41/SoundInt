#include "update/update_service.h"
#include "soundint/version.h"

// STUB Wave 0 — Track F substitui por: WinHTTP (manifest + download),
// BCrypt (SHA256) e spawn do instalador (/SILENT).

namespace soundint::update {

std::wstring installedVersion()
{
    return L"" SOUNDINT_VERSION_STRING;
}

CheckResult checkForUpdates(bool /*betaChannel*/)
{
    CheckResult r;
    r.error = L"updater nao implementado (Wave 0)";
    return r;
}

DownloadResult download(const Manifest& /*manifest*/, ProgressFn /*progress*/)
{
    DownloadResult r;
    r.error = L"updater nao implementado (Wave 0)";
    return r;
}

bool launchInstaller(const std::wstring& /*installerPath*/)
{
    return false;
}

}  // namespace soundint::update
