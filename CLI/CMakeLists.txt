cmake_minimum_required(VERSION 3.20)
project(ManifoldIdlerCli VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(IDLER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Source" CACHE PATH "The Source folder of Manifold Idler")
if(NOT EXISTS "${IDLER_SOURCE}/App/IdlerModel.cpp")
    message(FATAL_ERROR "Manifold Idler's sources were not found in ${IDLER_SOURCE}. Set IDLER_SOURCE.")
endif()

find_package(Threads REQUIRED)

function(idler_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /permissive-)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CONSOLE)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

add_library(idler_core STATIC
    ${IDLER_SOURCE}/Core/AppInfo.cpp
    ${IDLER_SOURCE}/Core/Cards.cpp
    ${IDLER_SOURCE}/Core/Creatures.cpp
    ${IDLER_SOURCE}/Core/Errors.cpp
    ${IDLER_SOURCE}/Core/Library.cpp
    ${IDLER_SOURCE}/Core/MiniGames.cpp
    ${IDLER_SOURCE}/Core/Queue.cpp
    ${IDLER_SOURCE}/Core/RuntimeLog.cpp
    ${IDLER_SOURCE}/Core/Settings.cpp
    ${IDLER_SOURCE}/Core/Stats.cpp
    ${IDLER_SOURCE}/Core/Terrarium.cpp
    ${IDLER_SOURCE}/Core/Text.cpp
    ${IDLER_SOURCE}/Core/Vdf.cpp
    ${IDLER_SOURCE}/Core/WebPolicy.cpp)
target_include_directories(idler_core PUBLIC ${IDLER_SOURCE})
idler_flags(idler_core)

add_library(idler_engine STATIC
    ${IDLER_SOURCE}/App/IdlerModel.cpp
    ${IDLER_SOURCE}/Steam/LibraryScan.cpp
    ${IDLER_SOURCE}/Steam/SteamClient.cpp
    ${IDLER_SOURCE}/Steam/SteamLocal.cpp
    ${IDLER_SOURCE}/Steam/Worker.cpp
    src/Desktop.cpp
    src/SteamWebNone.cpp)
if(WIN32)
    target_sources(idler_engine PRIVATE ${IDLER_SOURCE}/Steam/WorkerProcess.cpp)
    target_link_libraries(idler_engine PUBLIC shell32)
else()
    target_sources(idler_engine PRIVATE ${IDLER_SOURCE}/Steam/WorkerProcessPosix.cpp)
    target_link_libraries(idler_engine PUBLIC ${CMAKE_DL_LIBS})
endif()
target_include_directories(idler_engine BEFORE PUBLIC src/shim)
target_compile_definitions(idler_engine PUBLIC IDLER_CLI)
target_link_libraries(idler_engine PUBLIC idler_core Threads::Threads)
idler_flags(idler_engine)

add_library(idler_cli_base STATIC
    src/Http.cpp
    src/Ipc.cpp
    src/Paths.cpp
    src/Request.cpp
    src/Service.cpp
    src/Web.cpp
    src/WebFonts.cpp
    src/WebPage.cpp
    src/WebServer.cpp)
target_include_directories(idler_cli_base PUBLIC src)
target_link_libraries(idler_cli_base PUBLIC idler_core)
if(WIN32)
    target_link_libraries(idler_cli_base PUBLIC ws2_32 bcrypt)
endif()
idler_flags(idler_cli_base)

add_executable(idler
    src/main.cpp
    src/Commands.cpp
    src/Serve.cpp)
target_include_directories(idler BEFORE PRIVATE src/shim)
target_include_directories(idler SYSTEM PRIVATE third_party/CLI11)
target_link_libraries(idler PRIVATE idler_cli_base idler_engine)
idler_flags(idler)

enable_testing()
add_executable(idler_tests tests/CliTests.cpp)
target_link_libraries(idler_tests PRIVATE idler_cli_base)
idler_flags(idler_tests)
add_test(NAME idler_tests COMMAND idler_tests)

add_executable(idler_core_tests ${IDLER_SOURCE}/../Tests/Tests.cpp)
target_link_libraries(idler_core_tests PRIVATE idler_core)
idler_flags(idler_core_tests)
add_test(NAME idler_core_tests COMMAND idler_core_tests)
