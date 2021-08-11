cmake_minimum_required(VERSION 3.19)
project(pyppmd C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# TARGET PYTHON version
set(PY_VERSION 3.8)
set(Python_FIND_IMPLEMENTATIONS CPython)
#set(Python_FIND_IMPLEMENTATIONS PyPy)

# ##################################################################################################
# Configuration for python-ext
set(Python_FIND_STRATEGY VERSION)
find_package(Python ${PY_VERSION}.0...${PY_VERSION}.99 COMPONENTS Interpreter Development)
set(PY_EXT_FILE _ppmd)
set(PY_CFFI_FILE _ppmd)
set(PY_EXT_DIR src/pyppmd/c)
set(PY_CFFI_DIR src/pyppmd/cffi)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
     "import sysconfig\nprint(sysconfig.get_config_var('EXT_SUFFIX'))\n")
execute_process(
  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
  OUTPUT_VARIABLE PY_EXT_EXT
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(PY_EXT ${PY_EXT_DIR}/${PY_EXT_FILE}.${PY_EXT_EXT})
set(PY_CFFI ${PY_CFFI_DIR}/PY_CFFI_FILE}.${PY_EXT_EXT})
add_custom_target(
  generate_ext
  BYPRODUCTS ${PY_EXT}
  COMMAND ${Python_EXECUTABLE} setup.py build_ext --inplace --debug
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES ${pyppmd_sources})
add_custom_target(
  generate_cffi
  BYPRODUCTS ${PY_CFFI}
  COMMAND ${Python_EXECUTABLE} setup.py build_ext --cffi --debug
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES ${pyppmd_sources})

# ##################################################################################################
# create virtualenv
set(VENV_PATH "${CMAKE_BINARY_DIR}/venv")
file(
        WRITE ${CMAKE_CURRENT_BINARY_DIR}/requirements.txt
        "
coverage[toml]>=5.2
hypothesis
psutil
pytest>=6.0
pytest-benchmark
pytest-cov
")
if (WIN32)
  set(PIP_COMMAND ${VENV_PATH}/Scripts/pip.exe)
else()
  set(PIP_COMMAND ${VENV_PATH}/bin/pip)
endif()
add_custom_target(
        venv.stamp
        COMMAND ${Python_EXECUTABLE} -m venv ${VENV_PATH}
        COMMAND ${PIP_COMMAND} install -r ${CMAKE_BINARY_DIR}/requirements.txt)
set(VPKG_PATH_A "${VENV_PATH}/lib/python${PY_VERSION}/site-packages/")
set(VPKG_PATH_B "${VENV_PATH}/Lib/site-packages/")

# ##################################################################################################
# For pytest
file(
  WRITE ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp
  "
#include <string>
#include <filesystem>
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
    std::filesystem::path venv_path = \"${VENV_PATH}\";
    std::filesystem::path vlib_path = venv_path / \"lib\";
    std::filesystem::path vsite_path_a = \"${VPKG_PATH_A}\";
    std::filesystem::path vsite_path_b = \"${VPKG_PATH_B}\";
    std::string pycode =
        \"import sys\\n\"
        \"import pytest\\n\"
        \"sys.path.append('src')\\n\"
        \"sys.path.append('\" + venv_path.string() + \"')\\n\"
        \"sys.path.append('\" + vlib_path.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_a.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_b.string() + \"')\\n\"
        \"pytest.main(\" + args + \")\\n\";
    wchar_t * program_name = Py_DecodeLocale(argv[0], NULL);
    Py_SetProgramName(program_name);
    Py_Initialize();
    PyRun_SimpleString(&*pycode.begin());
    Py_Finalize();
    return 0;
}")
file(
    WRITE ${CMAKE_CURRENT_BINARY_DIR}/pypytest_runner.cpp
     "
#include <stdlib>
#include <filesystem>
#include <PyPy.h>
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
    std::filesystem::path venv_path = \"${VENV_PATH}\";
    std::filesystem::path vlib_path = venv_path / \"lib\";
    std::filesystem::path vsite_path_a = \"${VPKG_PATH_A}\";
    std::filesystem::path vsite_path_b = \"${VPKG_PATH_B}\";
    std::string pycode =
        \"import sys\\n\"
        \"import pytest\\n\"
        \"sys.path.append('src')\\n\"
        \"sys.path.append('\" + venv_path.string() + \"')\\n\"
        \"sys.path.append('\" + vlib_path.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_a.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_b.string() + \"')\\n\"
        \"pytest.main(\" + args + \")\\n\";
    rpython_startup_code();
    pypy_setup_home(NULL, 1);
    pypy_execute_source(pycode.c_str());
    return 0;
}")
if (${Python_INTERPRETER_ID} STREQUAL "PyPy")
  add_executable(pytest_runner ${CMAKE_CURRENT_BINARY_DIR}/pypytest_runner.c)
else()
  add_executable(pytest_runner ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp)
endif()
target_link_libraries(pytest_runner PUBLIC pyppmd)
target_include_directories(pytest_runner PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pytest_runner PRIVATE ${Python_LIBRARIES})
add_dependencies(pytest_runner venv.stamp generate_ext)

# ##################################################################################################
# for build test and analytics
include_directories(lib lib2)
add_library(
        pyppmd
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
        lib2/Buffer.c
        lib2/Buffer.h
        lib2/threading.c
        lib2/threading.h
        lib2/Ppmd8Tdecoder.c
        lib2/Ppmd8Tdecoder.h
        src/ext/_ppmdmodule.c)
target_include_directories(pyppmd PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pyppmd PRIVATE ${Python_LIBRARIES})
# ##################################################################################################
