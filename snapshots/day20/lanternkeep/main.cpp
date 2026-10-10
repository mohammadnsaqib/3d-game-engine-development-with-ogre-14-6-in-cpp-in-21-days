// The Lantern Keep -- Day 20. Profiling: count the work first, time it second.
//
// NO NEW ENGINE TYPES TODAY. Every part is already built -- the world from Day 8, handles from
// Day 9, the dirty flag from Day 10, the boxes from Day 14, the profiler from Day 7. Today is
// about finding out which of them is costing you, and proving that a change helped.
//
// THE CENTRAL DISCIPLINE, AND THE REASON THIS FILE IS SHAPED THE WAY IT IS:
//
//     COUNT THE WORK, THEN TIME IT. A count is exactly reproducible; a time is not.
//
// Run this program twice and every count below comes out identical to the last digit, because a
// count is a property of the algorithm. Run it twice and the milliseconds move, because they are
// a property of the machine at that moment -- what else was running, where the caches were, how
// warm the clock was. Day 7's chapter was written against timings and its captured percentages
// genuinely change between runs; that is not a defect in Day 7, it is what timing IS.
//
// So each cut here is justified by a number that cannot drift -- pushes avoided, tests skipped,
// square roots not taken -- and the clock is then used to CONFIRM it, with a control run to say
// how much of the difference the machine could have produced on its own. A speed-up smaller than
// the gap between two identical runs has not been measured. It has been hoped for.

#include "keepstone/Collision.h"
#include "keepstone/Engine.h"
#include "keepstone/Profiler.h"
#include "keepstone/Renderer.h"
#include "keepstone/World.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "baseline",     // every cut off: the work the naive loop does
    "dirty_on",     // only push transforms that actually moved (Day 10's flag, used properly)
    "broad_on",     // reject distant pairs before the narrow test (Day 14's boxes)
    "sqrt_on",      // compare squared distances and never call sqrt
    "all_on",       // all three together
    "worse",        // a "cut" that adds work: re-sorting every object every tick
    "control",      // the baseline again, unchanged, to show what the MACHINE contributes
    "scale_120",    // the same cuts against 120 objects
    "scale_480",    // and against 480, so the shape of the growth is visible
    "worse_all",    // the bad cut on top of the three good ones: one can swamp three
};

// Every number this program reports about WORK passes through here. Deliberately plain ints:
// a counter that is itself clever is a counter you end up debugging.
struct Work
{
    long long pushes      = 0;   // transforms handed to the renderer
    long long narrowTests = 0;   // full box-against-box tests
    long long sqrtCalls   = 0;   // square roots taken
    long long sorts       = 0;   // whole-array sorts performed
};

struct Options
{
    bool dirty = false;
    bool broad = false;
    bool noSqrt = false;
    bool sort = false;
    int  objects = 240;
    int  ticks = 120;
};

Options optionsFor(const std::string& scene)
{
    Options o;
    if (scene == "dirty_on") o.dirty = true;
    if (scene == "broad_on") o.broad = true;
    if (scene == "sqrt_on")  o.noSqrt = true;
    if (scene == "worse")    o.sort = true;
    if (scene == "worse_all") { o.dirty = o.broad = o.noSqrt = true; o.sort = true; }
    if (scene == "all_on")  { o.dirty = o.broad = o.noSqrt = true; }
    if (scene == "scale_120") { o.dirty = o.broad = o.noSqrt = true; o.objects = 120; }
    if (scene == "scale_480") { o.dirty = o.broad = o.noSqrt = true; o.objects = 480; }
    return o;
}

// The simulation, with each cut switchable. It is the same work in every scene; only which
// shortcuts are allowed changes.
Work simulate(const Options& opt, keepstone::Profiler& prof, double& msOut)
{
    Work w;

    std::vector<keepstone::Box> boxes(static_cast<std::size_t>(opt.objects));
    std::vector<keepstone::Vec3> velocity(boxes.size());
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        const float f = static_cast<float>(i);
        boxes[i].centre = { -600.0f + 5.0f * f, 20.0f, -300.0f + 2.5f * f };
        boxes[i].half   = { 8.0f, 20.0f, 8.0f };
        // A third of them never move. That is the whole point of the dirty flag: in a real keep
        // the walls, the braziers and the flagstones are most of the world and none of them go
        // anywhere.
        velocity[i] = (i % 3 == 0) ? keepstone::Vec3{ 0.0f, 0.0f, 0.0f }
                                   : keepstone::Vec3{ 12.0f + 0.1f * f, 0.0f, 0.0f };
    }

    const auto t0 = std::chrono::steady_clock::now();

    for (int t = 0; t < opt.ticks; ++t)
    {
        prof.beginFrame();
        prof.begin("simulation");

        // -- move, and decide what the renderer needs to be told ------------------------------
        for (std::size_t i = 0; i < boxes.size(); ++i)
        {
            const bool moved = (velocity[i].x != 0.0f);
            boxes[i].centre.x += velocity[i].x * 0.02f;
            // THE CUT: without it, every object is pushed every tick whether it moved or not.
            if (!opt.dirty || moved) ++w.pushes;
        }

        // -- the "optimisation" that is not one ----------------------------------------------
        if (opt.sort)
        {
            // Re-sorting every tick to "help cache coherence". It is real work, it is done
            // every tick, and the array was already in order. See the chapter.
            std::vector<float> keys(boxes.size());
            for (std::size_t i = 0; i < boxes.size(); ++i) keys[i] = boxes[i].centre.x;
            for (std::size_t i = 1; i < keys.size(); ++i)
                for (std::size_t j = i; j > 0 && keys[j] < keys[j - 1]; --j)
                    std::swap(keys[j], keys[j - 1]);
            ++w.sorts;
        }

        // -- who is near whom -----------------------------------------------------------------
        for (std::size_t i = 0; i < boxes.size(); ++i)
            for (std::size_t j = i + 1; j < boxes.size(); ++j)
            {
                const float dx = boxes[i].centre.x - boxes[j].centre.x;
                const float dz = boxes[i].centre.z - boxes[j].centre.z;

                if (opt.broad)
                {
                    // THE CUT: a cheap rejection before the expensive test. Two boxes further
                    // apart than the sum of their half-widths cannot be touching, and finding
                    // that out costs two subtractions.
                    const float reach = boxes[i].half.x + boxes[j].half.x;
                    if (dx > reach || dx < -reach) continue;
                }

                if (opt.noSqrt)
                {
                    // THE CUT: the comparison does not need the distance, only its order, and
                    // squaring both sides preserves order for non-negative numbers.
                    const float d2 = dx * dx + dz * dz;
                    if (d2 > 40.0f * 40.0f) continue;
                }
                else
                {
                    const float d = std::sqrt(dx * dx + dz * dz);
                    ++w.sqrtCalls;
                    if (d > 40.0f) continue;
                }
                ++w.narrowTests;
            }

        prof.end();
        prof.endFrame();
    }

    const auto t1 = std::chrono::steady_clock::now();
    msOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return w;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "baseline";
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
    renderer.setAmbient({0.20f, 0.20f, 0.25f});
    renderer.setCamera({0.0f, 250.0f, 520.0f}, {0.0f, 40.0f, 0.0f});

    const Options opt = optionsFor(scene);
    keepstone::Profiler prof(16.67);

    double ms = 0.0;
    const Work w = simulate(opt, prof, ms);

    // A SECOND, IDENTICAL RUN, as a control. Every scene does it, and what it reports is whether
    // the two runs did the same WORK -- not how their clocks compared.
    //
    // THE TIMING SPREAD IS DELIBERATELY NOT PRINTED, and that decision is the chapter in
    // miniature. Putting "the two runs differed by 4.2%" on stdout would land it in
    // `code/outputs/d20.txt`, which is captured evidence this book checks its prose against --
    // and that number is different every single run. The chapter would then be quoting a
    // measurement nobody can reproduce, in a chapter whose whole argument is that you should not
    // do that. Day 7 already carries the scar: its captured percentages move between runs, and
    // re-running its capture changes five of its nine published figures.
    //
    // What IS printed is the claim that can be checked: both runs did identical work.
    keepstone::Profiler prof2(16.67);
    double ms2 = 0.0;
    const Work w2 = simulate(opt, prof2, ms2);
    const bool sameWork = (w.pushes == w2.pushes && w.narrowTests == w2.narrowTests &&
                           w.sqrtCalls == w2.sqrtCalls && w.sorts == w2.sorts);
    (void)ms; (void)ms2;   // measured, deliberately not reported: see above

    // -- the picture: five bars of WORK, which do not move between runs -------------------------
    // Deliberately NOT the milliseconds. A figure whose bars change every time the book is built
    // is a figure that cannot be checked, and Day 7's captured percentages already demonstrate
    // what that costs.
    const long long bars[5] = { w.pushes / 100, w.narrowTests / 100, w.sqrtCalls / 1000,
                                w.sorts, static_cast<long long>(opt.objects) };
    for (int i = 0; i < 5; ++i)
    {
        const float h = 0.05f + 0.0016f * static_cast<float>(bars[i]);
        renderer.addBlock({ -180.0f + 90.0f * i, 20.0f + h * 45.0f, 150.0f },
                          { 0.14f, h, 0.14f }, 0.0f);
    }
    // The objects themselves, thinned so the courtyard stays readable.
    for (int i = 0; i < opt.objects; i += 8)
        renderer.addBlock({ -600.0f + 5.0f * i, 40.0f, -300.0f + 2.5f * i },
                          { 0.10f, 0.22f, 0.10f }, 0.0f);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("objects          %d\n", opt.objects);
    std::printf("ticks            %d\n", opt.ticks);
    std::printf("cuts             dirty %s, broad %s, no-sqrt %s, sort %s\n",
                opt.dirty ? "on" : "off", opt.broad ? "on" : "off",
                opt.noSqrt ? "on" : "off", opt.sort ? "on" : "off");
    std::printf("WORK -- reproducible, identical on every run\n");
    std::printf("  transform pushes   %lld\n", w.pushes);
    std::printf("  narrow tests       %lld\n", w.narrowTests);
    std::printf("  sqrt calls         %lld\n", w.sqrtCalls);
    std::printf("  whole-array sorts  %lld\n", w.sorts);
    std::printf("CONTROL -- the same work run a second time\n");
    std::printf("  identical work both runs  %s\n", sameWork ? "yes" : "NO");
    std::printf("  elapsed time reported     no -- it differs on every run; see the source\n");

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    return 0;
}
