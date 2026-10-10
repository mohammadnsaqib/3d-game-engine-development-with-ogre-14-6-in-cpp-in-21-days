// The Lantern Keep -- Day 14.
//
// The warden has walked through the keep wall since Day 6 and nobody has minded, because
// nothing in the engine has ever known the wall was there.
//
// THE TWO SCENES THAT MATTER ARE `tunnelled` AND `stopped`. Same warden, same speed, same wall,
// same tick rate. One resolves the move by checking where he ENDS UP; the other checks the path
// he took to get there. The first lets him through a solid wall, and the printed numbers say so.

#include "keepstone/Collision.h"
#include "keepstone/Engine.h"
#include "keepstone/Prefab.h"
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
    "tunnelled",     // move-then-check at speed: straight through a solid wall
    "stopped",       // the swept test: the same walk, stopped at the face
    "slow_discrete", // move-then-check at a crawl: correct, which is why the bug survives
    "slow_swept",    // the sweep at the same crawl: the same answer, as it should be
    "thick_wall",    // move-then-check against a wall thicker than one tick's travel: correct
    "ray_hit",       // a ray down the courtyard, stopping at the first thing it meets
    "ray_miss",      // the same ray aimed over the top of everything
    "volumes",       // every collision box drawn, so the reader can see what the engine sees
    "corner",        // a diagonal move into a corner: stopped by whichever face is nearer
    "no_walls",      // collision on, nothing to hit: the sweep costs nothing and blocks nothing
};

const float TICK = 1.0f / 50.0f;

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "stopped";
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

    renderer.setAmbient({0.21f, 0.21f, 0.26f});
    renderer.setCamera({0.0f, 170.0f, 450.0f}, {0.0f, 40.0f, 0.0f});
    renderer.addLantern({170.0f, 200.0f, 220.0f}, {1.0f, 0.88f, 0.66f});

    // -- the wall the warden is about to meet -------------------------------------------------
    keepstone::Collision collision;

    const bool thick = (scene == "thick_wall");
    const float wallHalfX = thick ? 26.0f : 4.0f;      // a thin wall is the interesting case
    const float wallX = 60.0f;

    if (scene != "no_walls")
    {
        keepstone::Box wall;
        wall.centre = { wallX, 40.0f, 120.0f };
        wall.half   = { wallHalfX, 40.0f, 30.0f };
        collision.add(wall, "keep-wall");
        renderer.addBlock(wall.centre,
                          { wall.half.x / 50.0f, wall.half.y / 50.0f, wall.half.z / 50.0f }, 0.0f);

        if (scene == "corner")
        {
            keepstone::Box side;
            side.centre = { 10.0f, 40.0f, 176.0f };
            side.half   = { 60.0f, 40.0f, 4.0f };
            collision.add(side, "side-wall");
            renderer.addBlock(side.centre,
                              { side.half.x / 50.0f, side.half.y / 50.0f, side.half.z / 50.0f },
                              0.0f);
        }
    }

    // -- the warden ---------------------------------------------------------------------------
    keepstone::Box warden;
    // START AND SPEED ARE BOTH CHOSEN, and the arithmetic is worth writing down because getting
    // it wrong is what makes a tunnelling demonstration quietly fail to tunnel.
    //
    // A mover skips a wall only when one tick's travel clears BOTH bodies:
    //
    //     delta  >  2 x (wall half-width + mover half-width)  =  2 x (4 + 9)  =  26 units
    //
    // and only when a tick actually LANDS in the window between "not yet touching" and "already
    // past". With delta = 60 that window is x in (13, 47); stepping 60 from -165 puts a tick at
    // x = 15, which is inside it. The first attempt used 18 units a tick, comfortably under the
    // 26-unit threshold, so the discrete resolver caught it every time and the scene named
    // `tunnelled` reported `past the wall: no`.
    warden.centre = { -165.0f, 40.0f, 120.0f };
    warden.half   = { 9.0f, 20.0f, 9.0f };

    const bool crawl = (scene == "slow_discrete" || scene == "slow_swept");
    const bool swept = (scene == "stopped" || scene == "slow_swept" ||
                        scene == "corner" || scene == "no_walls");
    const float speed = crawl ? 40.0f : 3000.0f;   // 3000 u/s is 60 units a tick

    std::vector<keepstone::Vec3> path;
    int blockedTicks = 0;
    float firstToi = -1.0f;

    if (scene != "ray_hit" && scene != "ray_miss" && scene != "volumes")
    {
        for (int tick = 0; tick < 50; ++tick)
        {
            keepstone::Vec3 delta { speed * TICK, 0.0f, 0.0f };
            if (scene == "corner") { delta.x = 420.0f * TICK; delta.z = 420.0f * TICK; }

            bool blocked = false;
            float toi = 1.0f;
            keepstone::Vec3 next = swept
                ? collision.moveSwept(warden, delta, &blocked, &toi)
                : collision.moveDiscrete(warden, delta, &blocked);

            if (blocked) { ++blockedTicks; if (firstToi < 0.0f) firstToi = toi; }
            warden.centre = next;
            path.push_back(warden.centre);
        }

        for (std::size_t i = 0; i < path.size(); i += 2)
            renderer.addBlock({ path[i].x, 14.0f, path[i].z }, {0.05f, 0.05f, 0.05f}, 0.0f);
        renderer.addBlock(warden.centre, {0.18f, 0.40f, 0.18f}, 0.0f);
    }

    // -- the ray scenes -----------------------------------------------------------------------
    keepstone::Hit hit;
    if (scene == "ray_hit" || scene == "ray_miss")
    {
        const keepstone::Vec3 from { -260.0f, scene == "ray_miss" ? 160.0f : 40.0f, 120.0f };
        hit = collision.raycast(from, {1.0f, 0.0f, 0.0f}, 600.0f);

        // The ray drawn as a line of markers, stopping where it stopped.
        const float end = hit.hit ? hit.distance : 600.0f;
        for (float t = 0.0f; t < end; t += 14.0f)
            renderer.addBlock({ from.x + t, from.y, from.z }, {0.04f, 0.04f, 0.04f}, 0.0f);
        if (hit.hit)
            renderer.addBlock(hit.point, {0.14f, 0.14f, 0.14f}, 0.0f);
    }

    if (scene == "volumes")
    {
        // WHAT THE ENGINE SEES, NOT WHAT THE ARTIST DREW. A collision box is not the mesh, and
        // the commonest source of "it stopped me and there was nothing there" is the two having
        // drifted apart. Drawing them is the cheapest debugging tool in the book.
        for (int i = 0; i < collision.boxCount(); ++i)
        {
            const keepstone::Box& b = collision.box(i);
            renderer.addBlock(b.centre, { b.half.x / 50.0f, b.half.y / 50.0f, b.half.z / 50.0f },
                              0.0f);
        }
        renderer.addBlock(warden.centre, {0.18f, 0.40f, 0.18f}, 0.0f);
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("resolver         %s\n", swept ? "swept (checks the path)"
                                               : "discrete (checks the destination)");
    std::printf("speed            %.0f units/second  (%.1f per tick)\n", speed, speed * TICK);
    std::printf("wall half-width  %.0f units at x = %.0f\n", wallHalfX, wallX);
    std::printf("boxes            %d\n", collision.boxCount());
    if (!path.empty())
    {
        std::printf("warden ended x   %.1f\n", warden.centre.x);
        std::printf("past the wall    %s\n", warden.centre.x > wallX ? "YES -- through solid stone"
                                                                     : "no");
        std::printf("ticks blocked    %d\n", blockedTicks);
    }
    if (scene == "ray_hit" || scene == "ray_miss")
    {
        std::printf("ray hit          %s\n", hit.hit ? collision.boxName(hit.which).c_str()
                                                     : "nothing");
        if (hit.hit) std::printf("ray distance     %.1f\n", hit.distance);
    }

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
