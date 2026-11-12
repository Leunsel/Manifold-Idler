cmake_minimum_required(VERSION 3.20)
project(ManifoldIdlerLinux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(IDLER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "The Manifold-Idler folder")
set(SOURCE "${IDLER_ROOT}/Source")
if(NOT EXISTS "${SOURCE}/App/SdlApplication.cpp")
    message(FATAL_ERROR "Manifold Idler's sources were not found in ${IDLER_ROOT}. Set IDLER_ROOT.")
endif()

find_package(Threads REQUIRED)
find_package(SDL3 CONFIG QUIET)
if(TARGET SDL3::SDL3)
    set(IDLER_SDL SDL3::SDL3)
else()
    find_library(IDLER_SDL_LIBRARY NAMES SDL3 libSDL3.so.0 REQUIRED)
    add_library(idler_sdl INTERFACE)
    target_include_directories(idler_sdl SYSTEM INTERFACE "${IDLER_ROOT}/SDL3/include")
    target_link_libraries(idler_sdl INTERFACE "${IDLER_SDL_LIBRARY}")
    set(IDLER_SDL idler_sdl)
endif()

set(FONT_DATA "${CMAKE_CURRENT_BINARY_DIR}/generated/FontData.cpp")
add_custom_command(
    OUTPUT "${FONT_DATA}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${CMAKE_COMMAND}" "-DOUT=${FONT_DATA}" "-DTEXT=${IDLER_ROOT}/Resources/Fonts/NotoSans-Regular.ttf"
        "-DBOLD=${IDLER_ROOT}/Resources/Fonts/NotoSans-Bold.ttf" "-DICONS=${IDLER_ROOT}/Resources/Fonts/fa-solid-900.ttf"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFonts.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedFonts.cmake" "${IDLER_ROOT}/Resources/Fonts/NotoSans-Regular.ttf"
        "${IDLER_ROOT}/Resources/Fonts/NotoSans-Bold.ttf" "${IDLER_ROOT}/Resources/Fonts/fa-solid-900.ttf"
    VERBATIM)

add_library(idler_imgui STATIC
    "${IDLER_ROOT}/ImGui/imgui.cpp"
    "${IDLER_ROOT}/ImGui/imgui_draw.cpp"
    "${IDLER_ROOT}/ImGui/imgui_tables.cpp"
    "${IDLER_ROOT}/ImGui/imgui_widgets.cpp"
    "${IDLER_ROOT}/ImGui/backends/imgui_impl_sdl3.cpp"
    "${IDLER_ROOT}/ImGui/backends/imgui_impl_opengl3.cpp")
target_include_directories(idler_imgui PUBLIC "${IDLER_ROOT}/ImGui" "${IDLER_ROOT}/ImGui/backends")
target_link_libraries(idler_imgui PUBLIC ${IDLER_SDL})
if(NOT WIN32)
    target_link_libraries(idler_imgui PUBLIC ${CMAKE_DL_LIBS})
endif()

set(CORE AppInfo Cards Creatures Errors Library MiniGames Queue RuntimeLog Settings Stats Terrarium Text Vdf WebPolicy)
set(STEAM LibraryScan SteamClient SteamLocal Worker)
set(UI Covers CoversSdl Fonts IdleAnimation MenuWidgets MiniGameView ScreenMain TerrariumView
    Scenes/HomeAutumn Scenes/HomeSpring Scenes/HomeSummer Scenes/HomeWinter Scenes/StageClouds Scenes/StageMeadow Scenes/StageUnderwater)
set(APP IdlerModel CardService WebPanelNone SdlApplication Gl)

set(SOURCES "${SOURCE}/MainSdl.cpp" "${FONT_DATA}")
foreach(name IN LISTS CORE)
    list(APPEND SOURCES "${SOURCE}/Core/${name}.cpp")
endforeach()
foreach(name IN LISTS STEAM)
    list(APPEND SOURCES "${SOURCE}/Steam/${name}.cpp")
endforeach()
foreach(name IN LISTS UI)
    list(APPEND SOURCES "${SOURCE}/UI/${name}.cpp")
endforeach()
foreach(name IN LISTS APP)
    list(APPEND SOURCES "${SOURCE}/App/${name}.cpp")
endforeach()

if(WIN32)
    list(APPEND SOURCES "${SOURCE}/App/Desktop.cpp" "${SOURCE}/Steam/WorkerProcess.cpp" "${SOURCE}/Steam/SteamWeb.cpp")
else()
    list(APPEND SOURCES "${SOURCE}/App/DesktopLinux.cpp" "${SOURCE}/Steam/WorkerProcessPosix.cpp" "${IDLER_ROOT}/CLI/src/SteamWebNone.cpp")
endif()

add_executable(manifold-idler ${SOURCES})
target_include_directories(manifold-idler PRIVATE "${SOURCE}")
target_include_directories(manifold-idler SYSTEM PRIVATE "${IDLER_ROOT}/stb")
target_compile_definitions(manifold-idler PRIVATE IDLER_BUNDLED_FONTS)
target_link_libraries(manifold-idler PRIVATE idler_imgui ${IDLER_SDL} Threads::Threads)
if(MSVC)
    target_compile_options(manifold-idler PRIVATE /W4 /WX /utf-8 /permissive-)
    target_compile_definitions(manifold-idler PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(manifold-idler PRIVATE shell32 ole32 advapi32 user32 windowscodecs winhttp crypt32)
    target_compile_options(idler_imgui PRIVATE /utf-8)
else()
    target_compile_options(manifold-idler PRIVATE -Wall -Wextra)
    target_link_libraries(manifold-idler PRIVATE ${CMAKE_DL_LIBS})
endif()
