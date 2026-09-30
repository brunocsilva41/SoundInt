// ============================================================================
// CONTRACT — log rotativo. Wave 0 garante apenas a API; implementacao completa
// (rotacao, niveis, thread-safety) e responsabilidade do Track C (core).
// ============================================================================
#pragma once

#include <string>

namespace soundint::log {

enum class Level : uint8_t { Debug, Info, Warn, Error };

// Cria o diretorio de dados e abre o arquivo de log.
void init(const std::wstring& dir);
void shutdown();

void write(Level level, const char* channel, const std::wstring& message);
void write(Level level, const char* channel, const char* message);

// Caminho absoluto do arquivo de log atual (para anexar em bug reports).
std::wstring currentFilePath();

}  // namespace soundint::log

#define SI_LOG_DEBUG(channel, message) \
    ::soundint::log::write(::soundint::log::Level::Debug, channel, message)
#define SI_LOG_INFO(channel, message) \
    ::soundint::log::write(::soundint::log::Level::Info, channel, message)
#define SI_LOG_WARN(channel, message) \
    ::soundint::log::write(::soundint::log::Level::Warn, channel, message)
#define SI_LOG_ERROR(channel, message) \
    ::soundint::log::write(::soundint::log::Level::Error, channel, message)
