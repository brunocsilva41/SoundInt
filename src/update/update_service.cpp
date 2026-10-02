// ============================================================================
// Servico de atualizacao (Track F): checagem no GitHub Releases, download
// com validacao SHA256 e spawn silencioso do instalador.
// ============================================================================
#include "update/update_service.h"

#include <windows.h>

#include <cwctype>
#include <string>

#include <nlohmann/json.hpp>

#include "core/semver.h"
#include "soundint/version.h"
#include "update/config.h"
#include "update/http_win.h"
#include "update/installer.h"
#include "update/manifest.h"
#include "update/sha256.h"

// SOUNDINT_VERSION_STRING e string estreita; gera a literal larga correspondente.
#define SOUNDINT_WIDEN_IMPL(text) L##text
#define SOUNDINT_WIDEN(text) SOUNDINT_WIDEN_IMPL(text)

namespace soundint::update {
namespace {

// Compara hex sem importar caixa (o manifesto pode trazer ABCDEF).
bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

// API do GitHub: primeira release nao-draft e o browser_download_url do
// asset "update-manifest.json".
std::wstring manifestUrlFromApi(const std::string& body, std::wstring& error)
{
    try {
        const nlohmann::json root = nlohmann::json::parse(body);
        if (!root.is_array()) {
            error = L"API do GitHub devolveu resposta inesperada";
            return {};
        }

        for (const auto& release : root) {
            if (!release.is_object() || release.value("draft", false)) {
                continue;
            }

            std::string url;
            const auto assets = release.find("assets");
            if (assets != release.end() && assets->is_array()) {
                for (const auto& asset : *assets) {
                    if (!asset.is_object()) {
                        continue;
                    }
                    if (asset.value("name", std::string()) != kManifestAssetNameA) {
                        continue;
                    }
                    url = asset.value("browser_download_url", std::string());
                    break;
                }
            }

            if (url.empty()) {
                error = L"release mais recente nao tem o asset update-manifest.json";
                return {};
            }
            return utf8ToWide(url);
        }

        error = L"API do GitHub nao retornou nenhuma release publicada";
    } catch (const nlohmann::json::exception& ex) {
        error = L"JSON invalido da API do GitHub: " + utf8ToWide(ex.what());
    }
    return {};
}

}  // namespace

std::wstring installedVersion()
{
    return SOUNDINT_WIDEN(SOUNDINT_VERSION_STRING);
}
#undef SOUNDINT_WIDEN
#undef SOUNDINT_WIDEN_IMPL

CheckResult checkForUpdates(bool betaChannel)
{
    CheckResult result;

    std::wstring manifestUrl;
    if (betaChannel) {
        const std::wstring apiUrl =
            L"https://api.github.com/repos/" + std::wstring(kRepoSlug) + L"/releases";
        const http::FetchResult api = http::fetchString(apiUrl);
        if (!api.ok) {
            result.error = L"falha na API do GitHub: " + api.error;
            return result;
        }
        manifestUrl = manifestUrlFromApi(api.body, result.error);
        if (manifestUrl.empty()) {
            return result;
        }
    } else {
        // Releases latest: sem rate-limit de API e com redirect para o CDN.
        manifestUrl = L"https://github.com/" + std::wstring(kRepoSlug) +
                      L"/releases/latest/download/" + kManifestAssetName;
    }

    const http::FetchResult fetched = http::fetchString(manifestUrl);
    if (!fetched.ok) {
        result.error = L"falha ao baixar o manifesto: " + fetched.error;
        return result;
    }

    result.manifest = parseManifest(fetched.body, &result.error);
    if (!result.manifest.valid) {
        return result;
    }

    soundint::Version remote;
    soundint::Version local;
    if (!soundint::Version::parse(result.manifest.version, remote)) {
        result.error = L"versao remota invalida: " + result.manifest.version;
        result.manifest = Manifest{};  // nao deixar valid=true com versao invalida
        return result;
    }
    const std::wstring current = installedVersion();
    if (!soundint::Version::parse(current, local)) {
        result.error = L"versao instalada invalida: " + current;
        return result;
    }

    result.updateAvailable = soundint::Version::compare(remote, local) > 0;
    return result;
}

DownloadResult download(const Manifest& manifest, ProgressFn progress)
{
    DownloadResult result;

    if (!manifest.valid) {
        result.error = L"manifesto invalido";
        return result;
    }

    // Revalida antes de montar caminho de arquivo (defesa em profundidade:
    // version vem de JSON remoto e vira nome de arquivo em %TEMP%).
    soundint::Version parsedVersion;
    if (!soundint::Version::parse(manifest.version, parsedVersion)) {
        result.error = L"versao do manifesto invalida: " + manifest.version;
        return result;
    }

    wchar_t tempPath[MAX_PATH + 1] = {};
    const DWORD tempLength = GetTempPathW(MAX_PATH + 1, tempPath);
    if (tempLength == 0 || tempLength > MAX_PATH) {
        result.error = L"nao foi possivel obter o diretorio temporario";
        return result;
    }

    const std::wstring destPath = std::wstring(tempPath, tempLength) + L"SoundInt-Setup-" +
                                  manifest.version + L".exe";

    std::wstring downloadError;
    if (!http::downloadFile(manifest.url, destPath, progress, manifest.sizeBytes, &downloadError)) {
        DeleteFileW(destPath.c_str());
        result.error = L"falha no download: " + downloadError;
        return result;
    }

    std::wstring hashError;
    const std::wstring hash = sha256File(destPath, &hashError);
    if (hash.empty()) {
        DeleteFileW(destPath.c_str());
        result.error = L"falha ao calcular o SHA256: " + hashError;
        return result;
    }
    if (!equalsIgnoreCase(hash, manifest.sha256)) {
        DeleteFileW(destPath.c_str());
        result.error = L"SHA256 divergente: esperado " + manifest.sha256 + L", obtido " + hash;
        return result;
    }

    result.ok = true;
    result.installerPath = destPath;
    return result;
}

bool launchInstaller(const std::wstring& installerPath)
{
    std::wstring error;
    if (!installer::spawn(installerPath, &error)) {
        OutputDebugStringW((L"SoundInt updater: " + error + L"\n").c_str());
        return false;
    }
    return true;
}

}  // namespace soundint::update
