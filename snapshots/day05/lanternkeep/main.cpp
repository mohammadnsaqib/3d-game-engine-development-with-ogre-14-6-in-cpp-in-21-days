// The Lantern Keep -- Day 5.
//
// Still no OGRE anywhere in this file. What changed today is that main() stopped being a
// sequence of start-up calls and became a list of services plus one `engine.startUp()`.
//
// The interesting scene is `reversed`. Scenery needs the renderer to be up before it can put
// anything in the world, so registering it FIRST is a dependency stated in the wrong order --
// and the engine says so, by name, instead of crashing.

#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

const char* SCENES[] = {
    "ordered",     // renderer, then scenery: the courtyard is built and drawn
    "reversed",    // scenery first: it refuses, and the engine stops with a named failure
    "dark",        // ordered, with the scenery service choosing a dimmer courtyard
    "lit",         // ordered, brighter
    "close",       // ordered, camera moved in by the scenery service
    "sparse",      // ordered, three blocks
    "dense",       // ordered, nine blocks
    "twin",        // ordered, two lanterns
    "bare",        // ordered, no blocks: the engine still comes up and still draws
    "wide",        // ordered, the camera pulled back by the scenery service
};

// A GAME-SIDE SERVICE. The engine does not know what scenery is; it knows something implements
// Service and asked to be started. That is the whole point of the interface.
class Scenery : public keepstone::Service
{
public:
    Scenery(keepstone::Renderer& renderer, const std::string& scene)
        : mRenderer(renderer), mScene(scene) {}

    const char* name() const override { return "scenery"; }

    bool startUp() override
    {
        // THE DEPENDENCY, STATED AND CHECKED. Scenery cannot build anything through a
        // renderer that has not come up, so it asks rather than assuming. A service
        // that assumes gets a crash here instead of a sentence.
        if (!mRenderer.isUp())
        {
            std::cout << "scenery: the renderer is not up yet, so there is nothing "
                         "to build in\n";
            return false;
        }

        mRenderer.setAmbient(ambient());
        mRenderer.setCamera(eye(), {0.0f, 30.0f, 0.0f});
        mRenderer.addLantern({140.0f, 180.0f, 190.0f}, {1.0f, 0.88f, 0.66f});
        if (mScene == "twin")
            mRenderer.addLantern({-180.0f, 90.0f, 140.0f}, {0.45f, 0.62f, 1.0f});

        const int count = blocks();
        for (int i = 0; i < count; ++i)
        {
            const float step = 700.0f / static_cast<float>(count + 1);
            const float x = -350.0f + step * static_cast<float>(i + 1);
            const float h = 0.55f + 0.14f * static_cast<float>(i % 3);
            mRenderer.addBlock({x, 40.0f, 0.0f}, {0.32f, h, 0.32f},
                               13.0f * static_cast<float>(i));
        }
        return true;
    }

    void shutDown() override
    {
        // Nothing to release yet: the renderer owns every node. Day 9 gives the world its own
        // lifetime and this stops being empty.
    }

private:
    keepstone::Colour ambient() const
    {
        if (mScene == "dark") return {0.05f, 0.05f, 0.07f};
        if (mScene == "lit")  return {0.70f, 0.70f, 0.74f};
        return {0.25f, 0.25f, 0.30f};
    }

    keepstone::Vec3 eye() const
    {
        if (mScene == "close") return {0.0f, 70.0f, 170.0f};
        if (mScene == "wide")  return {0.0f, 210.0f, 520.0f};
        return {0.0f, 120.0f, 300.0f};
    }

    int blocks() const
    {
        if (mScene == "bare")   return 0;
        if (mScene == "sparse") return 3;
        if (mScene == "dense")  return 9;
        return 5;
    }

    keepstone::Renderer& mRenderer;
    std::string mScene;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "ordered";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    Scenery scenery(renderer, scene);

    keepstone::Engine engine;

    // THE ONLY DIFFERENCE BETWEEN THE TWO OUTCOMES IS THESE TWO LINES' ORDER.
    if (scene == "reversed")
    {
        engine.add(&scenery);
        engine.add(&renderer);
    }
    else
    {
        engine.add(&renderer);
        engine.add(&scenery);
    }

    const bool up = engine.startUp();

    for (const std::string& line : engine.log())
        std::cout << line << "\n";

    if (!up)
    {
        std::cout << "engine did not come up; no frame written\n";
        std::cout.flush();
        std::_Exit(0);
    }

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::cout << "wrote " << out << "\n";

    engine.shutDown();
    for (const std::string& line : engine.log())
        std::cout << line << "\n";

    std::cout.flush();
    std::_Exit(0);
}
