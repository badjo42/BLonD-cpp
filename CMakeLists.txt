# usage: cmake -DCMAKE_BUILD_TYPE=Debug/Release ..

# Requirements list
cmake_minimum_required(VERSION 2.8.4)
project(BLonD++ CXX)
include(CheckCXXCompilerFlag)
include(ExternalProject)
# include(ProcessorCount)
include(CTest)


# Code editors usability feature
set_property(GLOBAL PROPERTY USE_FOLDERS On)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING 
    "We have optemized configuration for Debug and Release configurations")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "We have optimized configuration 
    for Debug and Release configurations")

# Variables default values list
set(CMAKE_BUILD_TYPE Release CACHE STRING "default build type 
    for Make-like generators")
set(SHARED_LIBRARY_ENABLED "True")
set(EXTERNAL_INSTALL_DIR ${PROJECT_SOURCE_DIR}/external/install)
set(TEST_ENABLED "True" CACHE STRING "Cerate unit-test projects")
# set(BENCHMARK_ENABLED "True" CACHE STRING "Cerate benchmark projects")
set(ARCHITECTURE "x64" CACHE STRING "Set architecture 'x86' or 'x32'") 
# TODO: find usages and  fix comment

set(DATAFILES_DIR_DEMOS "${CMAKE_CURRENT_LIST_DIR}/demos/input_files" 
    CACHE STRING "Set demos input files path")
set(DATAFILES_DIR_TESTS "${CMAKE_CURRENT_LIST_DIR}/unit-tests/references" 
    CACHE STRING "Set unit-tests input files path")

set(USE_OMP "True" CACHE STRING
    "OpenMP is used by default to parallelize the code")

# Packages
set(EXTERNAL_INSTALL_DIR ${PROJECT_SOURCE_DIR}/external/install CACHE STRING 
    "External libraries installation folder (with subfolders lib and include)")

set(WITH_FORMAT "False" CACHE STRING 
    "Clang-Format all project files on build (requires clang-format)")

# Package settings
set(USE_FFTW_OMP "False" CACHE STRING 
    "Should use OpenMP flavour of FFTW library (linux only)") 

add_definitions(-DDEMO_FILES="${DATAFILES_DIR_DEMOS}" 
                -DTEST_FILES="${DATAFILES_DIR_TESTS}") #demos datafiles

# CPU for build count
# ProcessorCount(N)
# if (NOT N EQUAL 0)
#     set(ctest_test_args ${ctest_test_args} PARALLEL_LEVEL ${N})
# endif ()

# Project architecture
set(ARCHITECTURE "Default" CACHE STRING 
    "Two architectures avaliable 'Default' and 'x64',
     it is important for external libraries")
if ( (${CMAKE_GENERATOR} MATCHES ".*(64).*") OR 
        (${CMAKE_SIZEOF_VOID_P} MATCHES ".*(64).*"))
    set(ARCHITECTURE "x64")
    message(STATUS "architecture: ${ARCHITECTURE} detected 
        from (${CMAKE_GENERATOR} and ${CMAKE_SIZEOF_VOID_P}) ")
endif ()

#Compiler settings
check_cxx_compiler_flag(-std=c++11 COMPILER_HAS_CXX11)

if (NOT COMPILER_HAS_CXX11)
    message(FATAL_ERROR "ERROR: Compiler is lacking support for c++11")
endif ()



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall -pedantic -Wno-unused-variable")

if(CYGWIN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=large")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -INTERFACE_POSITION_INDEPENDENT_CODE")
endif ()


# set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3 -fno-omit-frame-pointer") #-fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3 -fno-omit-frame-pointer -funsafe-math-optimizations")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -march=native")
    # -fopt-info-vec=report.txt")

#CLANG and GCC specific
if (CMAKE_CXX_COMPILER_ID MATCHES GNU)

    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.0")
        message(FATAL_ERROR "ERROR: Insufficient compiler version, \
                            need at lest 4.8 to continue")
    endif ()
    # NOTE funsafe-math-optimizations can produce incorrect results in 
    # Distributions functions
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} \
    #     -fno-math-errno \
    #     -funsafe-math-optimizations \
    #     -fno-rounding-math \
    #     -ffinite-math-only \
    #     -fno-signaling-nans \
    #     -fcx-limited-range")

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}-flto")
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
    if (TEST_ENABLED STREQUAL "True")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
    else ()
        message(STATUS "Code test coveredge is enabeled only in Debug mode")
    endif ()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.8)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    else ()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
    endif ()

    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}-flto")

else ()
    message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} is not supported")
endif()


#OpenMP
find_package(OpenMP QUIET)
if ((OPENMP_FOUND) AND (USE_OMP))
    message(STATUS "OpenMP found")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_OMP -fopenmp")
else ()
    message(STATUS "OpenMP will not be used")
    set(USE_FFTW_OMP "False")
endif ()

# FFTW
find_path(FFTW_INCLUDES fftw3.h
    PATHS ${EXTERNAL_INSTALL_DIR}/include
    NO_DEFAULT_PATH)

if(FFTW_INCLUDES STREQUAL "FFTW_INCLUDES-NOTFOUND")
    message(FATAL_ERROR "ERROR: fftw3.h not found")
endif()

find_library(FFTW_LIB NAMES fftw fftw3 fftw3-3 fftw-3.3
    PATHS ${EXTERNAL_INSTALL_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED)


if (USE_FFTW_OMP)
    # message(STATUS "using omp version of fftw")
    find_library(FFTW_OMP_LIB 
        NAMES fftw_omp fftw3_omp fftw3_omp-3 fftw3-3_omp
        PATHS ${EXTERNAL_INSTALL_DIR}/lib
        NO_DEFAULT_PATH
        REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_FFTW_OMP")
    set(FFTW_LIB ${FFTW_LIB} ${FFTW_OMP_LIB})
endif ()

# GTEST
if (TEST_ENABLED)
    find_path(GTEST_INCLUDES gtest.h
        PATHS "${EXTERNAL_INSTALL_DIR}/include/gtest"
        NO_DEFAULT_PATH)
    
    if(GTEST_INCLUDES STREQUAL "GTEST_INCLUDES-NOTFOUND")
        message(FATAL_ERROR "ERROR: gtest.h not found")
    endif()
    
    find_library(GTEST_LIB NAMES gtest libgtest
        PATHS "${EXTERNAL_INSTALL_DIR}/lib"
        NO_DEFAULT_PATH
        REQUIRED)
    find_library(GTEST_MAIN_LIB NAMES gtest_main libgtest_main
        PATHS "${EXTERNAL_INSTALL_DIR}/lib"
        NO_DEFAULT_PATH
        REQUIRED)
endif ()


include_directories(
    ${PROJECT_SOURCE_DIR}/include/ 
    ${PROJECT_SOURCE_DIR}/unit-tests/test-utilities/
    )

include_directories(SYSTEM
    ${EXTERNAL_INSTALL_DIR}/include/
    ${EXTERNAL_INSTALL_DIR}/include/python2.7
    ${EXTERNAL_INSTALL_DIR}/lib/python2.7/site-packages
    )

link_directories(
    ${EXTERNAL_INSTALL_DIR}/lib/python2.7/config
    ${EXTERNAL_INSTALL_DIR}/lib
   )


# main library
list(APPEND PYTHON_LIB python2.7)
list(APPEND HDF5_LIB hdf5 hdf5_cpp hdf5_hl hdf5_hl_cpp)
list(APPEND REST_LIBS util dl)

list(APPEND LIBRARIES
        ${FFTW_LIB}
        ${PYTHON_LIB}
        ${HDF5_LIB}
        ${REST_LIBS}
        )


file(GLOB_RECURSE SOURCES
        "src/*"
        "include/*"
        )

set(SOURCE_FILES ${SOURCES})


if (SHARED_LIBRARY_ENABLED)
    add_library(blond SHARED ${SOURCE_FILES})
    target_compile_definitions(blond PRIVATE SHARED=1)
else ()
    add_library(blond STATIC ${SOURCE_FILES})
    target_compile_definitions(blond PRIVATE STATIC=1)
endif ()
target_link_libraries(blond ${LIBRARIES})


# demo executables
list(APPEND LIBRARIES blond)

file(GLOB EXECUTABLE_SOURCES "demos/*.cpp")
set(EXECUTABLE_FILES ${EXECUTABLE_SOURCES})


# demo dependencies
# message(STATUS "Generating demo executables")
foreach (src ${EXECUTABLE_FILES})
    file(RELATIVE_PATH testfile ${CMAKE_CURRENT_SOURCE_DIR} ${src})
    string(REPLACE ".cpp" "" testname ${testfile})
    string(REPLACE "demos/" "" testname ${testname})
    add_executable(${testname} ${src})
    set_target_properties(${testname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY demos)
    target_link_libraries(${testname} ${LIBRARIES})
endforeach ()


# testing
if (TEST_ENABLED)

    # message(STATUS "Generating test executables")
    list(APPEND LIBRARIES ${GTEST_LIB})
    list(APPEND LIBRARIES ${GTEST_MAIN_LIB})

    # if (UNIX)
    #     set(CTEST_MEMORYCHECK_COMMAND, "usr/bin/valgrind")
    #     set(CTEST_MEMORYCHECK_COMMAND_OPTIONS, "--leak-check=full")
    # endif ()

    file(GLOB_RECURSE UNIT_TEST_SOURCES "./unit-tests/*.cpp")
    set(UNIT_TEST_FILES ${UNIT_TEST_SOURCES})

    foreach (src ${UNIT_TEST_FILES})
        file(RELATIVE_PATH testfile ${CMAKE_CURRENT_SOURCE_DIR} ${src})
        string(REPLACE ".cpp" "" testname ${testfile})
        string(REGEX REPLACE "unit-tests/.+/" "" testname ${testname})
        add_executable(${testname} ${src})
        set_target_properties(${testname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY unit-tests)
        target_link_libraries(${testname} ${LIBRARIES})
        add_test(${testname} unit-tests/${testname})
    endforeach ()

    enable_testing()
endif ()


install(TARGETS blond
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        INCLUDES DESTINATION include
        CONFIGURATIONS Debug|Release
        )

include(clang-format.cmake)