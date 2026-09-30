// ============================================================================
// SHA-256 via BCrypt (Track F): hash de buffer/arquivo -> hex minusculo.
// ============================================================================
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace soundint::update {

// SHA-256 de um buffer em memoria -> hex minusculo (64 chars).
std::wstring sha256Buffer(const void* data, std::size_t size);

// SHA-256 de arquivo em streaming (le em blocos) -> hex minusculo.
// Em falha retorna vazio e preenche *error (se fornecido).
std::wstring sha256File(const std::wstring& path, std::wstring* error = nullptr);

// true se text tem exatamente 64 chars hexadecimais (qualquer caixa).
bool isSha256Hex(std::wstring_view text);

// Converte para hex minusculo (normaliza o campo sha256 do manifest).
std::wstring toHexLowerAscii(std::wstring_view text);

}  // namespace soundint::update
