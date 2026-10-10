// The Lantern Keep -- Day 13.
//
// The warden has walked on a timer since Day 6. Today somebody drives him.
//
// NOTHING IN THIS FILE MENTIONS A KEY except the trace and the bindings. The warden's own code
// asks `input.isDown("walk_right")` and has no idea which key that is, which is what makes the
// `rebound` scene a two-line change instead of a search through the gameplay.
//
// THE TRACE IS THE DEVICE. These programs render with no window on screen, and a key press needs
// a focused window to exist -- so the events below stand in for a keyboard. That is not a dodge:
// a replayable input source is how input gets tested, and the action map cannot tell the
// difference.

#include "keepstone/Engine.h"
#include "keepstone/Input.h"
#include "keepstone/Prefab.h"
#include "keepstone/Renderer.h"
#include "keepstone/TransformSystem.h"
#include "keepstone/World.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

namespace {

const char* SCENES[] = {
    "driven",       // the trace drives the warden: right, pause, right again
    "rebound",      // ONE key rebound mid-run; the rest of the trace now means something else
    "menu_open",    // a menu context on top swallows the movement keys
    "menu_closed",  // pushed and popped: the walk resumes, and nothing is left stuck down
    "unbound",      // a trace of keys nothing binds: counted, ignored, no warning storm
    "held",         // a key held for the whole run instead of tapped
    "tapped",       // the same key tapped five times
    "both",         // left and right down together: the later one wins by the game's own rule
    "no_input",     // the same program, no events at all
    "walk_left",    // the other movement key: the same code path, the other direction
    "lift_held",    // the lantern action held to the end: the warden finishes raised
    "rebind_early", // the rebind at tick 10 instead of 40: a much shorter walk
};

// Key codes. THE ENGINE ATTACHES NO MEANING TO THESE NUMBERS -- they are whatever the device
// layer produced, and only the bindings below say what any of them is for.
const int KEY_A = 65, KEY_D = 68, KEY_E = 69, KEY_ESC = 27, KEY_Q = 81;

struct Moment { int tick; keepstone::DeviceEvent event; };

std::vector<Moment> traceFor(const std::string& scene)
{
    using K = keepstone::DeviceEvent;
    std::vector<Moment> t;
    auto add = [&](int tick, K::Kind kind, int code) {
        Moment m; m.tick = tick; m.event.kind = kind; m.event.code = code; t.push_back(m);
    };

    if (scene == "no_input") return t;

    if (scene == "unbound")
    {
        // Keys the play context binds nothing to. A real keyboard sends a great many of these.
        for (int i = 0; i < 8; ++i) { add(4 + i * 6, K::KeyDown, 300 + i);
                                      add(7 + i * 6, K::KeyUp, 300 + i); }
        return t;
    }

    if (scene == "held")   { add(4, K::KeyDown, KEY_D); add(95, K::KeyUp, KEY_D); return t; }
    if (scene == "tapped") { for (int i = 0; i < 5; ++i) { add(4 + i * 16, K::KeyDown, KEY_D);
                                                           add(12 + i * 16, K::KeyUp, KEY_D); }
                             return t; }
    if (scene == "both")   { add(4, K::KeyDown, KEY_D); add(20, K::KeyDown, KEY_A);
                             add(70, K::KeyUp, KEY_A); add(90, K::KeyUp, KEY_D); return t; }
    if (scene == "walk_left")  { add(4, K::KeyDown, KEY_A); add(88, K::KeyUp, KEY_A); return t; }
    if (scene == "lift_held")  { add(4, K::KeyDown, KEY_D); add(40, K::KeyUp, KEY_D);
                                 add(44, K::KeyDown, KEY_E); return t; }   // never released

    // The default trace: walk right, stop, walk right again, and lift the lantern in between.
    add(4,  K::KeyDown, KEY_D);
    add(30, K::KeyUp,   KEY_D);
    add(36, K::KeyDown, KEY_E);
    add(44, K::KeyUp,   KEY_E);
    add(50, K::KeyDown, KEY_D);
    add(92, K::KeyUp,   KEY_D);
    if (scene == "menu_open" || scene == "menu_closed")
    {
        add(2,  K::KeyDown, KEY_ESC);      // opens the menu before the walk begins
        add(3,  K::KeyUp,   KEY_ESC);
    }

    // A TRACE MUST BE IN TICK ORDER, AND THIS SORT IS NOT TIDINESS.
    //
    // The consumer below advances while `trace[next].tick == tick`, so an event sitting behind a
    // later one is never reached -- it is skipped in silence. The menu events above are appended
    // after the walk events and carry earlier ticks, and without this line the menu scenes ran
    // the ordinary walk: same warden position, same frame, context depth 1, and nothing anywhere
    // said an event had been dropped.
    //
    // A replay source that quietly loses input is worse than one that refuses to start, because
    // the run still looks like a run. Sorting is the cheap fix; the expensive lesson is that the
    // consumer's assumption was never checked.
    std::stable_sort(t.begin(), t.end(),
                     [](const Moment& a, const Moment& b) { return a.tick < b.tick; });
    return t;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "driven";
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
    keepstone::Input input;
    keepstone::Engine engine;
    engine.add(&renderer);
    engine.add(&input);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    renderer.setAmbient({0.21f, 0.21f, 0.26f});
    renderer.setCamera({0.0f, 150.0f, 440.0f}, {0.0f, 40.0f, 0.0f});

    keepstone::World world;
    keepstone::TransformSystem bridge(world, renderer);

    keepstone::Prefab prefab;
    std::string error;
    if (!prefab.loadFile(std::string(LK_GAME_DATA) + "/keep.prefab", error))
    { std::printf("prefab error %s\n", error.c_str()); return 1; }
    prefab.spawnAll(world);

    keepstone::Handle warden = world.find("warden");
    if (keepstone::GameObject* w = world.get(warden))
    {
        delete w->mover; w->mover = nullptr;      // driven now, not on a timer
        w->transform->moveTo({ -40.0f, 34.0f, 120.0f });
    }
    for (keepstone::Handle h : world.all()) bridge.attach(h);

    // -- the bindings ------------------------------------------------------------------------
    keepstone::InputContext play("play");
    play.bind(KEY_A, "walk_left");
    play.bind(KEY_D, "walk_right");
    play.bind(KEY_E, "lift_lantern");
    play.bind(KEY_ESC, "open_menu");
    input.push(&play);

    keepstone::InputContext menu("menu");
    menu.bind(KEY_ESC, "close_menu");
    menu.bind(KEY_Q, "quit");

    // MODAL, AND IT HAS TO SAY SO. Declining to bind the movement keys is NOT enough: the stack
    // falls through when the top context has nothing to say, so without this line the warden
    // walks the full distance with the menu open, and the frame is identical to `driven`.
    // Measured before it was fixed, which is how it was found.
    menu.setExclusive(true);

    const std::vector<Moment> trace = traceFor(scene);
    std::size_t next = 0;
    std::vector<float> path;
    bool menuHandled = false;

    for (int tick = 0; tick < 100; ++tick)
    {
        while (next < trace.size() && trace[next].tick == tick)
            input.feed(trace[next++].event);

        if ((scene == "rebound" && tick == 40) || (scene == "rebind_early" && tick == 10))
        {
            // From here the trace's KEY_D means nothing and KEY_Q walks right. The rest of the
            // run therefore stops moving -- which is the point: one call changed what a device
            // event means, and no gameplay code was touched.
            input.rebind(KEY_D, KEY_Q);
        }

        // EDGE-TRIGGERED, NOT LEVEL-TRIGGERED, and this one is worth dwelling on.
        //
        // `open_menu` goes down on the Escape press and is never released, because the key-up
        // arrives after the menu is on top and the menu binds Escape to `close_menu` instead.
        // So the action stays down for the rest of the run. Written as `if (isDown(...)) push`,
        // that re-pushes the menu on every single tick, and the pop in `menu_closed` is undone
        // the tick after it happens.
        //
        // Opening a menu is a thing that happens ONCE when a key goes down. Remembering that it
        // has been handled is what turns a state question into an edge.
        if (input.isDown("open_menu") && !menuHandled)
        {
            input.push(&menu);
            menuHandled = true;
        }
        if (scene == "menu_closed" && tick == 45 && input.contextDepth() == 2)
            input.pop();

        keepstone::GameObject* w = world.get(warden);
        if (w)
        {
            float dx = 0.0f;
            if (input.isDown("walk_left"))  dx -= 90.0f * (1.0f / 50.0f);
            if (input.isDown("walk_right")) dx += 90.0f * (1.0f / 50.0f);
            if (dx != 0.0f)
            {
                keepstone::Vec3 p = w->transform->position;
                p.x += dx;
                w->transform->moveTo(p);
            }
            if (input.isDown("lift_lantern"))
            {
                keepstone::Vec3 p = w->transform->position;
                p.y = 66.0f;                      // held up while the action is down
                w->transform->moveTo(p);
            }
            else if (w->transform->position.y > 34.0f)
            {
                keepstone::Vec3 p = w->transform->position;
                p.y = 34.0f;
                w->transform->moveTo(p);
            }
            path.push_back(w->transform->position.x);
        }
        bridge.sync();
    }

    // The warden's path, one marker every fifth tick, so the frame shows where the input took
    // him rather than only where he ended up.
    for (std::size_t i = 0; i < path.size(); i += 5)
        renderer.addBlock({ path[i], 12.0f, 150.0f }, {0.05f, 0.05f, 0.05f}, 0.0f);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("events fed       %d\n", static_cast<int>(trace.size()));
    std::printf("unresolved       %d\n", input.unresolvedCount());
    std::printf("context depth    %d (top: %s)\n", input.contextDepth(),
                input.top() ? input.top()->name().c_str() : "none");
    if (keepstone::GameObject* w = world.get(warden))
        std::printf("warden x         %.1f\n", w->transform->position.x);
    for (const std::string& line : input.log())
        std::printf("  %s\n", line.c_str());

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
