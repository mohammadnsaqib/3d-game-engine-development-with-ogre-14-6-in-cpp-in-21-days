# The Lantern Keep -- Day 21. Cooking and shipping: a folder somebody else can run.
#
# The split from Day 4 stands and is what makes today possible:
#
#   keepstone    links OGRE.           Now also owns the Engine and its services.
#   lanternkeep  links keepstone ONLY. Still cannot see OGRE's headers.
#
# The game's main() gets SHORTER today, not longer. That is the test of whether an
# engine object is worth having: it should absorb the start-up sequence, not add a layer
# on top of it.

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
    keepstone/Behaviour.cpp
    keepstone/Save.cpp
    keepstone/Tools.cpp
)

target_include_directories(keepstone PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(keepstone PRIVATE OgreMain OgreRTShaderSystem OgreBites)

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

# The game's OWN data directory. This one the game is allowed to know: a prefab is the
# game's content, not the engine's, and keepstone has no opinion about what a courtyard
# contains.
target_compile_definitions(lanternkeep PRIVATE
    LK_GAME_DATA="${CMAKE_CURRENT_SOURCE_DIR}/lanternkeep/data"
)

# ------------------------------------------------------------- the cook config ----
# THE GAME CANNOT SEE WHERE THE SDK IS, AND THAT IS THE POINT. `LK_OGRE_MEDIA` and
# `LK_OGRE_PLUGINS` are PRIVATE compile definitions on `keepstone`, so a lanternkeep
# file that names one does not compile -- which is what happened the first time Day 21's
# packaging harness was written. The boundary drawn on Day 4 refused it, correctly.
#
# A cook tool genuinely does need those paths, so it reads them from a file written at
# configure time and placed beside the executable. That is also what a real cook script
# does: the paths belong to the build, not to the program.
file(GENERATE OUTPUT "$<TARGET_FILE_DIR:lanternkeep>/cook.cfg" CONTENT
"ogre_media=${OGRE_MEDIA_DIR}
game_media=${CMAKE_CURRENT_SOURCE_DIR}/../../media
game_data=${CMAKE_CURRENT_SOURCE_DIR}/lanternkeep/data
")
