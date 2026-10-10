# The Lantern Keep -- Day 4. THE DAY THE PROJECT BECOMES TWO PROJECTS.
#
# Days 1 to 3 built one executable. From today there is a library and a game, and the CMake file
# is where that separation is first enforced rather than merely intended:
#
#   keepstone    links OGRE.           Knows what a render system is.
#   lanternkeep  links keepstone ONLY. Does not have OGRE's include paths, so a gameplay file
#                                      that reaches for <Ogre.h> does not compile.
#
# That last line is the part worth noticing. The boundary is not a convention the authors agree
# to respect -- the game physically cannot see OGRE's headers, because nothing gives them to it.
# tools/check_book.py greps for `Ogre::` under lanternkeep/ as a second line of defence, but the
# build is the first one.

cmake_minimum_required(VERSION 3.13)
project(lanternkeep CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OGRE_DIR "D:/OgreSDK/CMake" CACHE PATH "the folder holding OGREConfig.cmake")
find_package(OGRE 14.6 REQUIRED CONFIG)

# ---------------------------------------------------------------- the engine ----
add_library(keepstone STATIC
    keepstone/Renderer.cpp
)

# PUBLIC on the include directory: anything linking keepstone can write
# #include "keepstone/Renderer.h". PRIVATE on OGRE: the OGRE targets carry their own include
# paths, and PRIVATE stops those paths being handed on to whoever links this library.
target_include_directories(keepstone PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(keepstone PRIVATE OgreMain OgreRTShaderSystem)

target_compile_definitions(keepstone PRIVATE
    LK_OGRE_MEDIA="${OGRE_MEDIA_DIR}"
    LK_OGRE_PLUGINS="${OGRE_PLUGIN_DIR}"
    LK_BOOK_MEDIA="${CMAKE_CURRENT_SOURCE_DIR}/../../media"
)

# ------------------------------------------------------------------ the game ----
add_executable(lanternkeep
    lanternkeep/main.cpp
)

# One library, and it is not OGRE.
target_link_libraries(lanternkeep PRIVATE keepstone)

# ------------------------------------------------------- the counter-example ----
# The welded version from the chapter: the same courtyard with the renderer and the game in one
# file. It is built so the book can prove the two write identical frames, and it is deliberately
# NOT under lanternkeep/, because it is the thing the boundary exists to prevent.
add_executable(welded
    welded/main.cpp
)
target_link_libraries(welded PRIVATE OgreMain OgreRTShaderSystem)
target_compile_definitions(welded PRIVATE
    LK_OGRE_MEDIA="${OGRE_MEDIA_DIR}"
    LK_OGRE_PLUGINS="${OGRE_PLUGIN_DIR}"
    LK_BOOK_MEDIA="${CMAKE_CURRENT_SOURCE_DIR}/../../media"
)

message(STATUS "OGRE ${OGRE_VERSION}")
