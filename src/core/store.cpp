// Persistencia do SoundInt: %LOCALAPPDATA%\SoundInt em JSON (UTF-8).
// Gravacao atomica (.tmp + MoveFileExW) e fallback para padroes quando o
// arquivo esta ausente ou corrompido. Mutex interno em todas as mutacoes.
#include "core/store.h"

#include "core/log.h"
#include "core/store_codec.h"

#include <Windows.h>
#include <shlobj.h>

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace soundint::core {

namespace {

using codec::SettingsDoc;

// Override usado pelos testes (aponta a raiz de dados para %TEMP%).
constexpr wchar_t kEnvDataDir[] = L"SOUNDINT_DATA_DIR";

constexpr wchar_t kSettingsFile[] = L"\\settings.json";
constexpr wchar_t kRulesFile[] = L"\\rules.json";
constexpr wchar_t kArrivalFile[] = L"\\arrival_rules.json";
constexpr wchar_t kProfilesFile[] = L"\\profiles.json";

enum class FileState { Missing, ReadOk, ReadError };

// %LOCALAPPDATA%\SoundInt (ou SOUNDINT_DATA_DIR, quando definido).
std::wstring resolveDataDir()
{
    wchar_t buf[MAX_PATH + 1] = {};
    const DWORD len = GetEnvironmentVariableW(kEnvDataDir, buf, MAX_PATH + 1);
    if (len > 0 && len <= MAX_PATH) {
        return std::wstring(buf, len);
    }

    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &raw))) {
        return {};
    }
    std::wstring dir(raw);
    CoTaskMemFree(raw);
    return dir + L"\\SoundInt";
}

// Cria o diretorio (e os pais) se nao existir. Idempotente.
void ensureDir(const std::wstring& path)
{
    if (path.empty()) {
        return;
    }
    const int rc = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS && rc != ERROR_FILE_EXISTS) {
        SI_LOG_WARN("store", "nao foi possivel criar o diretorio de dados");
    }
}

// Le o arquivo inteiro em UTF-8. Maximo de 64 MB (protecao contra arquivo bizarro).
bool readFileUtf8(const std::wstring& path, std::string& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart >= 0 && size.QuadPart <= 64 * 1024 * 1024;
    if (ok) {
        out.assign(static_cast<size_t>(size.QuadPart), '\0');
        const DWORD toRead = static_cast<DWORD>(out.size());
        DWORD read = 0;
        ok = toRead == 0 ||
             (ReadFile(h, out.data(), toRead, &read, nullptr) && read == toRead);
    }
    CloseHandle(h);
    if (!ok) {
        out.clear();
    }
    return ok;
}

FileState readFileRaw(const std::wstring& path, std::string& out)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return FileState::Missing;
    }
    return readFileUtf8(path, out) ? FileState::ReadOk : FileState::ReadError;
}

// Gravacao atomica: escreve .tmp, faz flush e renomeia por cima do destino.
bool writeFileAtomic(const std::wstring& path, std::string_view bytes)
{
    const std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool ok = true;
    if (!bytes.empty()) {
        const DWORD toWrite = static_cast<DWORD>(bytes.size());
        DWORD written = 0;
        ok = WriteFile(h, bytes.data(), toWrite, &written, nullptr) &&
             written == toWrite;
    }
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);

    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

// Tamanho em disco confere com o esperado (guarda contra arquivo apagado ou
// trocado por fora entre o ultimo write e este save).
bool fileSizeEquals(const std::wstring& path, size_t bytes)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart == static_cast<ULONGLONG>(bytes);
}

// Serializa com indentacao; false se o dump falhar (nao grava lixo).
bool dumpJson(const nlohmann::json& j, std::string& out)
{
    try {
        out = j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Store::Impl
// ---------------------------------------------------------------------------
struct Store::Impl {
    mutable std::mutex mutex;
    Settings settings = codec::defaultSettings();
    std::vector<AppRule> rules;
    std::vector<ArrivalRule> arrivalRules;
    std::vector<Profile> profiles;
    std::wstring lastManualDefaultDevice;
    // Ultimo conteudo gravado por arquivo (chamar com o mutex). save() pula
    // os arquivos cujo conteudo nao mudou: um toggle de setting regravava os
    // 4 arquivos (Create+flush+rename com WRITE_THROUGH) na UI thread.
    std::map<std::wstring, std::string> lastWritten;

    // Grava so se o conteudo mudou desde a ultima gravacao/leitura.
    bool writeCached(const std::wstring& path, const nlohmann::json& j)
    {
        std::string bytes;
        if (!dumpJson(j, bytes)) {
            return false;
        }
        const auto it = lastWritten.find(path);
        if (it != lastWritten.end() && it->second == bytes &&
            fileSizeEquals(path, bytes.size())) {
            return true; // disco ja tem exatamente este conteudo
        }
        if (!writeFileAtomic(path, bytes)) {
            return false;
        }
        lastWritten[path] = std::move(bytes);
        return true;
    }
};

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
Store::Store() : impl_(new Impl) {}

Store::~Store()
{
    delete impl_;
}

Store& Store::instance()
{
    // Singleton com vida de processo (nao destruido): consumidores podem
    // consulta-lo durante a destruicao de estaticos sem risco de UAF.
    static Store* s = new Store();
    return *s;
}

// ---------------------------------------------------------------------------
// Diretorios
// ---------------------------------------------------------------------------
std::wstring Store::dataDir() const
{
    const std::wstring dir = resolveDataDir();
    ensureDir(dir);
    return dir;
}

std::wstring Store::logDir() const
{
    const std::wstring dir = resolveDataDir() + L"\\logs";
    ensureDir(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// Carga: ausente => padroes + criacao; corrompido => padroes em memoria.
// Retorna false se algum arquivo existia e nao pôde ser lido/parseado.
// ---------------------------------------------------------------------------
bool Store::load()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const std::wstring dir = resolveDataDir();
    ensureDir(dir);
    ensureDir(dir + L"\\logs");

    bool ok = true;
    std::string bytes;

    // --- settings.json -------------------------------------------------
    {
        const std::wstring path = dir + kSettingsFile;
        const FileState state = readFileRaw(path, bytes);
        if (state == FileState::Missing) {
            impl_->settings = codec::defaultSettings();
            impl_->lastManualDefaultDevice.clear();
            if (!impl_->writeCached(path, codec::settingsToJson({impl_->settings, {}}))) {
                ok = false;
            }
        } else if (state == FileState::ReadError) {
            SI_LOG_WARN("store", "falha de leitura em settings.json");
            ok = false;
        } else {
            const auto j = nlohmann::json::parse(bytes, nullptr, false);
            if (j.is_discarded() || !j.is_object()) {
                SI_LOG_WARN("store", "settings.json corrompido; usando padroes");
                impl_->settings = codec::defaultSettings();
                impl_->lastManualDefaultDevice.clear();
                ok = false;
            } else {
                impl_->lastWritten[path] = bytes; // base para o dedup do save()
                const SettingsDoc doc = codec::settingsFromJson(j);
                impl_->settings = doc.settings;
                impl_->lastManualDefaultDevice = doc.lastManualDefaultDevice;
            }
        }
    }

    // --- rules.json ----------------------------------------------------
    {
        const std::wstring path = dir + kRulesFile;
        const FileState state = readFileRaw(path, bytes);
        if (state == FileState::Missing) {
            impl_->rules.clear();
            if (!impl_->writeCached(path, codec::rulesToJson({}))) {
                ok = false;
            }
        } else if (state == FileState::ReadError) {
            SI_LOG_WARN("store", "falha de leitura em rules.json");
            ok = false;
        } else {
            const auto j = nlohmann::json::parse(bytes, nullptr, false);
            if (j.is_discarded() || !(j.is_object() || j.is_array())) {
                SI_LOG_WARN("store", "rules.json corrompido; usando padroes");
                impl_->rules.clear();
                ok = false;
            } else {
                impl_->lastWritten[path] = bytes;
                impl_->rules = codec::rulesFromJson(j);
            }
        }
    }

    // --- arrival_rules.json -------------------------------------------
    {
        const std::wstring path = dir + kArrivalFile;
        const FileState state = readFileRaw(path, bytes);
        if (state == FileState::Missing) {
            impl_->arrivalRules.clear();
            if (!impl_->writeCached(path, codec::arrivalRulesToJson({}))) {
                ok = false;
            }
        } else if (state == FileState::ReadError) {
            SI_LOG_WARN("store", "falha de leitura em arrival_rules.json");
            ok = false;
        } else {
            const auto j = nlohmann::json::parse(bytes, nullptr, false);
            if (j.is_discarded() || !(j.is_object() || j.is_array())) {
                SI_LOG_WARN("store", "arrival_rules.json corrompido; usando padroes");
                impl_->arrivalRules.clear();
                ok = false;
            } else {
                impl_->lastWritten[path] = bytes;
                impl_->arrivalRules = codec::arrivalRulesFromJson(j);
            }
        }
    }

    // --- profiles.json -------------------------------------------------
    {
        const std::wstring path = dir + kProfilesFile;
        const FileState state = readFileRaw(path, bytes);
        if (state == FileState::Missing) {
            impl_->profiles.clear();
            if (!impl_->writeCached(path, codec::profilesToJson({}))) {
                ok = false;
            }
        } else if (state == FileState::ReadError) {
            SI_LOG_WARN("store", "falha de leitura em profiles.json");
            ok = false;
        } else {
            const auto j = nlohmann::json::parse(bytes, nullptr, false);
            if (j.is_discarded() || !(j.is_object() || j.is_array())) {
                SI_LOG_WARN("store", "profiles.json corrompido; usando padroes");
                impl_->profiles.clear();
                ok = false;
            } else {
                impl_->lastWritten[path] = bytes;
                impl_->profiles = codec::profilesFromJson(j);
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Gravacao atomica dos quatro arquivos. false se qualquer um falhar.
// ---------------------------------------------------------------------------
bool Store::save() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    try {
        const std::wstring dir = resolveDataDir();
        ensureDir(dir);

        const SettingsDoc doc{ impl_->settings, impl_->lastManualDefaultDevice };
        // writeCached: pula os arquivos sem mudanca (menos I/O na UI thread).
        const bool s1 = impl_->writeCached(dir + kSettingsFile, codec::settingsToJson(doc));
        const bool s2 = impl_->writeCached(dir + kRulesFile, codec::rulesToJson(impl_->rules));
        const bool s3 = impl_->writeCached(dir + kArrivalFile,
                                           codec::arrivalRulesToJson(impl_->arrivalRules));
        const bool s4 = impl_->writeCached(dir + kProfilesFile,
                                           codec::profilesToJson(impl_->profiles));
        return s1 && s2 && s3 && s4;
    } catch (...) {
        return false;  // serializacao inesperada nao pode derrubar o app
    }
}

// ---------------------------------------------------------------------------
// Settings (a referencia escapa do lock por contrato; mutacoes pontuais do
// chamador devem ser seguidas de save(), que serializa com o mutex).
// ---------------------------------------------------------------------------
Settings& Store::settings()
{
    return impl_->settings;
}

const Settings& Store::settings() const
{
    return impl_->settings;
}

// ---------------------------------------------------------------------------
// Regras por app (chave: processName em minusculas ASCII)
// ---------------------------------------------------------------------------
const std::vector<AppRule>& Store::rules() const
{
    return impl_->rules;
}

std::optional<AppRule> Store::findRule(const std::wstring& processName) const
{
    const std::wstring key = codec::normalizeProcessName(processName);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const AppRule& rule : impl_->rules) {
        if (rule.processName == key) {
            return rule;
        }
    }
    return std::nullopt;
}

void Store::upsertRule(AppRule rule)
{
    rule.processName = codec::normalizeProcessName(rule.processName);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (AppRule& existing : impl_->rules) {
        if (existing.processName == rule.processName) {
            existing = std::move(rule);
            return;
        }
    }
    impl_->rules.push_back(std::move(rule));
}

bool Store::removeRule(const std::wstring& processName)
{
    const std::wstring key = codec::normalizeProcessName(processName);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto it = impl_->rules.begin(); it != impl_->rules.end(); ++it) {
        if (it->processName == key) {
            impl_->rules.erase(it);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Regras de chegada (chave: deviceNamePattern, comparacao exata)
// ---------------------------------------------------------------------------
const std::vector<ArrivalRule>& Store::arrivalRules() const
{
    return impl_->arrivalRules;
}

void Store::upsertArrivalRule(ArrivalRule rule)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (ArrivalRule& existing : impl_->arrivalRules) {
        if (existing.deviceNamePattern == rule.deviceNamePattern) {
            existing = std::move(rule);
            return;
        }
    }
    impl_->arrivalRules.push_back(std::move(rule));
}

bool Store::removeArrivalRule(const std::wstring& deviceNamePattern)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto it = impl_->arrivalRules.begin(); it != impl_->arrivalRules.end(); ++it) {
        if (it->deviceNamePattern == deviceNamePattern) {
            impl_->arrivalRules.erase(it);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Perfis (chave: name, comparacao exata)
// ---------------------------------------------------------------------------
const std::vector<Profile>& Store::profiles() const
{
    return impl_->profiles;
}

void Store::upsertProfile(Profile profile)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (Profile& existing : impl_->profiles) {
        if (existing.name == profile.name) {
            existing = std::move(profile);
            return;
        }
    }
    impl_->profiles.push_back(std::move(profile));
}

bool Store::removeProfile(const std::wstring& name)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto it = impl_->profiles.begin(); it != impl_->profiles.end(); ++it) {
        if (it->name == name) {
            impl_->profiles.erase(it);
            return true;
        }
    }
    return false;
}

std::wstring& Store::lastManualDefaultDevice()
{
    return impl_->lastManualDefaultDevice;
}

}  // namespace soundint::core
