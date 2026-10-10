// Keepstone -- Renderer.h
//
// THE BOUNDARY. This header is what the game is allowed to see, and there is not one OGRE type
// in it. No Ogre::SceneNode, no Ogre::Entity, no Ogre::Vector3, and no #include <Ogre.h>.
//
// That is not tidiness. A header is a contract, and every type named in it becomes something
// the caller depends on: include Ogre.h here and every gameplay file that includes Renderer.h
// is now an OGRE file, needs OGRE's include paths to compile, and cannot be built or read
// without it. The renderer stops being a component the engine owns and becomes something the
// whole program is made of.
//
// The trick that makes it possible is the pointer to Impl at the bottom. Everything OGRE is on
// the other side of it, in Renderer.cpp, which is the only file in Keepstone that includes
// OGRE at all.

#ifndef KEEPSTONE_RENDERER_H
#define KEEPSTONE_RENDERER_H

#include "keepstone/Engine.h"

#include <cstdint>
#include <string>

namespace keepstone {

// Keepstone's own small vocabulary of shapes. Three floats is three floats -- the point is not
// that this is better than Ogre::Vector3, it is that it belongs to us, so nothing in the game
// has to know which renderer is underneath.
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Colour
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

// A number, not a pointer. The game holds this and cannot dereference it, cannot delete it, and
// cannot reach anything OGRE owns through it. Day 9 makes it survive the death of the thing it
// names; today it is a handle in the simplest sense of the word.
using ObjectId = std::uint32_t;

// A SERVICE LIKE ANY OTHER, as of Day 5. The engine starts it, stops it, and knows nothing
// else about it -- `Engine` never names a renderer anywhere in its source.
class Renderer : public Service
{
public:
    Renderer();
    ~Renderer() override;

    const char* name() const override { return "renderer"; }
    bool startUp() override;
    void shutDown() override;

    // The size is set before start-up now, because startUp() takes no arguments: that
    // is the price of a uniform Service interface, and it is paid once, here.
    void setSize(unsigned width, unsigned height);
    bool isUp() const;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void setAmbient(Colour light);
    void setCamera(Vec3 eye, Vec3 target);

    ObjectId addBlock(Vec3 position, Vec3 scale, float yawDegrees);
    ObjectId addLantern(Vec3 position, Colour light);

    void setPosition(ObjectId id, Vec3 position);

    // Draws one frame and writes it to a PNG. The game asks for a picture; how a picture is
    // made is not the game's business.
    void renderTo(const std::string& pngPath);

private:
    struct Impl;
    Impl* mImpl;      // everything OGRE lives behind this pointer
};

}  // namespace keepstone

#endif
