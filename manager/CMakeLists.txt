cmake_minimum_required(VERSION 3.24)

# version = apm_version in ../versions.json; build = git commit count (+hash, date, -dirty)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../versions.json" APM_VERSIONS_JSON)
string(JSON APM_VERSION GET "${APM_VERSIONS_JSON}" apm_version)
project(apm_manager VERSION ${APM_VERSION} LANGUAGES CXX)

find_package(Git QUIET)
set(APM_BUILD 0)
set(APM_COMMIT "nogit")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE _count OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND _count)
        set(APM_BUILD ${_count})
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE _hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND _hash)
        set(APM_COMMIT ${_hash})
        execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain --untracked-files=no WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
                        OUTPUT_VARIABLE _dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_dirty)
            string(APPEND APM_COMMIT "-dirty")
        endif()
    endif()
endif()
string(TIMESTAMP APM_BUILD_DATE "%Y-%m-%d" UTC)
message(STATUS "APM Manager ${PROJECT_VERSION} build ${APM_BUILD} (${APM_COMMIT}, ${APM_BUILD_DATE})")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network Svg)
qt_standard_project_setup()

configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)

qt_add_executable(APMManager WIN32
    src/main.cpp
    src/Win.h            src/Win.cpp
    src/Engine.h         src/Engine.cpp
    src/Theme.h          src/Theme.cpp
    src/MainWindow.h     src/MainWindow.cpp
    src/SettingsDialog.h src/SettingsDialog.cpp
    src/UpdateDialog.h   src/UpdateDialog.cpp
    src/AboutDialog.h    src/AboutDialog.cpp
    src/Elevated.h       src/Elevated.cpp
    src/Setup.h          src/Setup.cpp
    src/SetupDialog.h    src/SetupDialog.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

qt_add_resources(APMManager "ui" PREFIX "/" BASE resources FILES resources/check.svg)

target_compile_definitions(APMManager PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    APM_MANAGER_VERSION="${PROJECT_VERSION}"
    APM_BUILD="${APM_BUILD}"
    APM_COMMIT="${APM_COMMIT}"
    APM_BUILD_DATE="${APM_BUILD_DATE}")
target_compile_options(APMManager PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(APMManager PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Svg
    advapi32 user32 shell32 iphlpapi)
