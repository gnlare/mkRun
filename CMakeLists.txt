cmake_minimum_required(VERSION 3.15)
project(mkRun LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mkRun WIN32
    src/main.cpp
    src/Util.cpp
    src/Hotkey.cpp
    src/Config.cpp
    src/Launcher.cpp
    src/Settings.cpp
    src/mkRun.rc
    src/Common.h src/Util.h src/Hotkey.h src/Config.h src/Json.h src/Launcher.h src/Settings.h)

target_compile_definitions(mkRun PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(mkRun PRIVATE /utf-8 /W3)
else()
    # MinGW-w64: wWinMain 사용을 위해 -municode, DLL 의존 없이 단독 실행되도록 정적 링크
    target_link_options(mkRun PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(mkRun PRIVATE comctl32 shell32 gdi32 user32 advapi32)
