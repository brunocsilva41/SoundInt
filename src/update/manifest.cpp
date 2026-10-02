// ============================================================================
// Parse do update-manifest.json (Track F) — nlohmann/json, campos UTF-8.
// ============================================================================
#include "update/manifest.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include "update/sha256.h"

namespace soundint::update {

std::wstring utf8ToWide(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), length);
    return out;
}

namespace {

// Le uma obrigatoria do JSON: string nao-vazia.
bool requiredString(const nlohmann::json& node, const char* key, std::wstring& out,
                    std::wstring& error)
{
    auto it = node.find(key);
    if (it == node.end()) {
        error = L"manifest: campo obrigatorio ausente: " + utf8ToWide(key);
        return false;
    }
    if (!it->is_string()) {
        error = L"manifest: campo invalido (esperado string): " + utf8ToWide(key);
        return false;
    }
    const std::string value = it->get<std::string>();
    if (value.empty()) {
        error = L"manifest: campo obrigatorio vazio: " + utf8ToWide(key);
        return false;
    }
    out = utf8ToWide(value);
    return true;
}

// Campo opcional string (vazio se ausente/invalido).
std::wstring optionalString(const nlohmann::json& node, const char* key)
{
    auto it = node.find(key);
    if (it == node.end() || !it->is_string()) {
        return {};
    }
    return utf8ToWide(it->get<std::string>());
}

}  // namespace

Manifest parseManifest(std::string_view json, std::wstring* error)
{
    Manifest manifest;
    std::wstring localError;

    auto fail = [&localError, error](std::wstring message) {
        localError = std::move(message);
        if (error != nullptr) {
            *error = localError;
        }
        return Manifest{};
    };

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json.begin(), json.end());
    } catch (const nlohmann::json::exception& ex) {
        return fail(L"manifest: JSON invalido (" + utf8ToWide(ex.what()) + L")");
    }
    if (!root.is_object()) {
        return fail(L"manifest: a raiz deve ser um objeto JSON");
    }

    try {
        std::wstring version;
        std::wstring tag;
        std::wstring url;
        std::wstring sha;
        if (!requiredString(root, "version", version, localError) ||
            !requiredString(root, "tag", tag, localError) ||
            !requiredString(root, "url", url, localError) ||
            !requiredString(root, "sha256", sha, localError)) {
            if (error != nullptr) {
                *error = localError;
            }
            return Manifest{};
        }

        auto sizeIt = root.find("size");
        if (sizeIt == root.end() || !sizeIt->is_number_integer()) {
            return fail(L"manifest: campo obrigatorio ausente/invalido: size");
        }
        const std::int64_t size = sizeIt->get<std::int64_t>();
        if (size <= 0) {
            return fail(L"manifest: size deve ser maior que zero");
        }

        if (url.rfind(L"https://", 0) != 0) {
            return fail(L"manifest: url deve comecar com https://");
        }

        // Charset restrito: version entra no nome do arquivo baixado
        // (sem separadores nao ha path traversal alem de pontos).
        for (const wchar_t ch : version) {
            const bool ok = (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'z') ||
                            (ch >= L'A' && ch <= L'Z') || ch == L'.' || ch == L'-' ||
                            ch == L'+';
            if (!ok) {
                return fail(L"manifest: version contem caractere invalido");
            }
        }
        if (version.empty()) {
            return fail(L"manifest: version vazio");
        }

        sha = toHexLowerAscii(sha);
        if (!isSha256Hex(sha)) {
            return fail(L"manifest: sha256 invalido (esperado 64 chars hex)");
        }

        manifest.version = std::move(version);
        manifest.tag = std::move(tag);
        manifest.url = std::move(url);
        manifest.sha256 = std::move(sha);
        manifest.sizeBytes = static_cast<std::uint64_t>(size);
        manifest.notes = optionalString(root, "notes");
        manifest.prerelease = root.value("prerelease", false);
        manifest.valid = true;
        if (error != nullptr) {
            error->clear();
        }
        return manifest;
    } catch (const nlohmann::json::exception& ex) {
        return fail(L"manifest: erro ao ler campo (" + utf8ToWide(ex.what()) + L")");
    }
}

}  // namespace soundint::update
