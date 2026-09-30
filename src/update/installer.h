// ============================================================================
// Spawn do instalador (Track F): modo silencioso NSIS.
// ============================================================================
#pragma once

#include <string>

namespace soundint::update::installer {

// Spawna o instalador com "/SILENT /SP- /NOCANCEL". Retorna true se o
// processo foi criado; em falha preenche *error (se fornecido).
bool spawn(const std::wstring& installerPath, std::wstring* error = nullptr);

}  // namespace soundint::update::installer
