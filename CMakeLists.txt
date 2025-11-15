cmake_minimum_required(VERSION 3.21)
project(dare VERSION 1.1.0 LANGUAGES CXX)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version/Version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/src/version/Version.h"
        @ONLY
)

find_package(cpr CONFIG REQUIRED)
find_package(ctre CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
find_package(efsw CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

if (WIN32)
    find_package(7zip CONFIG REQUIRED)
endif ()

add_executable(dare
        src/main.cpp

        src/auth/Authentication.cpp
        src/auth/Authentication.h

        src/data/Enrollment.cpp
        src/data/Terms.cpp
        src/data/Enrollment.h
        src/data/Links.h
        src/data/Regexes.h
        src/data/Terms.h

        src/registration/Register.cpp
        src/registration/RegistrationUtil.cpp
        src/registration/Register.h
        src/registration/RegistrationUtil.h

        src/task/ConfigLoader.cpp
        src/task/CourseManager.cpp
        src/task/SessionManager.cpp
        src/task/TaskLogger.cpp
        src/task/TaskManager.cpp
        src/task/TaskScheduler.cpp
        src/task/ConfigLoader.h
        src/task/CourseManager.h
        src/task/SessionManager.h
        src/task/Task.h
        src/task/TaskConfig.h
        src/task/TaskLogger.h
        src/task/TaskManager.h
        src/task/TaskScheduler.h

        src/util/Requests.cpp
        src/util/Utility.cpp
        src/util/Course.h
        src/util/Exceptions.h
        src/util/Requests.h
        src/util/Utility.h

        src/version/Version.cpp
)

target_include_directories(dare PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/src"
)

target_link_libraries(dare PRIVATE
        cpr::cpr
        ctre::ctre
        date::date
        date::date-tz
        efsw::efsw
        fmt::fmt
        RapidJSON
        spdlog::spdlog
        tomlplusplus::tomlplusplus
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(dare PRIVATE -flto)
        if (APPLE)
            target_link_options(dare PRIVATE -flto -Wl,-dead_strip)
        else ()
            target_link_options(dare PRIVATE -flto -Wl,--gc-sections)
        endif ()
    elseif (MSVC)
        target_compile_options(dare PRIVATE /GL)
        target_link_options(dare PRIVATE /LTCG)
    endif ()
endif ()

if (WIN32)
    target_compile_definitions(dare PRIVATE NOMINMAX)
    target_link_libraries(dare PRIVATE 7zip::7zip)
endif ()