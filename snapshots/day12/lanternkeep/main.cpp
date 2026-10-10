// The Lantern Keep -- Day 12.
//
// LOOK AT WHAT IS MISSING FROM THIS FILE. There is no loop that places walls, no position for a
// brazier, no speed for the warden, and no colour for either lantern. All of that moved into
// `data/keep.prefab`, and this program is now the thing that runs a courtyard rather than the
// thing that contains one.
//
// The keep looks the same as it did on Day 11. That is the result: the same world, described
// somewhere a person can change without a compiler.

#include "keepstone/Assets.h"
#include "keepstone/Engine.h"
#include "keepstone/Handle.h"
#include "keepstone/Prefab.h"
#include "keepstone/Profiler.h"
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
    "from_data",    // the whole keep, built from data/keep.prefab
    "broken",       // data/broken.prefab: one misspelled word. MEANT to fail, and it names the line
    "walked",       // the same keep after the warden has walked for two seconds
    "storm",        // the prefab spawned four times over: a spawn storm, and the counts to match
    "despawned",    // half the walls destroyed after spawning
    "respawned",    // destroyed, then spawned again into the reused slots
    "one_lantern",  // the blue lantern never spawned
    "dense",        // every repeat count tripled
    "sparse",       // the walls thinned to three
    "stacked",      // the prefab spawned twice with NO offset: 28 objects, and it looks like 14
    "night",        // ambient dropped to nothing: only the two lanterns the FILE declares
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "from_data";
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

    renderer.setAmbient(scene == "night" ? keepstone::Colour{0.03f, 0.03f, 0.05f}
                                         : keepstone::Colour{0.20f, 0.20f, 0.25f});
    renderer.setCamera({0.0f, 150.0f, 440.0f}, {0.0f, 40.0f, 0.0f});

    keepstone::World world;
    keepstone::TransformSystem bridge(world, renderer);

    // -- the courtyard arrives from a file ----------------------------------------------------
    keepstone::Prefab prefab;
    std::string error;
    const std::string file = std::string(LK_GAME_DATA) +
                             (scene == "broken" ? "/broken.prefab" : "/keep.prefab");

    if (!prefab.loadFile(file, error))
    {
        // NAMED, AND THE PROGRAM STOPS. A level that half-loads is worse than one that does not
        // load: the game runs, the courtyard is wrong, and nobody knows which line did it.
        std::printf("scene            %s\n", scene.c_str());
        std::printf("prefab error     %s\n", error.c_str());
        std::printf("no frame written\n");
        std::fflush(stdout);
        std::_Exit(1);
    }

    const int spawnTimes = (scene == "storm") ? 4 : 1;
    std::vector<keepstone::Handle> spawned;
    for (int s = 0; s < spawnTimes; ++s)
    {
        std::vector<keepstone::Handle> batch = prefab.spawnAll(world);

        // EACH BATCH IS LIFTED CLEAR OF THE LAST. Spawning one prefab four times without this
        // puts four objects at identical coordinates, and four coincident blocks render as one:
        // the object count says twelve walls and the picture shows three. A spawn storm that
        // looks like a quiet courtyard is the kind of bug that gets chased in the wrong file.
        if (s > 0)
        {
            for (keepstone::Handle h : batch)
            {
                keepstone::GameObject* o = world.get(h);
                if (o && o->transform)
                {
                    keepstone::Vec3 p = o->transform->position;
                    p.y += 46.0f * s;
                    p.z += 26.0f * s;
                    o->transform->moveTo(p);
                }
            }
        }
        spawned.insert(spawned.end(), batch.begin(), batch.end());
    }

    if (scene == "one_lantern")
    {
        keepstone::Handle h = world.find("lantern_blue");
        if (!h.isNull()) world.destroy(h);
    }

    if (scene == "despawned" || scene == "respawned")
    {
        for (int i = 0; i < 9; i += 2)
        {
            keepstone::Handle h = world.find("wall" + std::to_string(i));
            if (!h.isNull()) world.destroy(h);
        }
    }
    if (scene == "respawned")
    {
        // Straight back into the slots that were just freed. The generations moved on, so any
        // handle from the first spawn is already refusing to resolve.
        prefab.spawnAll(world);
    }

    if (scene == "sparse")
    {
        for (int i = 3; i < 9; ++i)
        {
            keepstone::Handle h = world.find("wall" + std::to_string(i));
            if (!h.isNull()) world.destroy(h);
        }
    }
    if (scene == "dense")
    {
        // Two more batches, each lifted, so the count and the picture agree.
        for (int s = 1; s <= 2; ++s)
        {
            std::vector<keepstone::Handle> batch = prefab.spawnAll(world);
            for (keepstone::Handle h : batch)
            {
                keepstone::GameObject* o = world.get(h);
                if (o && o->transform)
                {
                    keepstone::Vec3 p = o->transform->position;
                    p.x += 18.0f * s;
                    p.y += 30.0f * s;
                    o->transform->moveTo(p);
                }
            }
        }
    }
    if (scene == "stacked")
    {
        // DELIBERATELY WITHOUT THE LIFT, so the chapter can show the two frames side by
        // side.
        // The printout says the object count doubled; the picture does not change at all.
        prefab.spawnAll(world);
    }

    // Attach everything the prefab made to the renderer, through Day 10's one bridge.
    for (keepstone::Handle h : world.all())
        bridge.attach(h);

    // -- the warden walks, if this scene lets him ---------------------------------------------
    const int ticks = (scene == "walked") ? 100 : 0;
    for (int t = 0; t < ticks; ++t)
    {
        for (keepstone::Handle h : world.all())
        {
            keepstone::GameObject* o = world.get(h);
            if (!o || !o->mover || !o->transform) continue;
            keepstone::Vec3 p = o->transform->position;
            p.x += o->mover->velocity.x * (1.0f / 50.0f);
            o->transform->moveTo(p);
        }
        bridge.sync();
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("prefab file      %s\n", scene == "broken" ? "broken.prefab" : "keep.prefab");
    std::printf("definitions      %d\n", static_cast<int>(prefab.definitions().size()));
    std::printf("objects declared %d\n", prefab.totalObjects());
    std::printf("objects living   %d\n", world.livingCount());
    std::printf("slots allocated  %d\n", world.slotCount());
    std::printf("attached to view %d\n", bridge.attachedCount());
    std::printf("ticks run        %d\n", ticks);

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
