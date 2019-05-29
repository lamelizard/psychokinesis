cmake_minimum_required(VERSION 3.0)
project(GameDevSS19)

# ==============================================================================
# global settings

# enable folders for visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# you can enable C++17 if you like
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# some dependencies want to build as shared libs
set(BUILD_SHARED_LIBS ON CACHE BOOL " " FORCE)


# ==============================================================================
# Set bin dir
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/Default)
else()
    set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR})


# ==============================================================================
# dependencies

# Add glm library
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE extern/glm)
target_compile_definitions(glm INTERFACE
    GLM_FORCE_CXX11
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_CTOR_INIT # 0.9.9 breaks a lot otherwise
)
if (GCC)
    # class-memaccess error not available before gcc 8 (new-ish warning that triggers a lot in glm)
    if(NOT ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 8)
        target_compile_options(glm INTERFACE -Wno-error=class-memaccess)
    endif()
endif()

# Add CTracer lib
add_subdirectory(extern/ctracer)

# Add GLOW lib
add_subdirectory(extern/glow)

# Add GLFW lib (with disabled spam)
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
if(MSVC)
  option(BUILD_SHARED_LIBS "" OFF)
else()
  option(BUILD_SHARED_LIBS "" ON)
endif()
add_subdirectory(extern/glfw)

# Add ImGui
add_subdirectory(extern/imgui)

# Add PolyMesh
add_subdirectory(extern/polymesh)

# Add GLOW Extras lib
add_subdirectory(extern/glow-extras)

# "some dependencies want to build as shared libs"
# -> well, not anymore I guess since the following were added by me
# needed for Bullet
set(BUILD_SHARED_LIBS OFF CACHE BOOL " " FORCE)

# Add EntityX
option(ENTITYX_BUILD_SHARED "" OFF)
add_subdirectory(extern/entityx EXCLUDE_FROM_ALL)
include_directories(extern/entityx)

# Add Bullet
set(BULLET_VERSION "2.88")
option(BUILD_BULLET3 "" OFF)
include_directories(extern/bullet/src)
add_subdirectory(extern/bullet/src)

# Add SoLoud
add_subdirectory(extern/soloud)

# Add Assimp
option(ASSIMP_BUILD_ZLIB "" ON)
option(ASSIMP_BUILD_ASSIMP_TOOLS "" OFF)
option(ASSIMP_BUILD_TESTS "" OFF)
add_subdirectory(extern/assimp EXCLUDE_FROM_ALL)
include_directories(extern/assimp/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/extern/assimp/include)

# Folder grouping
foreach(TARGET_NAME
    glfw
    ctracer
    glad
    glow
    glow-extras
    imgui
    lodepng
    stb
    polymesh
    entityx
    Bullet3Common
    BulletCollision
    BulletDynamics
    BulletInverseDynamics
    BulletSoftBody
    LinearMath
    assimp
	soloud
    IrrXML
    zlibstatic
    )
  set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "Extern") 
endforeach()


# ===========================================================================================
# Create target
file(GLOB_RECURSE SOURCES
    "src/*.cc"
    "src/*.hh"
    "data/*.*sh"
    "data/*.glsl"
)
if(${CMAKE_VERSION} VERSION_GREATER "3.8.0")
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
endif()
add_executable(${PROJECT_NAME} ${SOURCES})

# Add src/ inc dir
target_include_directories(${PROJECT_NAME} PUBLIC src)

# Link libs
target_link_libraries(${PROJECT_NAME} PUBLIC 
    ctracer
    glow
    glow-extras
    polymesh
    imgui
    entityx
    BulletDynamics
    BulletCollision
    LinearMath
    assimp
)

# Compile flags

if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC 
        /MP # multi-core compiling
    )
else()
    target_compile_options(${PROJECT_NAME} PUBLIC 
        -Wall # useful warnings
        -Wno-unused-variable # not a useful warning
    )
endif()

# Visual Studio
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE) # fixes compilation (entityx?)
endif()
