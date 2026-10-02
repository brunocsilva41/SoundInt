// ============================================================================
// WinHTTP (Track F): GET com redirect automatico e download com progresso.
// ============================================================================
#include "update/http_win.h"

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

#include "update/config.h"

namespace soundint::update::http {
namespace {

// Handle WinHTTP com fechamento automatico.
struct HttpHandle {
    HINTERNET handle = nullptr;

    HttpHandle() = default;
    ~HttpHandle() { reset(); }
    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;

    void reset()
    {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
            handle = nullptr;
        }
    }

    explicit operator bool() const { return handle != nullptr; }
};

// Handle de arquivo com fechamento automatico.
struct FileHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;

    FileHandle() = default;
    ~FileHandle()
    {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    explicit operator bool() const { return handle != INVALID_HANDLE_VALUE; }
};

// Texto legivel de um erro do sistema.
std::wstring systemErrorText(DWORD code)
{
    wchar_t buffer[512] = {};
    const DWORD capacity = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, 0, buffer, capacity, nullptr);

    std::wstring text;
    if (length > 0) {
        text.assign(buffer, length);
    } else {
        text = L"erro " + std::to_wstring(code);
    }
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) {
        text.pop_back();
    }
    return text;
}

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
    bool https = false;
};

// Decompoe a URL em host/caminho/porta (WinHttpCrackUrl).
bool crackUrl(const std::wstring& url, ParsedUrl& out, std::wstring& error)
{
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        error = L"URL invalida: " + url;
        return false;
    }
    if (components.lpszHostName == nullptr || components.dwHostNameLength == 0) {
        error = L"URL sem host: " + url;
        return false;
    }

    out.host.assign(components.lpszHostName, components.dwHostNameLength);
    out.port = components.nPort;
    out.https = (components.nScheme == INTERNET_SCHEME_HTTPS);
    out.path.clear();
    if (components.lpszUrlPath != nullptr && components.dwUrlPathLength > 0) {
        out.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    }
    if (components.lpszExtraInfo != nullptr && components.dwExtraInfoLength > 0) {
        out.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (out.path.empty()) {
        out.path = L"/";
    }
    return true;
}

struct Session {
    HttpHandle session;
    HttpHandle connection;
    HttpHandle request;
    std::wstring error;
};

// Abre sessao/conexao/requisicao GET (UA proprio, timeout 15s, redirects ON).
bool openRequest(const std::wstring& url, Session& out)
{
    ParsedUrl parsed;
    if (!crackUrl(url, parsed, out.error)) {
        return false;
    }

    out.session.handle =
        WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!out.session) {
        out.error = L"WinHttpOpen falhou: " + systemErrorText(GetLastError());
        return false;
    }

    const DWORD timeout = static_cast<DWORD>(kHttpTimeoutMs);
    WinHttpSetTimeouts(out.session.handle, timeout, timeout, timeout, timeout);

    out.connection.handle = WinHttpConnect(out.session.handle, parsed.host.c_str(), parsed.port, 0);
    if (!out.connection) {
        out.error = L"WinHttpConnect falhou: " + systemErrorText(GetLastError());
        return false;
    }

    const DWORD flags = parsed.https ? WINHTTP_FLAG_SECURE : 0;
    out.request.handle =
        WinHttpOpenRequest(out.connection.handle, L"GET", parsed.path.c_str(), nullptr,
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!out.request) {
        out.error = L"WinHttpOpenRequest falhou: " + systemErrorText(GetLastError());
        return false;
    }

    // Segue redirecionamentos (302 de releases/latest -> objetos do GitHub).
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(out.request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
                     sizeof(redirectPolicy));
    return true;
}

// Envia o GET e espera a resposta final (redirects ja resolvidos).
bool sendRequest(Session& out)
{
    if (!WinHttpSendRequest(out.request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        out.error = L"WinHttpSendRequest falhou: " + systemErrorText(GetLastError());
        return false;
    }
    if (!WinHttpReceiveResponse(out.request.handle, nullptr)) {
        out.error = L"WinHttpReceiveResponse falhou: " + systemErrorText(GetLastError());
        return false;
    }
    return true;
}

// Codigo HTTP final (0 se nao for possivel consultar).
unsigned statusCode(HINTERNET request)
{
    DWORD status = 0;
    DWORD size = sizeof(status);
    const DWORD query = WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER;
    if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return static_cast<unsigned>(status);
}

// Le o corpo inteiro para uma string (UTF-8).
bool readAll(HINTERNET request, std::string& body, std::wstring& error)
{
    // Teto para API/manifesto: 1 MiB. Sem ele, `available` vem do servidor
    // e body.append pode estourar a memoria (bad_alloc na thread de update
    // sem handler = std::terminate no app inteiro).
    constexpr std::size_t kMaxBody = 1u << 20;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = L"falha ao ler resposta: " + systemErrorText(GetLastError());
            return false;
        }
        if (available == 0) {
            return true;
        }
        if (body.size() + static_cast<std::size_t>(available) > kMaxBody) {
            error = L"resposta HTTP maior que o limite de 1 MiB";
            return false;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            error = L"falha ao ler resposta: " + systemErrorText(GetLastError());
            return false;
        }
        if (read == 0) {
            return true;
        }
        body.append(buffer.data(), read);
    }
}

}  // namespace

FetchResult fetchString(const std::wstring& url)
{
    FetchResult result;

    Session session;
    if (!openRequest(url, session) || !sendRequest(session)) {
        result.error = session.error;
        return result;
    }

    result.statusCode = statusCode(session.request.handle);
    if (result.statusCode != 200) {
        result.error = L"HTTP " + std::to_wstring(result.statusCode) + L" ao consultar " + url;
        return result;
    }
    if (!readAll(session.request.handle, result.body, result.error)) {
        return result;
    }

    result.ok = true;
    return result;
}

bool downloadFile(const std::wstring& url, const std::wstring& destPath, const ProgressFn& progress,
                  std::uint64_t expectedSize, std::wstring* error)
{
    auto fail = [error](const std::wstring& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };

    Session session;
    if (!openRequest(url, session) || !sendRequest(session)) {
        return fail(session.error);
    }

    const unsigned status = statusCode(session.request.handle);
    if (status != 200) {
        return fail(L"HTTP " + std::to_wstring(status) + L" ao baixar " + url);
    }

    // Tamanho total: Content-Length, ou a pista vinda do manifesto.
    std::uint64_t total = 0;
    DWORD contentLength = 0;
    DWORD headerSize = sizeof(contentLength);
    const DWORD query = WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER;
    if (WinHttpQueryHeaders(session.request.handle, query, WINHTTP_HEADER_NAME_BY_INDEX,
                            &contentLength, &headerSize, WINHTTP_NO_HEADER_INDEX)) {
        total = contentLength;
    }
    if (total == 0) {
        total = expectedSize;
    }

    FileHandle file;
    file.handle = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!file) {
        const DWORD code = GetLastError();
        return fail(L"nao foi possivel criar " + destPath + L": " + systemErrorText(code));
    }

    std::uint64_t done = 0;
    // Teto do download e do aloc por leitura: servidor hostil nao enche
    // disco nem RAM. (Instalador real ~2,4 MB; 512 MB e folga enorme.)
    constexpr std::uint64_t kMaxDownload = 512ull << 20;
    constexpr DWORD kMaxChunk = 1u << 20;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(session.request.handle, &available)) {
            const DWORD code = GetLastError();
            return fail(L"falha ao baixar " + destPath + L": " + systemErrorText(code));
        }
        if (available == 0) {
            break;
        }
        if (available > kMaxChunk) {
            available = kMaxChunk;
        }

        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(session.request.handle, buffer.data(), available, &read)) {
            const DWORD code = GetLastError();
            return fail(L"falha ao baixar " + destPath + L": " + systemErrorText(code));
        }
        if (read == 0) {
            break;
        }

        if (done + read > kMaxDownload) {
            return fail(L"download excede o limite de 512 MB");
        }

        DWORD written = 0;
        if (!WriteFile(file.handle, buffer.data(), read, &written, nullptr) || written != read) {
            const DWORD code = GetLastError();
            return fail(L"falha ao gravar " + destPath + L": " + systemErrorText(code));
        }

        done += read;
        if (progress && total > 0) {
            double fraction = static_cast<double>(done) / static_cast<double>(total);
            progress((fraction > 1.0) ? 1.0 : fraction);
        }
    }

    if (progress) {
        progress(1.0);
    }
    return true;
}

}  // namespace soundint::update::http
