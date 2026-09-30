cmake_minimum_required(VERSION 3.20)

project(BCC203_TP
    VERSION 1.0.0
    DESCRIPTION "BCC203 Trabalho Pratico - Metodos de Pesquisa e Gerador de Dados"
    LANGUAGES CXX
)

# ==============================================================================
# Global Configuration
# ==============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clangd/LSP and clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place all executables in the top-level build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Compiler warnings and options
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type" FORCE)
endif()

# Symlink compile_commands.json to source root if needed
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json)
    file(CREATE_LINK
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
        SYMBOLIC
    )
endif()

# ==============================================================================
# Target: Search Implementation (pesquisa)
# ==============================================================================
set(SEARCH_SOURCES
    src/main.cpp
    src/Common.cpp
    src/File.cpp
    src/IndexedSequentialAccess/Cache.cpp
    src/IndexedSequentialAccess/ISA.cpp
    src/BinaryTree/BTreeFile.cpp
)

add_executable(pesquisa ${SEARCH_SOURCES})
target_include_directories(pesquisa PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==============================================================================
# Target: Data Generator (generator)
# ==============================================================================
add_executable(generator
    src/generator.cpp
    src/Common.cpp
)
target_include_directories(generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==============================================================================
# Custom Target: generate
# ==============================================================================
add_custom_target(generate
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/tmp
    COMMAND $<TARGET_FILE:generator> -o ${CMAKE_CURRENT_SOURCE_DIR}/tmp
    DEPENDS generator
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating test datasets into tmp/ directory"
)

# ==============================================================================
# Clang-Format Targets (format, format-check)
# ==============================================================================
find_program(CLANG_FORMAT_BIN NAMES clang-format)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE FORMAT_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting C++ source and header files using clang-format"
    )

    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking formatting of C++ source and header files"
    )
endif()

# ==============================================================================
# Clang-Tidy Targets (tidy, lint)
# ==============================================================================
find_program(CLANG_TIDY_BIN NAMES clang-tidy)
if(CLANG_TIDY_BIN)
    file(GLOB_RECURSE TIDY_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )

    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} ${TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy on C++ source files"
    )

    add_custom_target(lint
        DEPENDS tidy
        COMMENT "Running linter (alias for tidy)"
    )
endif()
