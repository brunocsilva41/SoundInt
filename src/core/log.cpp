// Log em arquivo UTF-8, thread-safe, com rotacao (2 MB, 5 backups) e
// espelho para OutputDebugStringW. Uma linha por chamada:
//   YYYY-MM-DD HH:MM:SS.mmm [NOME] [canal] mensagem\r\n
#include <cstdint>  // log.h usa uint8_t sem incluir; garantir antes do header.

#include "core/log.h"

#include <Windows.h>
#include <shlobj.h>

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace soundint::log {

namespace {

constexpr uint64_t kMaxBytes = 2ull * 1024ull * 1024ull;  // rotaciona ao ultrapassar
constexpr int kMaxBackups = 5;                            // .log.1 .. .log.5
constexpr wchar_t kFileName[] = L"SoundInt.log";

// Singleton com vida de processo (nao destruido): evita dependencia da ordem
// de destruicao de estaticos quando outros modulos logam durante o shutdown.
struct LogState {
    std::mutex mutex;
    std::ofstream file;
    std::wstring path;
    uint64_t bytes = 0;
    bool open = false;
};

LogState& state()
{
    static LogState* s = new LogState();
    return *s;
}

const char* levelName(Level level)
{
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "INFO";
}

std::string toUtf8(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

std::wstring fromUtf8(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int needed =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                            nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), needed);
    return out;
}

// "YYYY-MM-DD HH:MM:SS.mmm" no horario local.
std::string timestamp()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  static_cast<unsigned>(st.wYear),
                  static_cast<unsigned>(st.wMonth),
                  static_cast<unsigned>(st.wDay),
                  static_cast<unsigned>(st.wHour),
                  static_cast<unsigned>(st.wMinute),
                  static_cast<unsigned>(st.wSecond),
                  static_cast<unsigned>(st.wMilliseconds));
    return buf;
}

std::string buildLine(Level level, const char* channel, std::string_view message)
{
    std::string line;
    line.reserve(64 + message.size());
    line += timestamp();
    line += " [";
    line += levelName(level);
    line += "] [";
    line += (channel != nullptr) ? channel : "";
    line += "] ";
    line += message;
    line += "\r\n";
    return line;
}

// Rotaciona: .4→.5 ... .1→.2, atual→.1, e recria o arquivo vazio.
void rotateLocked(LogState& s)
{
    s.file.flush();
    s.file.close();
    s.open = false;

    for (int i = kMaxBackups - 1; i >= 1; --i) {
        const std::wstring from = s.path + L"." + std::to_wstring(i);
        const std::wstring to = s.path + L"." + std::to_wstring(i + 1);
        if (GetFileAttributesW(from.c_str()) != INVALID_FILE_ATTRIBUTES) {
            MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
    const std::wstring first = s.path + L"." + std::to_wstring(1);
    MoveFileExW(s.path.c_str(), first.c_str(), MOVEFILE_REPLACE_EXISTING);

    s.file.open(s.path, std::ios::out | std::ios::binary | std::ios::trunc);
    s.open = s.file.is_open();
    s.bytes = 0;
}

// Escreve a linha no arquivo (chamar com o mutex segurado).
void appendLocked(LogState& s, const std::string& line)
{
    if (!s.open) {
        return;
    }
    s.file.write(line.data(), static_cast<std::streamsize>(line.size()));
    s.bytes += line.size();
    if (s.file.good()) {
        s.file.flush();
    } else {
        s.file.clear();  // disco cheio/fechado: nao derruba a proxima chamada
    }
    if (s.bytes > kMaxBytes) {
        rotateLocked(s);
    }
}

void writeLine(Level level, const char* channel, std::string_view message)
{
    const std::string line = buildLine(level, channel, message);
    {
        LogState& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        appendLocked(s, line);
    }
    // Espelho para o debugger, fora do lock (pode ser lento).
    const std::wstring wide = fromUtf8(line);
    if (!wide.empty()) {
        OutputDebugStringW(wide.c_str());
    }
}

}  // namespace

void init(const std::wstring& dir)
{
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.file.is_open()) {
        s.file.flush();
        s.file.close();
    }
    s.open = false;
    s.bytes = 0;
    s.path.clear();

    if (dir.empty()) {
        return;  // sem diretorio nao ha arquivo (evita criar na raiz do drive)
    }
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    s.path = dir + L"\\" + kFileName;
    s.file.open(s.path, std::ios::out | std::ios::app | std::ios::binary);
    if (s.file.is_open()) {
        s.file.seekp(0, std::ios::end);
        const std::streampos pos = s.file.tellp();
        s.bytes = pos > 0 ? static_cast<uint64_t>(pos) : 0;
        s.open = true;
    }
}

void shutdown()
{
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (s.file.is_open()) {
        s.file.flush();
        s.file.close();
    }
    s.open = false;
}

void write(Level level, const char* channel, const std::wstring& message)
{
    writeLine(level, channel, toUtf8(message));
}

void write(Level level, const char* channel, const char* message)
{
    // Mensagem char ja e UTF-8; copia direto para a linha do arquivo.
    writeLine(level, channel, std::string_view(message != nullptr ? message : ""));
}

std::wstring currentFilePath()
{
    LogState& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.path;
}

}  // namespace soundint::log
