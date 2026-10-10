// The Lantern Keep -- Day 4.
//
// THE WHOLE GAME, AND THERE IS NO OGRE IN IT. Not an include, not a namespace, not a type. Run
// `grep -r "Ogre::" lanternkeep/` and it returns nothing; tools/check_book.py runs exactly that
// on every build from today onwards.
//
// The scene below is the same courtyard the welded program next door builds with direct OGRE
// calls, and the two write byte-identical PNGs. That is the claim today makes: the boundary
// costs nothing you can see.

#include "keepstone/Renderer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// EVERY SCENE IS A CHANGE TO THIS FILE AND ONLY THIS FILE. Keepstone is not recompiled for any
// of them, and Renderer.h does not gain a line. That is the day's claim made runnable: if the
// interface is the right one, the game can say everything it needs to say through it.
const char* SCENES[] = {
    "keep",            // the courtyard: five blocks, one lantern
    "keep_moved",      // the same courtyard, one block moved by setPosition
    "keep_dark",       // setAmbient turned down
    "keep_bright",     // setAmbient turned up
    "keep_close",      // setCamera moved in
    "keep_two_lights", // a second addLantern, from the other side
    "keep_tall",       // the same loop, a different scale argument
    "keep_crowded",    // the same loop, eleven blocks instead of five
    "keep_bare",       // no addBlock at all: what the engine draws with nothing in it
};

// The courtyard, written once, in the game's own vocabulary. Read it without knowing which
// renderer is underneath -- that is the test the book asks the reader to apply on Day 21, and
// it is already true on Day 4.
void buildKeep(keepstone::Renderer& renderer, const std::string& scene)
{
    if (scene == "keep_dark")        renderer.setAmbient({0.04f, 0.04f, 0.06f});
    else if (scene == "keep_bright") renderer.setAmbient({0.72f, 0.72f, 0.76f});
    else                             renderer.setAmbient({0.25f, 0.25f, 0.30f});

    if (scene == "keep_close")
        renderer.setCamera({0.0f, 70.0f, 170.0f}, {0.0f, 35.0f, 0.0f});
    else
        renderer.setCamera({0.0f, 120.0f, 300.0f}, {0.0f, 30.0f, 0.0f});

    renderer.addLantern({140.0f, 180.0f, 190.0f}, {1.0f, 0.88f, 0.66f});
    if (scene == "keep_two_lights")
        renderer.addLantern({-180.0f, 90.0f, 140.0f}, {0.45f, 0.62f, 1.0f});

    if (scene == "keep_bare")
        return;

    // The keep wall. The loop is the same in every scene; only its arguments move, which is
    // what an interface is for.
    const int count = (scene == "keep_crowded") ? 11 : 5;
    const float step = (scene == "keep_crowded") ? 32.0f : 70.0f;
    const float tall = (scene == "keep_tall") ? 1.9f : 1.0f;

    keepstone::ObjectId firstBlock = 0;
    for (int i = 0; i < count; ++i)
    {
        const float x = -(step * (count - 1) * 0.5f) + step * static_cast<float>(i);
        const float h = (0.55f + 0.14f * static_cast<float>(i % 3)) * tall;
        const keepstone::ObjectId id =
            renderer.addBlock({x, 40.0f * tall, 0.0f}, {0.32f, h, 0.32f},
                              13.0f * static_cast<float>(i));
        if (i == 0) firstBlock = id;
    }

    // The ObjectId the engine handed back, used the only way the game can use it: by naming it
    // in another call. It is a number, so there is nothing else the game COULD do with it.
    if (scene == "keep_moved")
        renderer.setPosition(firstBlock, {-140.0f, 130.0f, 60.0f});
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "keep";
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
    renderer.startUp(2400, 1800);

    buildKeep(renderer, scene);

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::cout << "wrote " << out << "\n";

    renderer.shutDown();
    std::cout.flush();

    // ~Root still faults after a clean shutdown on this build, so the exit stays here rather
    // than inside the engine: a game decides when the program ends.
    std::_Exit(0);
}
