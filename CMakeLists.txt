cmake_minimum_required(VERSION 3.19)
project(pyppmd C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

## TARGET PYTHON version
set(PY_VERSION 3.8)

############################################
## Configuration for python-ext
set(Python_FIND_STRATEGY VERSION)
set(Python_FIND_IMPLEMENTATIONS CPython)
find_package(Python ${PY_VERSION}.0...${PY_VERSION}.99 COMPONENTS Interpreter Development)
set(PY_EXT_FILE _ppmd)
set(PY_EXT_DIR src/pyppmd/c)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py "import sysconfig\nprint(sysconfig.get_config_var('EXT_SUFFIX'))\n")
execute_process(
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
        OUTPUT_VARIABLE PY_EXT_EXT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PY_EXT ${PY_EXT_DIR}/${PY_EXT_FILE}.${PY_EXT_EXT})
add_custom_target(generate_ext BYPRODUCTS ${PY_EXT}
    COMMAND ${Python_EXECUTABLE} setup.py build_ext --inplace
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${pyppmd_sources}
)

############################################
#  For pytest
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
    std::string pycode = \"import sys\\nimport pytest\\nsys.path.append('${CMAKE_BINARY_DIR}/venv/lib/python${PY_VERSION}/site-packages')\\nsys.path.append('${CMAKE_SOURCE_DIR}/src')\\npytest.main(\" + args + \")\\n\";
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
add_dependencies(pytest_runner venv.stamp generate_ext)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/requirements.txt
"
coverage[toml]>=5.2
hypothesis
psutil
pytest
pytest-benchmark
pytest-cov
")
add_custom_target(venv.stamp
        COMMAND ${Python_EXECUTABLE} -m venv ${CMAKE_BINARY_DIR}/venv
        COMMAND ${CMAKE_BINARY_DIR}/venv/bin/pip install -r ${CMAKE_BINARY_DIR}/requirements.txt)

############################################
## for build test and analytics
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
target_include_directories(pyppmd PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pyppmd PRIVATE ${Python_LIBRARIES})
############################################
