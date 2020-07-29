cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(rperf_c VERSION 0.1 LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
find_package (Threads)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

find_path(IBVERBS_INCLUDE_DIRS
  NAMES infiniband/verbs.h
  HINTS
  ${IBVERBS_INCLUDE_DIR}
  ${IBVERBS_ROOT_DIR}
  ${IBVERBS_ROOT_DIR}/include)

find_library(IBVERBS_LIBRARIES
  NAMES ibverbs
  HINTS
  ${IBVERBS_LIB_DIR}
  ${IBVERBS_ROOT_DIR}
  ${IBVERBS_ROOT_DIR}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ibverbs DEFAULT_MSG IBVERBS_INCLUDE_DIRS IBVERBS_LIBRARIES)
mark_as_advanced(IBVERBS_INCLUDE_DIR IBVERBS_LIBRARIES)


include_directories(include)
file(GLOB SOURCES "src/*.c")
add_executable(rperf_c ${SOURCES})

target_link_libraries (rperf_c ${CMAKE_THREAD_LIBS_INIT} ${IBVERBS_LIBRARIES} ${CURSES_LIBRARIES})
