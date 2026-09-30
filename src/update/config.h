// ============================================================================
// Configuracao do updater (Track F). Nao faz parte do contrato Wave 0.
// ============================================================================
#pragma once

namespace soundint::update {

// AJUSTAR na integracao: slug do repositorio GitHub que hospeda as releases.
inline constexpr wchar_t kRepoSlug[] = L"brunocsilva41/SoundInt";

// User-Agent de todos os requests (a API do GitHub exige um UA proprio).
inline constexpr wchar_t kUserAgent[] = L"SoundInt-Updater";

// Timeout (ms) de resolve/connect/send/receive — contrato: 15s.
inline constexpr int kHttpTimeoutMs = 15000;

// Asset do manifest dentro de cada release.
inline constexpr wchar_t kManifestAssetName[] = L"update-manifest.json";
inline constexpr char kManifestAssetNameA[] = "update-manifest.json";

}  // namespace soundint::update
