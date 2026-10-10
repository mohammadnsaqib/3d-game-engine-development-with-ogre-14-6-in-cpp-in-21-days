// The Lantern Keep -- Day 17. The warden's behaviour comes out of the executable.
//
// Nothing in this file decides what the warden does. It measures three numbers each tick --
// how long the warden has been in its state, how far the nearest intruder is, how far it has
// strayed from home -- hands them to `keepstone::Behaviour`, and moves the warden at whatever
// speed comes back. The decisions are in `data/warden.behaviour`.
//
// THE RUN DIRECTORY GETS ITS OWN COPY of the behaviour file, and the reload scenes edit THAT.
// A program that live-reloads by writing to its own source tree is a program that cannot be run
// twice, and this book's captures have to be reproducible.

#include "keepstone/Behaviour.h"
#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "idle",          // 10 ticks: the timer has not run out, so the warden has not moved
    "patrol",        // 60 ticks, nobody about: idle -> patrol, and it walks
    "chase",         // an intruder inside 200 units: patrol -> chase, and it speeds up
    "returning",     // the intruder gone: chase -> return, heading back
    "reload_before", // 80 ticks on the shipped file, patrol speed 70
    "reload_after",  // the same 80 ticks, patrol speed edited to 210 at tick 40
    "bad_reload",    // the same, but the edit is malformed: REFUSED, and the warden carries on
    "no_file",       // no behaviour file at all: the warden stands still and says why
    "two_wardens",   // two wardens, one file, two states at once
};

const float DT = 0.02f;                     // the fixed tick from Day 6, 50 a second
const keepstone::Vec3 HOME { -150.0f, 20.0f, 0.0f };
const keepstone::Vec3 INTRUDER { 200.0f, 20.0f, 0.0f };

float distance(const keepstone::Vec3& a, const keepstone::Vec3& b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Copy the chosen behaviour file into the run directory and hand back the working path.
bool installBehaviour(const std::string& from, const std::string& to)
{
    std::ifstream in(from.c_str(), std::ios::binary);
    if (!in) return false;
    std::ofstream out(to.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << in.rdbuf();
    return true;
}

struct Warden
{
    keepstone::Vec3 position = HOME;
    keepstone::BehaviourState bs;
    std::vector<keepstone::Vec3> trail;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "patrol";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    const std::string data = LK_GAME_DATA;
    const std::string working = "warden.behaviour";   // in the run directory, not the book

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    renderer.setAmbient({0.19f, 0.19f, 0.24f});
    renderer.setCamera({0.0f, 250.0f, 520.0f}, {0.0f, 20.0f, 0.0f});

    // -- the behaviour asset ------------------------------------------------------------------
    keepstone::Behaviour behaviour;
    bool haveFile = true;
    if (scene == "no_file")
    {
        // Nothing is installed. load() fails, `loaded()` stays false, and every step() returns
        // a speed of zero -- which is the correct answer to "what does an object with no
        // behaviour do", and a much better one than a crash.
        haveFile = false;
        behaviour.load(working + ".missing");
    }
    else
    {
        installBehaviour(data + "/warden.behaviour", working);
        if (!behaviour.load(working))
            std::printf("behaviour did not load: %s\n", behaviour.lastError().c_str());
    }

    // THE TICK COUNTS ARE ARITHMETIC, NOT GUESSES. The warden idles until `timer > 0.40`, which
    // is tick 20. It then patrols at 70 units a second, or 1.4 a tick, from x = -150. The chase
    // rule fires when the intruder is inside 200 units, and the intruder stands at x = 200, so
    // the warden must reach x = 0: that is 150 / 1.4 = 107 more ticks, or tick 128. A scene that
    // means to photograph a chase and stops at tick 90 photographs a patrol, which is what the
    // first version of this file did.
    const int ticks = (scene == "idle") ? 10
                    : (scene == "patrol" || scene == "two_wardens") ? 60
                    : (scene == "chase") ? 160
                    : (scene == "returning") ? 200
                    : 80;

    const bool hasIntruder = (scene == "chase" || scene == "returning");
    // Late enough that the chase has actually begun (tick 128), early enough to leave 60 ticks
    // of walking home to photograph.
    const int  intruderLeavesAt = (scene == "returning") ? 140 : 1000000;
    const int  reloadAt = (scene == "reload_after" || scene == "bad_reload") ? 40 : 1000000;

    std::vector<Warden> wardens(scene == "two_wardens" ? 2 : 1);
    if (scene == "two_wardens")
    {
        // Two objects, one asset. The second starts already patrolling, so the same file drives
        // two different states in the same tick -- which is the whole reason BehaviourState is
        // a separate struct from Behaviour.
        wardens[1].position = { -40.0f, 20.0f, -90.0f };
        wardens[1].bs.current = "patrol";
    }

    int reloads = 0, refused = 0;
    std::string reloadNote = "none attempted";

    for (int t = 0; t < ticks; ++t)
    {
        if (t == reloadAt)
        {
            const std::string src = (scene == "bad_reload") ? "/warden_broken.behaviour"
                                                            : "/warden_fast.behaviour";
            installBehaviour(data + src, working);
            // The stamp has second resolution, so force a re-read rather than waiting for the
            // clock to tick over. A real editor's save is seconds apart; a test is not.
            if (behaviour.reloadIfChanged() || behaviour.load(working))
            {
                ++reloads;
                reloadNote = "accepted at tick " + std::to_string(t);
            }
            else
            {
                ++refused;
                reloadNote = "REFUSED at tick " + std::to_string(t)
                           + ": " + behaviour.lastError();
            }
        }

        for (Warden& w : wardens)
        {
            keepstone::Vars vars;
            vars.home = distance(w.position, HOME);
            vars.intruder = (hasIntruder && t < intruderLeavesAt)
                          ? distance(w.position, INTRUDER) : 1.0e9f;

            const float speed = behaviour.step(w.bs, vars, DT);

            keepstone::Vec3 dir { 0.0f, 0.0f, 0.0f };
            if (w.bs.current == "patrol")      dir = { 1.0f, 0.0f, 0.0f };
            else if (w.bs.current == "chase")  dir = { INTRUDER.x > w.position.x ? 1.0f : -1.0f,
                                                       0.0f, 0.0f };
            else if (w.bs.current == "return") dir = { HOME.x > w.position.x ? 1.0f : -1.0f,
                                                       0.0f, 0.0f };

            w.position.x += dir.x * speed * DT;
            if (t % 4 == 0) w.trail.push_back(w.position);
        }
    }

    // -- the courtyard ------------------------------------------------------------------------
    for (int i = 0; i < 9; ++i)
        renderer.addBlock({ -320.0f + 80.0f * i, 40.0f, -190.0f },
                          { 0.17f, 0.36f + 0.06f * (i % 3), 0.17f }, 0.0f);

    // Home, as a post. The warden's `home` variable is measured from here.
    renderer.addBlock({ HOME.x, 12.0f, HOME.z }, { 0.14f, 0.05f, 0.14f }, 45.0f);
    if (hasIntruder)
    {
        const bool stillThere = (ticks <= intruderLeavesAt);
        if (stillThere)
        {
            renderer.addBlock({ INTRUDER.x, 30.0f, INTRUDER.z }, { 0.10f, 0.30f, 0.10f }, 30.0f);
            renderer.addLantern({ INTRUDER.x, 78.0f, INTRUDER.z }, { 0.9f, 0.4f, 0.35f });
        }
    }

    for (const Warden& w : wardens)
    {
        for (const keepstone::Vec3& p : w.trail)
            renderer.addBlock({ p.x, 6.0f, p.z }, { 0.04f, 0.04f, 0.04f }, 0.0f);
        renderer.addBlock(w.position, { 0.13f, 0.40f, 0.13f }, 0.0f);
        renderer.addLantern({ w.position.x, 96.0f, w.position.z }, { 1.0f, 0.88f, 0.66f });
    }

    // The reload status, as a marker the picture can carry: nothing when no reload was tried,
    // a post when one was accepted, a low bar when one was refused. Without it two scenes that
    // END in the same place would be the same picture, and "the refused edit changed nothing"
    // would be a claim the figure could not make.
    //
    // THE POSITIONS ARE COMPUTED, because two guesses at them were both wrong. The camera sits
    // at (0, 250, 520) looking at (0, 20, 0) with OGRE's default 45-degree vertical field of
    // view and a 4:3 viewport, so the visible half-width at a given depth is
    // `z_from_camera * tan(22.5) * 4/3`. A marker at x = 250, z = 210 lands at normalised x of
    // +1.22 -- outside the frustum. It was created, it was real, and it was off the edge of
    // every picture, so `bad_reload` came out byte-identical to `reload_before` and the
    // chapter's evidence quietly went from nine frames to eight.
    //
    // A MARKER THAT EXISTS AND CANNOT BE SEEN PASSES EVERY CHECK IN THIS REPO. The frame count
    // is right, the scene list is right, the program exits 0. Only comparing the bytes catches
    // it. x = 150, z = 150 lands at +0.64 and is comfortably inside.
    if (reloads)  renderer.addBlock({ 150.0f, 70.0f, 150.0f }, { 0.10f, 0.34f, 0.10f }, 0.0f);
    if (refused)  renderer.addBlock({ 150.0f, 20.0f, 150.0f }, { 0.34f, 0.08f, 0.34f }, 0.0f);
    // And the same for a warden with no rules at all: without this, `no_file` is a warden
    // standing at home, which is pixel for pixel what `idle` is.
    if (!behaviour.loaded())
        renderer.addBlock({ -150.0f, 70.0f, 150.0f }, { 0.30f, 0.08f, 0.30f }, 45.0f);

    // -- what happened -------------------------------------------------------------------------
    std::printf("scene            %s\n", scene.c_str());
    std::printf("behaviour file   %s\n", haveFile ? "warden.behaviour" : "(none installed)");
    std::printf("states loaded    %d\n", static_cast<int>(behaviour.states().size()));
    if (!behaviour.loaded())
        std::printf("load error       %s\n", behaviour.lastError().c_str());
    for (const keepstone::State& s : behaviour.states())
        std::printf("  %-8s speed %5.1f  rules %d\n", s.name.c_str(), s.speed,
                    static_cast<int>(s.rules.size()));
    std::printf("ticks run        %d  (dt %.2f s)\n", ticks, DT);
    std::printf("reload           %s\n", reloadNote.c_str());
    for (std::size_t i = 0; i < wardens.size(); ++i)
        std::printf("warden %d         state %-8s x %8.1f  transitions %d\n",
                    static_cast<int>(i), wardens[i].bs.current.c_str(),
                    wardens[i].position.x, wardens[i].bs.transitions);

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    return 0;
}
