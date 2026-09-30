// ============================================================================
// Testes do updater (Track F): parse de manifest e SHA256 (sem rede).
// ============================================================================
#include "doctest.h"

#include <windows.h>

#include <string>

#include <nlohmann/json.hpp>

#include "update/manifest.h"
#include "update/sha256.h"

using soundint::update::Manifest;
using soundint::update::parseManifest;
using soundint::update::sha256Buffer;
using soundint::update::sha256File;

namespace {

constexpr const char* kValidManifest = R"json({
    "version": "1.2.3",
    "tag": "v1.2.3",
    "url": "https://github.com/brunocsilva41/SoundInt/releases/download/v1.2.3/SoundInt-Setup.exe",
    "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    "size": 1234567,
    "notes": "Correcoes importantes",
    "prerelease": false
})json";

constexpr const char* kExpectedAbc =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
constexpr const char* kExpectedEmpty =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

std::wstring widen(const char* text)
{
    return soundint::update::utf8ToWide(text);
}

}  // namespace

TEST_CASE("update: parse manifest valido")
{
    std::wstring error;
    const Manifest manifest = parseManifest(kValidManifest, &error);

    CHECK(manifest.valid);
    CHECK(error.empty());
    CHECK(manifest.version == L"1.2.3");
    CHECK(manifest.tag == L"v1.2.3");
    CHECK(manifest.url == L"https://github.com/brunocsilva41/SoundInt/releases/download/v1.2.3/"
                           L"SoundInt-Setup.exe");
    CHECK(manifest.sha256 ==
          L"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    CHECK(manifest.sizeBytes == 1234567);
    CHECK(manifest.notes == L"Correcoes importantes");
    CHECK_FALSE(manifest.prerelease);
}

TEST_CASE("update: parse manifest aceita campos desconhecidos")
{
    const char* json = R"json({
        "version": "2.0.0", "tag": "v2.0.0", "url": "https://example.com/i.exe",
        "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "size": 1, "campo_novo": {"x": 1}
    })json";
    std::wstring error;
    const Manifest manifest = parseManifest(json, &error);
    CHECK(manifest.valid);
    CHECK(error.empty());
}

TEST_CASE("update: parse manifest invalido nao e JSON")
{
    std::wstring error;
    const Manifest manifest = parseManifest("isto nao e json {{{", &error);
    CHECK_FALSE(manifest.valid);
    CHECK_FALSE(error.empty());

    error.clear();
    const Manifest vazio = parseManifest("", &error);
    CHECK_FALSE(vazio.valid);
    CHECK_FALSE(error.empty());

    // raiz que nao e objeto
    error.clear();
    const Manifest lista = parseManifest("[1,2,3]", &error);
    CHECK_FALSE(lista.valid);
    CHECK_FALSE(error.empty());
}

TEST_CASE("update: parse manifest com campo faltando")
{
    for (const char* campo : {"version", "tag", "url", "sha256", "size"}) {
        nlohmann::json root = nlohmann::json::parse(kValidManifest);
        root.erase(campo);

        std::wstring error;
        const std::string jsonText = root.dump();
        const Manifest manifest = parseManifest(jsonText, &error);
        CHECK_FALSE(manifest.valid);
        CHECK_FALSE(error.empty());
        CHECK(error.find(widen(campo)) != std::wstring::npos);
    }
}

TEST_CASE("update: parse manifest com valores invalidos")
{
    nlohmann::json root = nlohmann::json::parse(kValidManifest);
    std::wstring error;
    std::string jsonText;
    Manifest manifest;

    // sha256 nao hexadecimal
    root["sha256"] = "zzzz";
    jsonText = root.dump();
    manifest = parseManifest(jsonText, &error);
    CHECK_FALSE(manifest.valid);
    CHECK(error.find(L"sha256") != std::wstring::npos);

    // url fora do padrao http
    root = nlohmann::json::parse(kValidManifest);
    root["url"] = "ftp://x/y.exe";
    jsonText = root.dump();
    manifest = parseManifest(jsonText, &error);
    CHECK_FALSE(manifest.valid);
    CHECK(error.find(L"url") != std::wstring::npos);

    // size zero
    root = nlohmann::json::parse(kValidManifest);
    root["size"] = 0;
    jsonText = root.dump();
    manifest = parseManifest(jsonText, &error);
    CHECK_FALSE(manifest.valid);
    CHECK(error.find(L"size") != std::wstring::npos);
}

TEST_CASE("update: sha256 do manifest e normalizado para minusculo")
{
    const char* json = R"json({
        "version": "1.0.0", "tag": "v1.0.0", "url": "https://x/y.exe",
        "sha256": "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789",
        "size": 10
    })json";
    std::wstring error;
    const Manifest manifest = parseManifest(json, &error);
    REQUIRE(manifest.valid);
    CHECK(manifest.sha256 ==
          L"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}

TEST_CASE("update: sha256 de vetores conhecidos")
{
    CHECK(sha256Buffer(nullptr, 0) == widen(kExpectedEmpty));
    CHECK(sha256Buffer("", 0) == widen(kExpectedEmpty));
    CHECK(sha256Buffer("abc", 3) == widen(kExpectedAbc));
}

TEST_CASE("update: sha256 produz hex minusculo")
{
    const std::wstring hex = sha256Buffer("abc", 3);
    REQUIRE(hex.size() == 64);

    bool semMaiusculas = true;
    bool somenteHex = true;
    for (const wchar_t ch : hex) {
        const bool decimal = (ch >= L'0' && ch <= L'9');
        const bool minusculo = (ch >= L'a' && ch <= L'f');
        const bool maiusculo = (ch >= L'A' && ch <= L'F');
        if (maiusculo) {
            semMaiusculas = false;
        }
        if (!(decimal || minusculo)) {
            somenteHex = false;
        }
    }
    CHECK(semMaiusculas);
    CHECK(somenteHex);
}

TEST_CASE("update: sha256 de arquivo em disco")
{
    wchar_t tempPath[MAX_PATH + 1] = {};
    const DWORD length = GetTempPathW(MAX_PATH + 1, tempPath);
    REQUIRE(length > 0);
    const std::wstring filePath = std::wstring(tempPath, length) + L"soundint_sha256_test.txt";

    HANDLE file = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(file != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    const BOOL ok = WriteFile(file, "abc", 3, &written, nullptr);
    CloseHandle(file);
    REQUIRE(ok);
    REQUIRE(written == 3);

    std::wstring error;
    const std::wstring hash = sha256File(filePath, &error);
    DeleteFileW(filePath.c_str());

    CHECK(error.empty());
    CHECK(hash == widen(kExpectedAbc));
}

TEST_CASE("update: sha256 de arquivo inexistente gera erro")
{
    std::wstring error;
    const std::wstring hash = sha256File(L"caminho\\que\\nao\\existe.xyz", &error);
    CHECK(hash.empty());
    CHECK_FALSE(error.empty());
}
