cmake_minimum_required(VERSION 3.0)
project(glow-extras)

#
# Options
#

set(GLOW_EXTRAS_EMBED_SHADERS ON CACHE BOOL "If true, shaders are embedded into the application")

#
# Utility
#

function(embed_files output fileDir mapName files)
    set(embedDir "${CMAKE_CURRENT_LIST_DIR}/${fileDir}")
    set(outputCC "${output}.cc")
    set(outputHH "${output}.hh")

    set(existingFilesObsolete TRUE)
    set(existingFilesPresent FALSE)
    set(existingFilePath "")

    if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/${outputCC})
        set(existingFilesObsolete FALSE)
        set(existingFilesPresent TRUE)
        set(existingFilePath ${CMAKE_CURRENT_LIST_DIR}/${outputCC})
    endif()

    set(resourceCount 0)
    set(registrationCode "")
    foreach(bin ${files})

        if (${existingFilesPresent} AND NOT ${existingFilesObsolete})
            if (${bin} IS_NEWER_THAN ${existingFilePath})
                set(existingFilesObsolete TRUE)
            endif()
        endif()


        MATH(EXPR resourceCount "${resourceCount}+1")

        string(REPLACE "${embedDir}" "" relativeBinPath "${bin}")
        file(READ ${bin} filedataString)
        set(registrationCode "${registrationCode}{\"${relativeBinPath}\", R\"%%RES_EMBED%%(\n${filedataString}\n)%%RES_EMBED%%\"},\n")
    endforeach()

    if(${resourceCount} AND ${existingFilesObsolete})
        file(WRITE ${outputHH} "#pragma once\n\n#include <utility>\n\n")
        file(APPEND ${outputHH} "// This file is generated upon running CMake, do not modify it!\n\n")
        file(APPEND ${outputHH} "namespace internal_embedded_files {\n")
        file(APPEND ${outputHH} "extern const std::pair<const char*, const char*> ${mapName}[${resourceCount}];\n}\n")

        file(WRITE ${outputCC} "#include \"${outputHH}\"\n\n")
        file(APPEND ${outputCC} "// This file is generated upon running CMake, do not modify it!\n\n")
        file(APPEND ${outputCC} "namespace internal_embedded_files {\n")
        file(APPEND ${outputCC} "\nconst std::pair<const char*, const char*> ${mapName}[] = {\n${registrationCode}\n};\n}\n")
    endif()
endfunction()

# ===================================================================================================
# Initialize glow-extras library

# all glow-extras
add_library(glow-extras ${GLOW_LINK_TYPE})

target_link_libraries(glow-extras PUBLIC glow)

if (MSVC)
    target_compile_options(glow-extras PUBLIC /MP /FC)
else()
    target_compile_options(glow-extras PRIVATE -Wall)
endif()

if (GLOW_EXTRAS_EMBED_SHADERS)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_EMBED_SHADERS)
endif()

# ===================================================================================================
# Check available targets

if (TARGET nanogui)
    target_link_libraries(glow-extras PUBLIC nanogui)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_NANOGUI)
endif()

if (TARGET AntTweakBar)
    target_link_libraries(glow-extras PUBLIC AntTweakBar)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_ANTTWEAKBAR)
endif()

if (TARGET imgui)
    target_link_libraries(glow-extras PUBLIC imgui)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_IMGUI)
endif()

if (TARGET aion)
    target_link_libraries(glow-extras PUBLIC aion)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_AION)
endif()

if (TARGET assimp)
    target_link_libraries(glow-extras PUBLIC assimp)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_ASSIMP)
endif()

if (TARGET polymesh)
    target_link_libraries(glow-extras PUBLIC polymesh)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_POLYMESH)
endif()

if (TARGET glfw)
    target_link_libraries(glow-extras PUBLIC glfw)
    target_compile_definitions(glow-extras PUBLIC GLOW_EXTRAS_HAS_GLFW)
endif()


# ===================================================================================================
# Add components

# Input
add_subdirectory(input)

# Colors
add_subdirectory(colors)

# Camera
add_subdirectory(camera)

# Shader
add_subdirectory(shader)

# Geometry
add_subdirectory(geometry)

# Debug
add_subdirectory(debugging)

# Material shading library
add_subdirectory(material)

# Timing
add_subdirectory(timing)

# Assimp
# requires 'assimp'
if (TARGET assimp)
    add_subdirectory(assimp)
else()
    message(STATUS "target 'assimp' not found, disabling glow-extras-assimp")
endif()

# GLFW App
add_subdirectory(glfw)


# ===================================================================================================
# Folders

if(${CMAKE_VERSION} VERSION_GREATER "3.8.0")
    get_target_property(GLOW_EXTRAS_SOURCES glow-extras SOURCES)
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GLOW_EXTRAS_SOURCES})
endif()
