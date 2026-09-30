// ============================================================================
// SHA-256 via BCrypt (Track F): hash em streaming -> hex minusculo.
// ============================================================================
#include "update/sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <string>
#include <vector>

namespace soundint::update {
namespace {

constexpr wchar_t kHexDigits[] = L"0123456789abcdef";

// Converte bytes brutos para hex minusculo.
std::wstring bytesToHex(const unsigned char* data, std::size_t size)
{
    std::wstring text;
    text.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        text.push_back(kHexDigits[(data[i] >> 4) & 0x0F]);
        text.push_back(kHexDigits[data[i] & 0x0F]);
    }
    return text;
}

// Codigo NT em hex (erros do BCrypt vem como NTSTATUS).
std::wstring ntStatusText(long status)
{
    const unsigned long value = static_cast<unsigned long>(status);
    std::wstring text = L"0x";
    for (int shift = 28; shift >= 0; shift -= 4) {
        text.push_back(kHexDigits[(value >> shift) & 0x0F]);
    }
    return text;
}

bool isHexChar(wchar_t ch)
{
    return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
}

// Hash SHA-256 reutilizavel (init/update/finish) com cleanup automatico.
class Sha256Hash {
public:
    Sha256Hash() = default;
    ~Sha256Hash()
    {
        if (m_hash != nullptr) {
            BCryptDestroyHash(m_hash);
        }
        if (m_algorithm != nullptr) {
            BCryptCloseAlgorithmProvider(m_algorithm, 0);
        }
    }
    Sha256Hash(const Sha256Hash&) = delete;
    Sha256Hash& operator=(const Sha256Hash&) = delete;

    bool init(std::wstring& error)
    {
        NTSTATUS status =
            BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (status < 0) {
            error = L"BCryptOpenAlgorithmProvider falhou (" + ntStatusText(status) + L")";
            return false;
        }

        ULONG objectSize = 0;
        ULONG copied = 0;
        status = BCryptGetProperty(m_algorithm, BCRYPT_OBJECT_LENGTH,
                                   reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
                                   &copied, 0);
        if (status < 0) {
            error = L"BCryptGetProperty(object) falhou (" + ntStatusText(status) + L")";
            return false;
        }

        ULONG hashSize = 0;
        status = BCryptGetProperty(m_algorithm, BCRYPT_HASH_LENGTH,
                                   reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &copied,
                                   0);
        if (status < 0 || hashSize == 0) {
            error = L"BCryptGetProperty(hash) falhou (" + ntStatusText(status) + L")";
            return false;
        }

        m_object.resize(objectSize);
        m_hashBytes.resize(hashSize);
        status = BCryptCreateHash(m_algorithm, &m_hash, m_object.data(), objectSize, nullptr, 0, 0);
        if (status < 0) {
            error = L"BCryptCreateHash falhou (" + ntStatusText(status) + L")";
            return false;
        }
        return true;
    }

    bool update(const void* data, std::size_t size, std::wstring& error)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::size_t offset = 0;
        while (offset < size) {
            const std::size_t chunk = (size - offset > 0x40000000u) ? 0x40000000u : (size - offset);
            const NTSTATUS status = BCryptHashData(
                m_hash, const_cast<PUCHAR>(bytes + offset), static_cast<ULONG>(chunk), 0);
            if (status < 0) {
                error = L"BCryptHashData falhou (" + ntStatusText(status) + L")";
                return false;
            }
            offset += chunk;
        }
        return true;
    }

    bool finish(std::wstring& hex, std::wstring& error)
    {
        const NTSTATUS status = BCryptFinishHash(m_hash, m_hashBytes.data(),
                                                  static_cast<ULONG>(m_hashBytes.size()), 0);
        if (status < 0) {
            error = L"BCryptFinishHash falhou (" + ntStatusText(status) + L")";
            return false;
        }
        hex = bytesToHex(m_hashBytes.data(), m_hashBytes.size());
        return true;
    }

private:
    BCRYPT_ALG_HANDLE m_algorithm = nullptr;
    BCRYPT_HASH_HANDLE m_hash = nullptr;
    std::vector<unsigned char> m_object;
    std::vector<unsigned char> m_hashBytes;
};

}  // namespace

std::wstring sha256Buffer(const void* data, std::size_t size)
{
    Sha256Hash hash;
    std::wstring error;
    std::wstring hex;
    if (!hash.init(error) || !hash.update(data, size, error) || !hash.finish(hex, error)) {
        OutputDebugStringW((L"SoundInt updater: " + error + L"\n").c_str());
        return {};
    }
    return hex;
}

std::wstring sha256File(const std::wstring& path, std::wstring* error)
{
    auto fail = [error](const std::wstring& message) {
        if (error != nullptr) {
            *error = message;
        }
        return std::wstring{};
    };

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        return fail(L"nao foi possivel abrir " + path + L" (erro " + std::to_wstring(code) + L")");
    }

    Sha256Hash hash;
    std::wstring localError;
    if (!hash.init(localError)) {
        CloseHandle(file);
        return fail(localError);
    }

    std::vector<unsigned char> buffer(64 * 1024);
    bool readOk = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            const DWORD code = GetLastError();
            readOk = false;
            localError = L"falha ao ler " + path + L" (erro " + std::to_wstring(code) + L")";
            break;
        }
        if (read == 0) {
            break;
        }
        if (!hash.update(buffer.data(), read, localError)) {
            readOk = false;
            break;
        }
    }
    CloseHandle(file);
    if (!readOk) {
        return fail(localError);
    }
    // Arquivo vazio: update nao e chamado e o finish devolve o hash do vazio.

    std::wstring hex;
    if (!hash.finish(hex, localError)) {
        return fail(localError);
    }
    if (error != nullptr) {
        error->clear();
    }
    return hex;
}

bool isSha256Hex(std::wstring_view text)
{
    if (text.size() != 64) {
        return false;
    }
    for (wchar_t ch : text) {
        if (!isHexChar(ch)) {
            return false;
        }
    }
    return true;
}

std::wstring toHexLowerAscii(std::wstring_view text)
{
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch >= L'A' && ch <= L'Z') {
            out.push_back(static_cast<wchar_t>(ch - L'A' + L'a'));
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

}  // namespace soundint::update
