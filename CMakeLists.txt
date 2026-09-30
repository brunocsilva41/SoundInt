cmake_minimum_required(VERSION 3.21)
project(SoundInt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CRT estatica: instalador sem dependencia de VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /MP /Zc:__cplusplus /Zc:preprocessor)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(SOUNDINT_BUILD_TESTS "Build dos testes unitarios" ON)

# ---------------------------------------------------------------------------
# Header de versao gerado
# ---------------------------------------------------------------------------
set(SOUNDINT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(cmake/version.h.in "${SOUNDINT_GENERATED_DIR}/soundint/version.h" @ONLY)

# ---------------------------------------------------------------------------
# Bibliotecas por modulo (GLOB CONFIGURE_DEPENDS: agents apenas adicionam
# .cpp dentro do proprio diretorio; o root CMakeLists nao precisa mudar)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOUNDINT_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/*.cpp")
file(GLOB_RECURSE SOUNDINT_AUDIO_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/audio/*.cpp")
file(GLOB_RECURSE SOUNDINT_UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/ui/*.cpp")
file(GLOB_RECURSE SOUNDINT_UPDATE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/update/*.cpp")

function(soundint_module name sources)
    add_library(${name} STATIC ${sources})
    target_include_directories(${name} PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
        "${SOUNDINT_GENERATED_DIR}"
        "${CMAKE_SOURCE_DIR}/third_party")
endfunction()

soundint_module(core "${SOUNDINT_CORE_SOURCES}")
soundint_module(audio "${SOUNDINT_AUDIO_SOURCES}")
soundint_module(ui "${SOUNDINT_UI_SOURCES}")
soundint_module(update "${SOUNDINT_UPDATE_SOURCES}")

# Dependencias entre modulos e sistema.
target_link_libraries(audio PUBLIC core ole32 runtimeobject)
target_link_libraries(ui PUBLIC core d2d1 dwrite dwmapi uxtheme gdi32 windowscodecs)
target_link_libraries(update PUBLIC core winhttp bcrypt shell32 shlwapi)
target_link_libraries(core PUBLIC advapi32 shell32 shlwapi version)

# ---------------------------------------------------------------------------
# Executavel (app de tray, subsystemo WINDOWS)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOUNDINT_APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/app/*.cpp")
add_executable(SoundInt WIN32 ${SOUNDINT_APP_SOURCES})
target_include_directories(SoundInt PRIVATE "${CMAKE_SOURCE_DIR}/src" "${SOUNDINT_GENERATED_DIR}")
target_link_libraries(SoundInt PRIVATE
    core audio ui update
    comctl32 shell32 shlwapi advapi32 version ole32 runtimeobject windowscodecs)

# ---------------------------------------------------------------------------
# Testes (doctest single-header)
# ---------------------------------------------------------------------------
if(SOUNDINT_BUILD_TESTS)
    enable_testing()
    file(GLOB SOUNDINT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
    add_executable(soundint_tests ${SOUNDINT_TEST_SOURCES})
    target_include_directories(soundint_tests PRIVATE "${CMAKE_SOURCE_DIR}/third_party/doctest")
    target_link_libraries(soundint_tests PRIVATE core audio ui update)
    add_test(NAME unit COMMAND soundint_tests)
endif()
