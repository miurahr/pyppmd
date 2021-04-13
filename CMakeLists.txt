cmake_minimum_required(VERSION 3.19)
project(pyppmd C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
find_package(Python 3.8 COMPONENTS Interpreter Development)

include_directories(lib)
add_library(pyppmd
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

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp
"
#include <string>
#include <Python.h>
int main(int argc, char **argv) {
    std::string args;
    if ( argc > 1) {
        args.append(\"[\");
        for (int i = 1; i < argc; i++) {
            if (i > 2)
                args.append(\",\");
            args.append(\"\\\"\");
            args.append(argv[i]);
            args.append(\"\\\"\");
        }
        args.append(\"]\");
    }
    std::string pycode = \"import pytest\\npytest.main(\" + args + \")\\n\";
    wchar_t * program_name = Py_DecodeLocale(argv[0], NULL);
    Py_SetProgramName(program_name);
    Py_Initialize();
    PyRun_SimpleString(&*pycode.begin());
    Py_Finalize();
    return 0;
}")
add_executable(pytest_runner ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp)
target_link_libraries(pytest_runner PUBLIC pyppmd)
target_include_directories(pytest_runner PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pytest_runner PRIVATE ${Python_LIBRARIES})

target_include_directories(pyppmd PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pyppmd PRIVATE ${Python_LIBRARIES})
