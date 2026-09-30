// ============================================================================
// CONTRACT — atualizador proprio via GitHub Releases. Track F implementa.
// Fonte de verdade: asset "update-manifest.json" da release latest
// (releases/latest/download/update-manifest.json — sem rate-limit de API).
// ============================================================================
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace soundint::update {

struct Manifest {
    std::wstring version;      // SemVer "1.2.3"
    std::wstring tag;          // "v1.2.3"
    std::wstring url;          // URL direta do instalador
    std::wstring sha256;       // hex minusculo
    std::uint64_t sizeBytes = 0;
    std::wstring notes;        // notas da versao
    bool prerelease = false;
    bool valid = false;
};

struct CheckResult {
    bool updateAvailable = false;
    Manifest manifest;
    std::wstring error;        // vazio = ok (mesmo sem update)
};

struct DownloadResult {
    bool ok = false;
    std::wstring installerPath;  // %TEMP%\SoundInt-Setup-<ver>.exe
    std::wstring error;
};

using ProgressFn = std::function<void(double fraction)>;  // 0..1, pode ser null

// Instala a versao em execucao (injetada via version.h).
std::wstring installedVersion();

// Baixa o manifest do canal (stable/beta) e compara com installedVersion().
CheckResult checkForUpdates(bool betaChannel);

// Baixa para o temp e valida SHA256 (BCrypt).
DownloadResult download(const Manifest& manifest, ProgressFn progress = nullptr);

// Spawna o instalador em modo silencioso. O app deve sair antes/depois.
bool launchInstaller(const std::wstring& installerPath);

}  // namespace soundint::update
