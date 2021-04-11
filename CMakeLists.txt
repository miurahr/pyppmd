cmake_minimum_required(VERSION 3.19)
project(pyppmd C)

set(CMAKE_C_STANDARD 11)

# Developer may want to specify some variable to find proper version.
# ~~~
# Priority is as same order as follows:
#   1. `Python_LOOKUP_VERSION`: specify minimum version to find.
#   2. 'Python_FIND_VIRTUALENV': specify 'ONLY' to use virtualenv activated.
#   3. `Python_ROOT`: specify installed location.
#   4. If nothing specified, use default behavior.
#
# for example
#   $ cmake -DPython_LOOKUP_VERSION=3.6 ..
#   $ cmake -DPython_FIND_VIRTUALENV=ONLY ..
#   $ cmake -DPython_ROOT=C:\Python36 ..
# ~~~
#
set(Python_LOOKUP_VERSION 3.8)

if (Python_LOOKUP_VERSION)
  set(Python_FIND_STRATEGY VERSION)
  find_package(Python ${Python_LOOKUP_VERSION} COMPONENTS Interpreter Development)
else ()
  set(Python_FIND_STRATEGY LOCATION)
  find_package(Python COMPONENTS Interpreter Development)
endif ()

include_directories(lib)

set(pyppmd_sources
             lib/Arch.h
             lib/Interface.h
             lib/Ppmd.h
             lib/Ppmd7.c
             lib/Ppmd7.h
             lib/Ppmd7Dec.c
             lib/Ppmd7Enc.c
             lib/Ppmd8.c
             lib/Ppmd8.h
             lib/Ppmd8Dec.c
             lib/Ppmd8Enc.c
             lib/PpmdBuffer.c
             lib/PpmdBuffer.h
             src/ext/_ppmdmodule.c)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py "import sysconfig\nprint(sysconfig.get_config_var('EXT_SUFFIX'))\n")
execute_process(
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
    OUTPUT_VARIABLE PY_EXT_EXT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

add_custom_command(
    OUTPUT build/lib.linux-x86_64-3.8/pyppmd/c/_ppmd.cpython-38.x86_64-linux-gnu.so
    COMMAND ${Python_EXECUTABLE} setup.py build_ext --inplace
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${pyppmd_sources}
)