cmake_minimum_required(VERSION 3.10)

project(app C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(SOURCES
    test.c
    ${CMAKE_SOURCE_DIR}/../../../source/app/app.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/utl_printf.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/common/port_stdout.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/common/main.c
)

if(WIN32)

elseif(APPLE)
    list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/../../../source/port/mac/port_cpu.c)
elseif(UNIX)
    list(APPEND SOURCES ${CMAKE_SOURCE_DIR}/../../../source/port/unix/port_cpu.c)
endif()

add_executable(app ${SOURCES})
target_link_libraries(app PRIVATE Threads::Threads)

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
    ${CMAKE_SOURCE_DIR}/../../../source/app/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/
    ${CMAKE_SOURCE_DIR}/../../../source/hal/
)