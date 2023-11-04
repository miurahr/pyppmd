cmake_minimum_required(VERSION 3.19)
project(pyppmd C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# TARGET PYTHON version
set(PY_VERSION 3.12)
set(Python_FIND_IMPLEMENTATIONS CPython)
#set(Python_FIND_IMPLEMENTATIONS PyPy)
set(VENV_PATH "${CMAKE_BINARY_DIR}/venv")
set(DEBUG_BUILD ON)

# ##################################################################################################
# Configuration for python-ext
set(Python_FIND_STRATEGY VERSION)
find_package(Python ${PY_VERSION}.0...${PY_VERSION}.99 COMPONENTS Interpreter Development)
set(PY_EXT_FILE _ppmd)
set(PY_CFFI_FILE _cffi_ppmd)
set(PY_EXT_DIR pyppmd/c)
set(PY_CFFI_DIR pyppmd/cffi)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
     "import sysconfig\nprint(sysconfig.get_config_var('EXT_SUFFIX'))\n")
execute_process(
  COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_ext.py
  OUTPUT_VARIABLE PY_EXT_EXT
  OUTPUT_STRIP_TRAILING_WHITESPACE)

set(PY_BUILD_LIB_DIR build/lib.${PY_PLATFORM}-${Python_VERSION_MAJOR}.${Python_VERSION_MINOR})
set(PY_EXT_INPLACE src/${PY_EXT_DIR}/${PY_EXT_FILE}.${PY_EXT_EXT})
set(PY_EXT ${PY_BUILD_LIB_DIR}/${PY_EXT_DIR}/${PY_EXT_FILE}.${PY_EXT_EXT})
set(PY_CFFI_INPLACE src/${PY_CFFI_DIR}/${PY_CFFI_FILE}.${PY_EXT_EXT})
set(PY_CFFI ${PY_BUILD_LIB_DIR}/${PY_CFFI_DIR}/${PY_CFFI_FILE}.${PY_EXT_EXT})
if (WIN32)
  if(DEBUG_BUILD)
    set(BUILD_EXT_PYTHON ${VENV_PATH}/Scripts/python_d.exe)
  else()
    set(BUILD_EXT_PYTHON ${VENV_PATH}/bin/python.exe)
   endif()
  set(BUILD_EXT_OPTION -g)
else()
    set(BUILD_EXT_PYTHON ${VENV_PATH}/bin/python)
    set(BUILD_EXT_OPTION -g)
endif()
add_custom_target(
  generate_ext
  BYPRODUCTS ${PY_EXT}
  COMMAND ${BUILD_EXT_PYTHON} setup.py build_ext ${BUILD_EXT_OPTION}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS venv.stamp
  SOURCES ${pyppmd_sources})
add_custom_target(
  generate_cffi
  BYPRODUCTS ${PY_CFFI_INPLACE}
  COMMAND ${BUILD_EXT_PYTHON} setup.py build_ext ${BUILD_EXT_OPTION} --cffi --inplace
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS venv.stamp
  SOURCES ${pyppmd_sources})

# ##################################################################################################
include_directories(src/lib/buffer src/lib/ppmd)
set(_sources src/ext/_ppmdmodule.c src/lib/buffer/Buffer.c src/lib/buffer/ThreadDecoder.c
        src/lib/ppmd/Ppmd7.c src/lib/ppmd/Ppmd7Dec.c src/lib/ppmd/Ppmd7Enc.c
        src/lib/ppmd/Ppmd8.c src/lib/ppmd/Ppmd8Dec.c src/lib/ppmd/Ppmd8Enc.c)
Python_add_library(_ppmd MODULE WITH_SOABI ${_sources})
add_custom_target(build_ext
        BYPRODUCTS ${PY_EXT_INPLACE}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:_ppmd> ${CMAKE_SOURCE_DIR}/src/${PY_EXT_DIR}
        DEPENDS _ppmd venv.stamp
        )

# ##################################################################################################
# create virtualenv
file(
        WRITE ${CMAKE_CURRENT_BINARY_DIR}/requirements.txt
        "
coverage[toml]>=5.2
hypothesis
pytest>=6.0
pytest-benchmark
pytest-cov
pytest-timeout
cffi
py-cpuinfo
tox
")
if (WIN32)
  set(PIP_COMMAND ${VENV_PATH}/Scripts/pip.exe)
else()
  set(PIP_COMMAND ${VENV_PATH}/bin/pip)
endif()
add_custom_command(OUTPUT venv.stamp
        COMMAND ${Python_EXECUTABLE} -m venv ${VENV_PATH}
        COMMAND ${PIP_COMMAND} install -r ${CMAKE_BINARY_DIR}/requirements.txt
        COMMAND ${CMAKE_COMMAND} -E touch venv.stamp)
set(SRC_PATH "${CMAKE_SOURCE_DIR}/src")
if ("${Python_INTERPRETER_ID}" STREQUAL "PyPy")
  set(VPKG_PATH_A "${VENV_PATH}/lib/pypy${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages/")
else()
  set(VPKG_PATH_A "${VENV_PATH}/lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages/")
endif()
set(VPKG_PATH_B "${VENV_PATH}/Lib/site-packages/")

# ##################################################################################################
# For pytest
file(
  WRITE ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp
  "
#include <string>
#include <filesystem>
#include <unistd.h>
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
    std::filesystem::path src_path = \"${SRC_PATH}\";
    std::filesystem::path vsite_path_a = \"${VPKG_PATH_A}\";
    std::filesystem::path vsite_path_b = \"${VPKG_PATH_B}\";
    std::string pycode =
        \"import sys\\n\"
        \"sys.path.append('\" + src_path.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_a.string() + \"')\\n\"
        \"sys.path.append('\" + vsite_path_b.string() + \"')\\n\"
        \"import pytest\\n\"
        \"pytest.main(\" + args + \")\\n\";
    execl(\"${Python_EXECUTABLE}\", \"${Python_EXECUTABLE}\", \"-c\", pycode.c_str(), (char*)0);
    return 0;
}")
add_executable(pytest_runner ${CMAKE_CURRENT_BINARY_DIR}/pytest_runner.cpp)
if ("${Python_INTERPRETER_ID}" STREQUAL "PyPy")
  add_dependencies(pytest_runner generate_cffi)
else()
  add_dependencies(pytest_runner generate_ext build_ext)
endif()
target_include_directories(pytest_runner PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pytest_runner PRIVATE ${Python_LIBRARIES})
add_custom_target(run_pytest
        COMMAND $<TARGET_FILE:pytest_runner> --verbose
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pytest_runner)

# ##################################################################################################
# for build test and analytics
include_directories(src/lib/ppmd src/lib/buffer)
add_library(
        pyppmd
        src/lib/ppmd/Arch.h
        src/lib/ppmd/Interface.h
        src/lib/ppmd/Ppmd.h
        src/lib/ppmd/Ppmd7.c
        src/lib/ppmd/Ppmd7.h
        src/lib/ppmd/Ppmd7Dec.c
        src/lib/ppmd/Ppmd7Enc.c
        src/lib/ppmd/Ppmd8.c
        src/lib/ppmd/Ppmd8.h
        src/lib/ppmd/Ppmd8Dec.c
        src/lib/ppmd/Ppmd8Enc.c
        src/lib/buffer/blockoutput.h
        src/lib/buffer/Buffer.c
        src/lib/buffer/Buffer.h
        src/lib/buffer/win_pthreads.h
        src/lib/buffer/ThreadDecoder.c
        src/lib/buffer/ThreadDecoder.h
        src/ext/_ppmdmodule.c)
target_include_directories(pyppmd PRIVATE ${Python_INCLUDE_DIRS})
target_link_libraries(pyppmd PRIVATE ${Python_LIBRARIES})
# ##################################################################################################
add_custom_target(run_tox
        COMMAND  ${CMAKE_COMMAND} -E env VIRTUAL_ENV=${VENV_PATH} ${VENV_PATH}/bin/python -m tox
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS venv.stamp
        )
