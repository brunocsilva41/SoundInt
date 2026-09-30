// ============================================================================
// WinHTTP (Track F): GET com redirect automatico e download com progresso.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "update/update_service.h"

namespace soundint::update::http {

struct FetchResult {
    bool ok = false;
    unsigned statusCode = 0;
    std::string body;   // corpo em UTF-8 (JSON/manifesto)
    std::wstring error; // vazio = ok
};

// GET com redirect automatico (github.com -> objects.githubusercontent.com),
// UA fixo e timeout de 15s. Corpo devolvido como UTF-8.
FetchResult fetchString(const std::wstring& url);

// Baixa a URL para destPath (substitui o arquivo). progress (0..1) e opcional;
// expectedSize serve de pista quando o servidor nao envia Content-Length.
// Em falha retorna false e preenche *error (se fornecido).
bool downloadFile(const std::wstring& url, const std::wstring& destPath, const ProgressFn& progress,
                  std::uint64_t expectedSize, std::wstring* error);

}  // namespace soundint::update::http
