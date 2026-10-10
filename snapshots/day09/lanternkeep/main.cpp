// The Lantern Keep -- Day 9.
//
// Yesterday's world handed out raw pointers and reused slots. Today the reader sees what that
// costs, in a picture, and then fixes it.
//
// THE TWO SCENES THAT MATTER ARE `stale_pointer` AND `handle_checked`. They run the same five
// steps in the same order:
//
//   1  create the warden            4  create a brazier (which REUSES the warden's slot)
//   2  remember it                  5  move "the warden"
//   3  destroy the warden
//
// The only difference is what step 2 remembers. A raw pointer survives step 3 without
// complaint and step 5 then moves the brazier. A handle stops resolving and step 5 does
// nothing, which is the correct answer to "move an object that no longer exists".

#include "keepstone/Engine.h"
#include "keepstone/Handle.h"
#include "keepstone/Loop.h"
#include "keepstone/Renderer.h"
#include "keepstone/World.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

const char* SCENES[] = {
    "stale_pointer",  // Day 8's bug, photographed: the brazier moves instead of the warden
    "handle_checked", // the same five steps with a handle: nothing moves, and that is right
    "intact",         // nobody is destroyed; the warden stands where it was put
    "destroyed",      // the warden is destroyed and nothing replaces it
    "reused",         // the slot is reused and both objects are drawn where they belong
    "respawn",        // destroy and recreate the warden three times over
    "churn",          // twelve objects created and destroyed; generations climb
    "walls_gone",     // every wall destroyed: the slots survive, the objects do not
    "many",           // a fuller courtyard, no destroys
    "few",            // a sparse one
};

keepstone::Handle addBlock(keepstone::World& w, const std::string& name,
                           keepstone::Vec3 pos, keepstone::Vec3 scale)
{
    keepstone::Handle h = w.create(name);
    keepstone::GameObject* o = w.get(h);
    o->transform = new keepstone::Transform();
    o->transform->position = pos;
    o->transform->scale = scale;
    o->visual = new keepstone::Visual();
    return h;
}

void drawSystem(keepstone::World& world, keepstone::Renderer& renderer)
{
    for (keepstone::Handle h : world.all())
    {
        keepstone::GameObject* o = world.get(h);
        if (!o || !o->transform) continue;
        if (o->glow)   renderer.addLantern(o->transform->position, o->glow->light);
        if (o->visual) renderer.addBlock(o->transform->position, o->transform->scale,
                                         o->transform->yaw);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "intact";
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
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    renderer.setAmbient({0.22f, 0.22f, 0.27f});
    renderer.setCamera({0.0f, 150.0f, 430.0f}, {0.0f, 40.0f, 0.0f});

    keepstone::World world;

    // A lantern, so the courtyard is lit whatever else happens.
    {
        keepstone::Handle l = world.create("lantern");
        keepstone::GameObject* o = world.get(l);
        o->transform = new keepstone::Transform();
        o->transform->position = { 170.0f, 190.0f, 210.0f };
        o->glow = new keepstone::Glow();
    }

    const int walls = (scene == "many") ? 14 : (scene == "few") ? 3 : 7;
    for (int i = 0; i < walls; ++i)
    {
        keepstone::Handle w = addBlock(world, "wall" + std::to_string(i),
                                       { -150.0f + (300.0f / walls) * i, 40.0f, -70.0f },
                                       { 0.22f, 0.44f + 0.09f * (i % 3), 0.22f });
        // The slots stay allocated after this; only the objects in them die. That is the
        // difference between a slot count and a living count, and the printout shows both.
        if (scene == "walls_gone") world.destroy(w);
    }

    // -- the warden, and the five steps ------------------------------------------------------
    keepstone::Handle warden = addBlock(world, "warden", { -40.0f, 34.0f, 110.0f },
                                        { 0.22f, 0.44f, 0.22f });

    // STEP 2, BOTH WAYS. One of these is kept across a destroy and one of them should not be.
    keepstone::GameObject* wardenPtr = world.get(warden);   // the Day 8 habit
    keepstone::Handle      wardenH   = warden;              // the Day 9 habit

    if (scene == "stale_pointer" || scene == "handle_checked" ||
        scene == "destroyed" || scene == "reused" || scene == "respawn")
    {
        world.destroy(warden);
    }

    if (scene == "stale_pointer" || scene == "handle_checked" || scene == "reused")
    {
        // The brazier lands in the warden's old slot. Nothing about that is wrong; a world that
        // never reused a slot would grow forever.
        addBlock(world, "brazier", { 95.0f, 30.0f, 110.0f }, { 0.30f, 0.30f, 0.30f });
    }

    if (scene == "respawn")
    {
        for (int i = 0; i < 3; ++i)
        {
            keepstone::Handle h = addBlock(world, "warden", { -40.0f + 30.0f * i, 34.0f, 110.0f },
                                           { 0.22f, 0.44f, 0.22f });
            if (i < 2) world.destroy(h);
        }
    }

    if (scene == "churn")
    {
        for (int i = 0; i < 12; ++i)
        {
            keepstone::Handle h = addBlock(world, "spark" + std::to_string(i),
                                           { -120.0f + 22.0f * i, 120.0f, 60.0f },
                                           { 0.10f, 0.10f, 0.10f });
            if (i % 2) world.destroy(h);
        }
    }

    // -- STEP 5: move "the warden" ------------------------------------------------------------
    if (scene == "stale_pointer")
    {
        // NOTHING HERE LOOKS WRONG. The pointer is not null, the memory is valid, the object is
        // alive. It is simply not the warden any more -- and the brazier is about to jump.
        if (wardenPtr && wardenPtr->transform)
            wardenPtr->transform->position = { -40.0f, 150.0f, 110.0f };
        std::printf("step 5           moved through a RAW POINTER kept across the destroy\n");
        std::printf("it pointed at    %s\n",
                     wardenPtr ? wardenPtr->name.c_str() : "(null)");
    }
    else if (scene == "handle_checked")
    {
        keepstone::GameObject* o = world.get(wardenH);
        if (o && o->transform)
            o->transform->position = { -40.0f, 150.0f, 110.0f };
        std::printf("step 5           moved through a HANDLE\n");
        std::printf("it resolved to   %s\n", o ? o->name.c_str() : "(nothing -- correct)");
    }

    drawSystem(world, renderer);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("objects living   %d\n", world.livingCount());
    std::printf("slots allocated  %d\n", world.slotCount());
    for (const std::string& line : world.describe())
        std::printf("%s\n", line.c_str());

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
