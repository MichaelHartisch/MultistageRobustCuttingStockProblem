cmake_minimum_required(VERSION 3.16)

project(BinPackingExperiments
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TOOLS "Build the non-CPLEX instance/model tools" ON)
option(BUILD_DECISION_RULE "Build the CPLEX-based decision-rule executable" OFF)

# Optional path to IBM ILOG CPLEX Optimization Studio. This can be either:
#   /path/to/CPLEX_StudioXXXX
# or the nested cplex directory:
#   /path/to/CPLEX_StudioXXXX/cplex
set(CPLEX_ROOT "" CACHE PATH "Path to CPLEX Studio root or cplex subdirectory")

set(PROJECT_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include/InstanceWriter
    ${PROJECT_SOURCE_DIR}/include/DataStructures
    ${PROJECT_SOURCE_DIR}/include/ScenarioTree
)

set(SCENARIO_TREE_SOURCES
    src/ScenarioTree/Node.cpp
    src/ScenarioTree/ScenarioTree.cpp
)

set(INSTANCE_WRITER_SOURCES
    src/InstanceWriter/HelperFunctions.cpp
    src/InstanceWriter/InstanceWriter.cpp
    src/InstanceWriter/InstanceWriter_Heuristic2.cpp
    src/InstanceWriter/InstanceWriter_MIP.cpp
    src/InstanceWriter/InstanceWriter_QIP.cpp
    src/InstanceWriter/InstanceWriter_RC.cpp
    src/InstanceWriter/InstanceWriter_RO.cpp
    src/InstanceWriter/InstanceWriter_RO_RC.cpp
)

add_library(instance_writer_core STATIC
    ${SCENARIO_TREE_SOURCES}
    ${INSTANCE_WRITER_SOURCES}
)

target_include_directories(instance_writer_core
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(instance_writer_core
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
)

if(BUILD_TOOLS)
    add_executable(GenerateInstances GenerateInstances_main.cpp)
    target_link_libraries(GenerateInstances PRIVATE instance_writer_core)

    add_executable(GenerateModelFiles GenerateModelFiles_main.cpp)
    target_link_libraries(GenerateModelFiles PRIVATE instance_writer_core)
endif()

function(configure_cplex target_name)
    if(NOT CPLEX_ROOT)
        message(FATAL_ERROR "BUILD_DECISION_RULE=ON requires -DCPLEX_ROOT=/path/to/CPLEX_StudioXXXX or /path/to/.../cplex")
    endif()

    if(EXISTS "${CPLEX_ROOT}/cplex/include/ilcplex/cplexx.h")
        set(CPLEX_STUDIO_DIR "${CPLEX_ROOT}")
        set(CPLEX_DIR "${CPLEX_ROOT}/cplex")
        set(CONCERT_DIR "${CPLEX_ROOT}/concert")
    elseif(EXISTS "${CPLEX_ROOT}/include/ilcplex/cplexx.h")
        set(CPLEX_DIR "${CPLEX_ROOT}")
        get_filename_component(CPLEX_STUDIO_DIR "${CPLEX_ROOT}" DIRECTORY)
        set(CONCERT_DIR "${CPLEX_STUDIO_DIR}/concert")
    else()
        message(FATAL_ERROR "Could not find ilcplex/cplexx.h under CPLEX_ROOT='${CPLEX_ROOT}'")
    endif()

    # CPLEX names its library directories by OS/architecture.  Do not hardcode
    # x86-64_osx: Apple Silicon installations use arm64_osx.  CPLEX_ARCH can be
    # set manually when IBM changes/adds a directory name.
    set(CPLEX_ARCH "" CACHE STRING "CPLEX library architecture directory, e.g. arm64_osx, x86-64_osx, x86-64_linux")

    if(CPLEX_ARCH)
        set(CPLEX_SYSTEM_CANDIDATES "${CPLEX_ARCH}")
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
            set(CPLEX_SYSTEM_CANDIDATES arm64_osx x86-64_osx)
        else()
            set(CPLEX_SYSTEM_CANDIDATES x86-64_osx arm64_osx)
        endif()
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(CPLEX_SYSTEM_CANDIDATES arm64_linux x86-64_linux)
        else()
            set(CPLEX_SYSTEM_CANDIDATES x86-64_linux arm64_linux)
        endif()
    else()
        message(FATAL_ERROR "Unsupported platform for this CPLEX setup")
    endif()

    set(CPLEX_LIB_DIR "")
    set(CONCERT_LIB_DIR "")
    foreach(candidate IN LISTS CPLEX_SYSTEM_CANDIDATES)
        set(candidate_cplex_dir "${CPLEX_DIR}/lib/${candidate}/static_pic")
        set(candidate_concert_dir "${CONCERT_DIR}/lib/${candidate}/static_pic")
        if(EXISTS "${candidate_cplex_dir}" AND EXISTS "${candidate_concert_dir}")
            set(CPLEX_SYSTEM "${candidate}")
            set(CPLEX_LIB_DIR "${candidate_cplex_dir}")
            set(CONCERT_LIB_DIR "${candidate_concert_dir}")
            break()
        endif()
    endforeach()

    if(NOT CPLEX_LIB_DIR OR NOT CONCERT_LIB_DIR)
        message(FATAL_ERROR
            "Could not find matching CPLEX library directories.\n"
            "Tried: ${CPLEX_SYSTEM_CANDIDATES}\n"
            "CPLEX_DIR='${CPLEX_DIR}'\n"
            "CONCERT_DIR='${CONCERT_DIR}'\n"
            "Set -DCPLEX_ARCH=<directory-name> if your installation uses a different folder name."
        )
    endif()

    message(STATUS "Using CPLEX library architecture: ${CPLEX_SYSTEM}")
    message(STATUS "Using CPLEX library directory: ${CPLEX_LIB_DIR}")
    message(STATUS "Using Concert library directory: ${CONCERT_LIB_DIR}")

    find_library(CPLEX_LIBRARY NAMES cplex PATHS "${CPLEX_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(ILOCPLEX_LIBRARY NAMES ilocplex PATHS "${CPLEX_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
    find_library(CONCERT_LIBRARY NAMES concert PATHS "${CONCERT_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)

    target_compile_definitions(${target_name} PRIVATE IL_STD)
    target_include_directories(${target_name} PRIVATE
        ${PROJECT_INCLUDE_DIRS}
        "${CPLEX_DIR}/include"
        "${CONCERT_DIR}/include"
    )

    # Link with absolute library paths found above; this is more robust than
    # relying on linker search-directory side effects.
    target_link_libraries(${target_name} PRIVATE
        "${CONCERT_LIBRARY}"
        "${ILOCPLEX_LIBRARY}"
        "${CPLEX_LIBRARY}"
    )

    if(APPLE)
        target_link_libraries(${target_name} PRIVATE
            pthread
            m
            "-framework CoreFoundation"
            "-framework IOKit"
        )
    elseif(UNIX)
        target_link_libraries(${target_name} PRIVATE pthread m dl)
    endif()
endfunction()

if(BUILD_DECISION_RULE)
    add_executable(RunDecisionRule
        RunDecisionRule_main.cpp
        src/CplexSolver/CplexSolver.cpp
        src/InstanceWriter/InstanceWriter_DR_basic.cpp
        src/InstanceWriter/InstanceWriter_DRPrep.cpp
        src/InstanceWriter/InstanceWriter_DR_Mod12a.cpp
        src/InstanceWriter/InstanceWriter_DR_Mod12b.cpp
    )
    target_link_libraries(RunDecisionRule PRIVATE instance_writer_core)
    target_include_directories(RunDecisionRule PRIVATE
        ${PROJECT_SOURCE_DIR}/include/CplexSolver
    )
    configure_cplex(RunDecisionRule)
endif()
