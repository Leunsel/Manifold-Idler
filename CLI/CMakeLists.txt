cmake_minimum_required(VERSION 3.20)
project(ManifoldIdlerCli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
if(WIN32)
    enable_language(RC)
endif()

set(IDLER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "The Manifold-Idler folder")
set(IDLER_SOURCE "${IDLER_ROOT}/Source")
if(NOT EXISTS "${IDLER_SOURCE}/App/IdlerModel.cpp")
    message(FATAL_ERROR "Manifold Idler's sources were not found in ${IDLER_ROOT}. Set IDLER_ROOT.")
endif()

option(IDLER_WARNINGS_AS_ERRORS "Stop the build at any compiler warning with GCC or Clang" OFF)

find_package(Threads REQUIRED)

function(idler_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/utf-8;/permissive->")
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CONSOLE)
    else()
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>")
        if(IDLER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Werror>")
        endif()
    endif()
endfunction()

add_library(idler_core STATIC
    ${IDLER_SOURCE}/Core/AppInfo.cpp
    ${IDLER_SOURCE}/Core/Backups.cpp
    ${IDLER_SOURCE}/Core/Cards.cpp
    ${IDLER_SOURCE}/Core/CrashReport.cpp
    ${IDLER_SOURCE}/Core/Creatures.cpp
    ${IDLER_SOURCE}/Core/Deflate.cpp
    ${IDLER_SOURCE}/Core/Errors.cpp
    ${IDLER_SOURCE}/Core/FileLock.cpp
    ${IDLER_SOURCE}/Core/Library.cpp
    ${IDLER_SOURCE}/Core/Json.cpp
    ${IDLER_SOURCE}/Core/MiniGames.cpp
    ${IDLER_SOURCE}/Core/Queue.cpp
    ${IDLER_SOURCE}/Core/Releases.cpp
    ${IDLER_SOURCE}/Core/RuntimeLog.cpp
    ${IDLER_SOURCE}/Core/Settings.cpp
    ${IDLER_SOURCE}/Core/SpriteEdit.cpp
    ${IDLER_SOURCE}/Core/SpriteFile.cpp
    ${IDLER_SOURCE}/Core/Stats.cpp
    ${IDLER_SOURCE}/Core/Terrarium.cpp
    ${IDLER_SOURCE}/Core/Text.cpp
    ${IDLER_SOURCE}/Core/Vdf.cpp
    ${IDLER_SOURCE}/Core/WebPolicy.cpp
    ${IDLER_SOURCE}/Core/Zip.cpp)
target_include_directories(idler_core PUBLIC ${IDLER_SOURCE})
target_include_directories(idler_core SYSTEM PRIVATE ${IDLER_ROOT}/Dependencies/stb)
idler_flags(idler_core)

add_library(idler_engine STATIC
    ${IDLER_SOURCE}/App/IdlerModel.cpp
    ${IDLER_SOURCE}/Steam/LibraryScan.cpp
    ${IDLER_SOURCE}/Steam/SteamClient.cpp
    ${IDLER_SOURCE}/Steam/SteamLocal.cpp
    ${IDLER_SOURCE}/Steam/Worker.cpp
    Source/Desktop.cpp
    ${IDLER_SOURCE}/Steam/SteamWebNone.cpp)
if(WIN32)
    target_sources(idler_engine PRIVATE ${IDLER_SOURCE}/Steam/WorkerProcess.cpp ${IDLER_SOURCE}/App/CrashHandlerWin32.cpp ${IDLER_SOURCE}/App/ReleaseCheckWin32.cpp)
    target_link_libraries(idler_engine PUBLIC shell32 winhttp)
else()
    target_sources(idler_engine PRIVATE ${IDLER_SOURCE}/Steam/WorkerProcessPosix.cpp ${IDLER_SOURCE}/App/CrashHandlerPosix.cpp ${IDLER_SOURCE}/App/ReleaseCheckCurl.cpp)
    target_link_libraries(idler_engine PUBLIC ${CMAKE_DL_LIBS})
endif()
target_include_directories(idler_engine BEFORE PUBLIC Shim)
target_compile_definitions(idler_engine PUBLIC IDLER_CLI)
target_link_libraries(idler_engine PUBLIC idler_core Threads::Threads)
idler_flags(idler_engine)

add_library(idler_cli_base STATIC
    Source/BackupCommands.cpp
    Source/Http.cpp
    Source/Ipc.cpp
    Source/Paths.cpp
    Source/Request.cpp
    Source/Service.cpp
    Source/Themes.cpp
    Source/Web.cpp
    Source/WebAssets.cpp
    Source/WebPage.cpp
    Source/WebServer.cpp)
target_include_directories(idler_cli_base PUBLIC Source)
target_link_libraries(idler_cli_base PUBLIC idler_core)
if(WIN32)
    target_link_libraries(idler_cli_base PUBLIC ws2_32 bcrypt)
endif()
idler_flags(idler_cli_base)

add_executable(idler
    Source/main.cpp
    Source/Commands.cpp
    Source/PageCommands.cpp
    Source/Serve.cpp)
target_include_directories(idler BEFORE PRIVATE Shim)
target_include_directories(idler SYSTEM PRIVATE ${IDLER_ROOT}/Dependencies/CLI11)
target_link_libraries(idler PRIVATE idler_cli_base idler_engine)
if(WIN32)
    target_sources(idler PRIVATE Resources/idler.rc Resources/idler.manifest)
endif()
idler_flags(idler)

enable_testing()
add_executable(idler_tests Tests/CliTests.cpp)
target_link_libraries(idler_tests PRIVATE idler_cli_base)
idler_flags(idler_tests)
add_test(NAME idler_tests COMMAND idler_tests)

add_executable(idler_core_tests ${IDLER_ROOT}/Tests/Tests.cpp ${IDLER_SOURCE}/UI/SpriteView.cpp)
target_include_directories(idler_core_tests SYSTEM PRIVATE ${IDLER_ROOT}/Dependencies/ImGui)
target_link_libraries(idler_core_tests PRIVATE idler_core)
idler_flags(idler_core_tests)
add_test(NAME idler_core_tests COMMAND idler_core_tests)
