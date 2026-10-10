# The Lantern Keep -- Day 16. Audio: sound that has a position.
#
# The split from Day 4 stands and is what makes today possible:
#
#   keepstone    links OGRE.           Now also owns the Engine and its services.
#   lanternkeep  links keepstone ONLY. Still cannot see OGRE's headers.
#
# The game's main() gets SHORTER today, not longer. That is the test of whether an engine object
# is worth having: it should absorb the start-up sequence, not add a layer on top of it.

cmake_minimum_required(VERSION 3.13)
project(lanternkeep CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OGRE_DIR "D:/OgreSDK/CMake" CACHE PATH "the folder holding OGREConfig.cmake")
find_package(OGRE 14.6 REQUIRED CONFIG)

# ---------------------------------------------------------------- the engine ----
add_library(keepstone STATIC
    keepstone/Renderer.cpp
    keepstone/Engine.cpp
    keepstone/Loop.cpp
    keepstone/Profiler.cpp
    keepstone/World.cpp
    keepstone/TransformSystem.cpp
    keepstone/Assets.cpp
    keepstone/Prefab.cpp
    keepstone/Input.cpp
    keepstone/Collision.cpp
    keepstone/Bus.cpp
    keepstone/Audio.cpp
)

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

target_link_libraries(lanternkeep PRIVATE keepstone)

message(STATUS "OGRE ${OGRE_VERSION}")

# The game's OWN data directory. This one the game is allowed to know: a prefab is the game's
# content, not the engine's, and keepstone has no opinion about what a courtyard contains.
target_compile_definitions(lanternkeep PRIVATE
    LK_GAME_DATA="${CMAKE_CURRENT_SOURCE_DIR}/lanternkeep/data"
)
