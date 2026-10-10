// The Lantern Keep -- Day 10.
//
// Until today the game rebuilt the whole scene every run: walk the world, call addBlock for
// everything, throw it away, do it again. That works and it is the wrong shape -- the renderer
// should be told what exists once, and then told only what changed.
//
// THE SCENE THAT MATTERS IS `stale_nodes`. The world is correct, the bridge runs, the renderer
// is correct, and the picture is wrong, because nothing marks a transform dirty and so nothing
// is ever pushed across. Every part works and the whole does not.

#include "keepstone/Engine.h"
#include "keepstone/Handle.h"
#include "keepstone/Renderer.h"
#include "keepstone/TransformSystem.h"
#include "keepstone/World.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "bridged",      // the bridge working: the patrol has moved and the picture agrees
    "stale_nodes",  // marking switched off: the world moved, the screen did not
    "unmoved",      // nobody moves. MEASURED: byte-identical to `stale_nodes`
    "halfway",      // the patrol stopped mid-walk: twenty-five ticks, not fifty
    "one_moved",    // a single object moves: one push, everything else skipped
    "all_moved",    // every object moves: nothing is skipped
    "far",          // a longer patrol
    "near",         // a shorter one
    "many",         // eighteen walls, three movers
    "few",          // four walls, one mover
};

struct Patrol
{
    keepstone::Handle handle;
    float             speed;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "bridged";
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
    keepstone::TransformSystem bridge(world, renderer);

    if (scene == "stale_nodes")
        bridge.setMarkingEnabled(false);

    // A lantern with a node of its own, so light crosses the same bridge as geometry.
    {
        keepstone::Handle l = world.create("lantern");
        keepstone::GameObject* o = world.get(l);
        o->transform = new keepstone::Transform();
        o->transform->position = { 170.0f, 190.0f, 210.0f };
        o->glow = new keepstone::Glow();
        bridge.attach(l);
    }

    const int walls = (scene == "many") ? 18 : (scene == "few") ? 4 : 9;
    for (int i = 0; i < walls; ++i)
    {
        keepstone::Handle w = world.create("wall" + std::to_string(i));
        keepstone::GameObject* o = world.get(w);
        o->transform = new keepstone::Transform();
        o->transform->position = { -150.0f + (300.0f / walls) * i, 40.0f, -70.0f };
        o->transform->scale = { 0.20f, 0.42f + 0.09f * (i % 3), 0.20f };
        o->visual = new keepstone::Visual();
        bridge.attach(w);
    }

    const int movers = (scene == "many") ? 3 : (scene == "few") ? 1 :
                       (scene == "one_moved") ? 1 : 2;
    std::vector<Patrol> patrols;
    for (int i = 0; i < movers; ++i)
    {
        keepstone::Handle p = world.create("patrol" + std::to_string(i));
        keepstone::GameObject* o = world.get(p);
        o->transform = new keepstone::Transform();
        o->transform->position = { -120.0f + 90.0f * i, 34.0f, 120.0f };
        o->transform->scale = { 0.20f, 0.40f, 0.20f };
        o->visual = new keepstone::Visual();
        bridge.attach(p);
        patrols.push_back({ p, 70.0f + 25.0f * i });
    }

    const float distance = (scene == "far") ? 2.4f : (scene == "near") ? 0.4f : 1.2f;

    // -- the tick loop -----------------------------------------------------------------------
    const int ticks = (scene == "unmoved") ? 0 : (scene == "halfway") ? 25 : 50;
    for (int t = 0; t < ticks; ++t)
    {
        for (std::size_t i = 0; i < patrols.size(); ++i)
        {
            keepstone::GameObject* o = world.get(patrols[i].handle);
            if (!o) continue;

            keepstone::Vec3 p = o->transform->position;
            p.x += patrols[i].speed * distance * (1.0f / 50.0f);

            if (bridge.markingEnabled())
            {
                // The right way: move through the transform, which marks it dirty.
                o->transform->moveTo(p);
            }
            else
            {
                // THE DEFECT, WRITTEN OUT. The position really is updated -- the world is not
                // lying about where the patrol is -- and `dirty` is never set, so the bridge
                // skips it forever.
                o->transform->position = p;
            }
        }

        if (scene == "all_moved")
        {
            // Everything moves a little, so nothing is skipped and the counts say so.
            for (keepstone::Handle h : world.all())
            {
                keepstone::GameObject* o = world.get(h);
                if (o && o->transform && o->visual)
                {
                    keepstone::Vec3 p = o->transform->position;
                    p.y += 0.25f;
                    o->transform->moveTo(p);
                }
            }
        }

        bridge.sync();
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("marking enabled  %s\n", bridge.markingEnabled() ? "yes" : "NO");
    std::printf("objects attached %d\n", bridge.attachedCount());
    std::printf("ticks run        %d\n", ticks);
    std::printf("pushed last sync %d\n", bridge.pushedLastSync());
    std::printf("skipped last sync %d\n", bridge.skippedLastSync());
    for (std::size_t i = 0; i < patrols.size(); ++i)
    {
        keepstone::GameObject* o = world.get(patrols[i].handle);
        if (o) std::printf("patrol%d world x   %.1f\n", static_cast<int>(i),
                           o->transform->position.x);
    }

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
