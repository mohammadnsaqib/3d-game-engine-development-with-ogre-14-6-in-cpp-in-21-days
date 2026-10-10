# The Lantern Keep -- Day 1.
#
# Four lines do the work. find_package asks OGRE where it installed itself, and the OGRE::
# targets that come back carry their own include directories and their own link libraries, so
# nothing here has to name a path inside the SDK.
#
# There is no keepstone/ target yet. The engine starts on Day 4; today is one file.

cmake_minimum_required(VERSION 3.13)
project(lanternkeep CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where OGRE put its CMake config when it installed. On this machine that is
# D:/OgreSDK/CMake; set OGRE_DIR on the command line if yours is elsewhere.
set(OGRE_DIR "D:/OgreSDK/CMake" CACHE PATH "the folder holding OGREConfig.cmake")
find_package(OGRE 14.6 REQUIRED CONFIG)

add_executable(lanternkeep
    src/main.cpp
)

target_link_libraries(lanternkeep PRIVATE
    OgreMain
    OgreRTShaderSystem
    OgreBites
)

# The SDK's own folders, compiled in as defaults so the program can find its shaders and its
# plugins without a config file sitting next to the executable. A shipped build overrides both
# -- Day 21 does exactly that.
target_compile_definitions(lanternkeep PRIVATE
    LK_OGRE_MEDIA="${OGRE_MEDIA_DIR}"
    LK_OGRE_PLUGINS="${OGRE_PLUGIN_DIR}"
)

message(STATUS "OGRE ${OGRE_VERSION}")
message(STATUS "  media   ${OGRE_MEDIA_DIR}")
message(STATUS "  plugins ${OGRE_PLUGIN_DIR}")
