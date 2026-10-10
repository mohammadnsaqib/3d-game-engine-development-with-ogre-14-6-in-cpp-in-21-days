// The Lantern Keep -- Day 7.
//
// The budget drawn INTO the frame. A profiler that only prints to a console is a profiler you
// read after the interesting moment has passed; one that draws itself is a profiler you watch.
//
// The bars along the bottom of every frame today are the engine reporting on itself: one bar a
// section, its length the share of the frame budget that section spent. The tall marker on the
// right is the budget itself, so a bar reaching it is a frame that did not fit.

#include "keepstone/Engine.h"
#include "keepstone/Loop.h"
#include "keepstone/Profiler.h"
#include "keepstone/Renderer.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "budget",        // the courtyard with the profiler's bars drawn under it
    "over",          // deliberate extra work until the frame stops fitting
    "heavy_sim",     // the simulation made expensive; watch which bar grows
    "heavy_build",   // the scene build made expensive instead
    "light",         // very little work: the bars nearly vanish
    "many_blocks",   // 200 blocks, so the render section has something to do
    "few_blocks",    // 12 blocks, the same scene cheaply
    "long_run",      // 120 frames instead of 24: the mean settles
    "short_run",     // 6 frames: the mean is still noisy, and the book says so
};

void burn(int microseconds)
{
    // A deliberate waste of exactly the time asked for. Not a sleep: a sleep yields the core
    // and measures the scheduler rather than the work, which is the wrong thing to put under a
    // profiler.
    //
    // AND NOT A FIXED ITERATION COUNT, which is what this was first written as. `for (i < us *
    // 40)` is a guess at how much work a microsecond is worth, and the guess was wrong by two
    // orders of magnitude on this machine: the scene named `over` -- whose entire job is to
    // exceed the frame budget -- reported 7.6% of budget and "EVERY FRAME FIT". A demonstration
    // that contradicts its own name is worse than no demonstration, and no gate would catch it,
    // because every number printed was true. Spin on the clock and the name stays honest on any
    // machine.
    const auto start = std::chrono::steady_clock::now();
    const auto want = std::chrono::microseconds(microseconds);
    volatile double x = 0.0;
    while (std::chrono::steady_clock::now() - start < want) x += 0.5;
    (void)x;
}

class Walk : public keepstone::Simulation
{
public:
    Walk(keepstone::Profiler& p, int simCost) : mProf(p), mSimCost(simCost) {}

    void tick(float dt) override
    {
        mProf.begin("simulation");
        mX += 240.0f * dt;
        burn(mSimCost);
        mProf.end();
    }

    void render(float) override {}

    float x() const { return mX; }

private:
    keepstone::Profiler& mProf;
    int   mSimCost;
    float mX = -300.0f;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "budget";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    int simCost = 900, buildCost = 1800, frames = 24, blocks = 40;
    if (scene == "over")        { simCost = 8000; buildCost = 10500; }
    if (scene == "heavy_sim")   { simCost = 4200; }
    if (scene == "heavy_build") { buildCost = 9000; }
    if (scene == "light")       { simCost = 60; buildCost = 60; }
    if (scene == "many_blocks") { blocks = 200; }
    if (scene == "few_blocks")  { blocks = 12; }
    if (scene == "long_run")    { frames = 120; }
    if (scene == "short_run")   { frames = 6; }

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    keepstone::Profiler prof;            // 16.67 ms, the 60 Hz budget
    keepstone::Loop loop(50.0f);
    Walk walk(prof, simCost);

    for (int f = 0; f < frames; ++f)
    {
        prof.beginFrame();
        loop.advance(walk, 0.0166f);
        prof.begin("scene build");
        burn(buildCost);
        prof.end();
        prof.endFrame();
    }

    renderer.setAmbient({0.22f, 0.22f, 0.27f});
    renderer.setCamera({0.0f, 150.0f, 430.0f}, {0.0f, 30.0f, 0.0f});
    renderer.addLantern({160.0f, 210.0f, 230.0f}, {1.0f, 0.88f, 0.66f});

    for (int i = 0; i < blocks; ++i)
    {
        const float x = -320.0f + (640.0f / blocks) * i;
        renderer.addBlock({x, 70.0f, -160.0f}, {0.10f, 0.42f, 0.10f}, 0.0f);
    }

    // -- THE INSTRUMENT, DRAWN. One bar per section, scaled against the budget. --------------
    const std::vector<keepstone::Profiler::Entry> es = prof.entries();
    for (std::size_t i = 0; i < es.size(); ++i)
    {
        const double share = es[i].meanMs() / prof.budgetMs();       // 0..1 of the budget
        const float width = static_cast<float>(share) * 2.4f;        // 1.0 budget -> 2.4 units
        renderer.addBlock({-280.0f + width * 100.0f, -40.0f - 34.0f * i, 40.0f},
                          {width, 0.10f, 0.22f}, 0.0f);
    }
    // The budget marker: where a bar reaching all the way would end.
    renderer.addBlock({-280.0f + 240.0f, -57.0f, 40.0f}, {0.035f, 0.42f, 0.24f}, 0.0f);

    // HOW MANY FRAMES THE MEAN WAS TAKEN OVER, one marker per six.
    //
    // Without this, `budget` (24 frames), `long_run` (120) and `short_run` (6) rendered
    // BYTE-IDENTICAL: the bars are drawn from the mean, the mean barely moves with run length,
    // and the sample size -- the entire point of those two scenes -- appeared nowhere. Nine
    // published frames were showing seven pictures, which is seven pieces of evidence wearing
    // nine captions, and no gate could see it: `check_book.py` counts frame FILES.
    //
    // A figure has to carry the thing its scene varies.
    for (int i = 0; i < prof.frames() / 6; ++i)
        renderer.addBlock({ -300.0f + 11.0f * static_cast<float>(i), 128.0f, 60.0f },
                          { 0.035f, 0.10f, 0.10f }, 0.0f);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("budget           %.2f ms per frame (60 Hz)\n", prof.budgetMs());
    std::printf("frames           %d\n", prof.frames());
    std::printf("frames over      %d\n", prof.framesOverBudget());
    std::printf("verdict          %s\n", prof.withinBudget() ? "EVERY FRAME FIT"
                                                             : "SOME FRAMES DID NOT FIT");
    for (const keepstone::Profiler::Entry& e : es)
        std::printf("  %-14s %5d calls, %5.1f%% of budget (mean)\n",
                    e.name.c_str(), e.calls, 100.0 * e.meanMs() / prof.budgetMs());

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
