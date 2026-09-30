// ============================================================================
// Parse do update-manifest.json (Track F). JSON em UTF-8 (nlohmann).
// ============================================================================
#pragma once

#include <string>
#include <string_view>

#include "update/update_service.h"

namespace soundint::update {

// Converte UTF-8 (corpo HTTP/JSON) para wstring (UTF-16).
std::wstring utf8ToWide(std::string_view text);

// Parseia o JSON do manifesto. Em sucesso devolve manifest.valid=true; em
// falha devolve manifest com valid=false e preenche *error (se fornecido).
Manifest parseManifest(std::string_view json, std::wstring* error = nullptr);

}  // namespace soundint::update
