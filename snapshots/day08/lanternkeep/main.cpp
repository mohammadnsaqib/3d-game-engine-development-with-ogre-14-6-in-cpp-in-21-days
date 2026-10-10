// The Lantern Keep -- Day 8.
//
// The courtyard stops being a list of draw calls and becomes a world of objects. Nothing new
// appears on screen today -- that is the point. The same keep, built out of things that have
// names and parts instead of out of instructions to the renderer.
//
// WHAT AN OBJECT IS DECIDES WHAT IT DOES. A wall has a transform and a visual; the warden has
// those and a mover; a lantern has a transform and a glow and no visual at all. Nothing asks
// "what kind of object is this" -- the systems below ask "does this have a mover" and act on
// whatever answers.

#include "keepstone/Engine.h"
#include "keepstone/Loop.h"
#include "keepstone/Renderer.h"
#include "keepstone/World.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

const char* SCENES[] = {
    "mixed",       // the keep: walls, a warden, lanterns, and one object with nothing to draw
    "still",       // the same world, zero ticks: the movers have not moved yet
    "drifted",     // the same world, sixty ticks: only the things with movers have moved
    "no_movers",   // every mover removed. MEASURED: byte-identical to `still`
    "slow_movers", // the same movers at a third of the speed
    "all_movers",  // a mover on everything, including the walls
    "no_visuals",  // the objects exist and are simulated; none is drawn
    "glow_only",   // the lanterns alone. MEASURED: byte-identical to `no_visuals`
    "one_lantern", // a single lantern: the same world lit from one side only
    "many",        // sixty objects
    "few",         // six
};

// A SYSTEM, not a method on the object. It runs over whatever has the components it cares
// about, and it does not know or ask what those objects are called.
void moveSystem(keepstone::World& world, float dt)
{
    for (keepstone::GameObject* o : world.all())
    {
        if (!o->mover || !o->transform) continue;
        o->transform->position.x += o->mover->velocity.x * dt;
        o->transform->position.y += o->mover->velocity.y * dt;
        o->transform->position.z += o->mover->velocity.z * dt;
    }
}

// The other system: hand everything with a transform and a visual to the renderer. Day 10 turns
// this into a proper bridge that keeps nodes rather than rebuilding the scene.
void drawSystem(keepstone::World& world, keepstone::Renderer& renderer)
{
    for (keepstone::GameObject* o : world.all())
    {
        if (!o->transform) continue;
        if (o->glow)
            renderer.addLantern(o->transform->position, o->glow->light);
        if (o->visual && o->visual->drawn)
            renderer.addBlock(o->transform->position, o->transform->scale,
                               o->transform->yaw);
    }
}

void buildWorld(keepstone::World& world, const std::string& scene)
{
    const bool movers  = (scene != "no_movers");
    (void)0;
    const bool visuals = (scene != "no_visuals");
    const int  walls   = (scene == "many") ? 40 : (scene == "few") ? 4 : 9;

    if (scene != "glow_only")
    {
        for (int i = 0; i < walls; ++i)
        {
            keepstone::GameObject* w = world.create("wall" + std::to_string(i));
            w->transform = new keepstone::Transform();
            w->transform->position = { -320.0f + (640.0f / walls) * i, 40.0f, -60.0f };
            w->transform->scale = { 0.26f, 0.48f + 0.10f * (i % 3), 0.26f };
            w->transform->yaw = 11.0f * i;
            if (visuals) w->visual = new keepstone::Visual();
            // A wall gets a mover ONLY in the scene that asks what that would look like.
            if (scene == "all_movers")
                w->mover = new keepstone::Mover{ { 0.0f, 22.0f, 0.0f } };
        }

        keepstone::GameObject* warden = world.create("warden");
        warden->transform = new keepstone::Transform();
        // INSIDE THE FRUSTUM, and that took a measurement rather than a guess. The warden began
        // at x = -260, which at this camera's distance and field of view is about seventy units
        // outside the left edge of the picture. Every scene rendered correctly, the warden moved
        // exactly as far as the numbers said, and three frames came out byte-identical because
        // the thing that moved was never in shot. A subject outside the frame is not a small
        // framing problem; it is a figure that proves nothing.
        warden->transform->position = { -150.0f, 34.0f, 90.0f };
        warden->transform->scale = { 0.22f, 0.44f, 0.22f };
        if (visuals) warden->visual = new keepstone::Visual();
        const float speed = (scene == "slow_movers") ? 35.0f : 105.0f;
        if (movers)  warden->mover = new keepstone::Mover{ { speed, 0.0f, 0.0f } };
    }

    // Lanterns: a transform and a glow, and NO visual. They light the courtyard and are not
    // themselves anything you can see, which is a sentence about components rather than about
    // lanterns.
    const int lanterns = (scene == "one_lantern") ? 1 : 2;
    for (int i = 0; i < lanterns; ++i)
    {
        keepstone::GameObject* l = world.create("lantern" + std::to_string(i));
        l->transform = new keepstone::Transform();
        l->transform->position = { i ? -190.0f : 170.0f, 180.0f, 190.0f };
        l->glow = new keepstone::Glow();
        if (i) l->glow->light = { 0.45f, 0.62f, 1.0f };
    }

    // AND ONE OBJECT WITH A TRANSFORM AND NOTHING ELSE. It is in the world, the systems walk
    // over it every tick, and it does nothing and is not drawn. That is legal, and a reader who
    // finds it surprising has not yet believed that the components are what matter.
    keepstone::GameObject* marker = world.create("survey-marker");
    marker->transform = new keepstone::Transform();
    marker->transform->position = { 0.0f, 10.0f, 150.0f };
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "mixed";
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
    buildWorld(world, scene);

    const int ticks = (scene == "still") ? 0 : (scene == "drifted") ? 60 : 24;
    for (int t = 0; t < ticks; ++t)
        moveSystem(world, 1.0f / 50.0f);

    drawSystem(world, renderer);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("objects living   %d\n", world.livingCount());
    std::printf("slots allocated  %d\n", world.slotCount());
    std::printf("ticks run        %d\n", ticks);
    int withMover = 0, withVisual = 0, withGlow = 0;
    for (keepstone::GameObject* o : world.all())
    {
        if (o->mover)  ++withMover;
        if (o->visual) ++withVisual;
        if (o->glow)   ++withGlow;
    }
    std::printf("with a mover     %d\n", withMover);
    std::printf("with a visual    %d\n", withVisual);
    std::printf("with a glow      %d\n", withGlow);
    if (keepstone::GameObject* w = world.find("warden"))
        std::printf("warden x         %.1f\n", w->transform->position.x);

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
