// The Lantern Keep -- Day 11.
//
// The courtyard has never loaded anything. Every texture it has used was already on disk where
// OGRE could find it, and every block has been a colour. A real keep has a few hundred assets,
// and the moment they arrive decides whether the game stutters.
//
// THE TWO SCENES THAT MATTER ARE `hitch` AND `spread`. They load the same manifest, the same
// bytes, off the same disk. One does it in a single frame and one does it a budget at a time,
// and the difference is the whole day.
//
// The bars at the bottom of each frame are Day 7's profiler, unchanged.

#include "keepstone/Assets.h"
#include "keepstone/Engine.h"
#include "keepstone/Handle.h"
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
    "hitch",         // the whole manifest loaded inside one frame
    "spread",        // the same manifest, 2 ms of loading a frame
    "budget_tight",  // half a millisecond a frame: smooth, and slow to finish
    "budget_loose",  // eight milliseconds a frame: quick to finish, and it shows
    "partial",       // stopped halfway: what a half-loaded world looks like
    "preloaded",     // everything loaded before the first frame, as a loading screen would
    "nothing",       // nothing requested at all: the baseline cost of the frame itself
    "many",          // a bigger courtyard, spread loading
    "few",           // a smaller one
};

// The book's own texture set, declared many times over under different names. A manifest of two
// hundred entries over fourteen files is not a cheat -- it is what a real keep looks like, where
// the same stone is used by forty walls under forty names.
const char* FILES[] = {
    "blockwork.png", "blockwork_nm.png", "cloth.png", "crypt.png", "flagstone.png",
    "marker.png", "sky_bk.png", "sky_dn.png", "sky_fr.png", "sky_lf.png",
    "sky_rt.png", "sky_up.png", "spark.png", "timber.png",
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "spread";
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
    keepstone::Assets assets;

    keepstone::Engine engine;
    engine.add(&renderer);
    engine.add(&assets);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    const int manifestSize = 200;
    for (int i = 0; i < manifestSize; ++i)
        assets.declare("asset" + std::to_string(i), FILES[i % 14]);

    keepstone::Profiler prof;               // 16.67 ms, the 60 Hz budget
    const int frames = 40;

    double pumpBudget = 2.0;
    if (scene == "budget_tight") pumpBudget = 0.5;
    if (scene == "budget_loose") pumpBudget = 8.0;

    if (scene == "preloaded")
    {
        // Outside the measured frames entirely: this is the loading screen, and a loading
        // screen is allowed to take as long as it takes.
        assets.loadAllNow();
    }
    else if (scene != "nothing")
    {
        for (int i = 0; i < manifestSize; ++i)
            assets.request("asset" + std::to_string(i));
    }

    const int stopAfter = (scene == "partial") ? 6 : frames;

    // CLOSE THE BOOKS ON EVERYTHING THAT HAPPENED BEFORE FRAME ZERO.
    //
    // `preloaded` calls loadAllNow() above, outside the measured frames, which is the whole
    // point of a loading screen. Without this line those two hundred loads are still sitting in
    // the counter when the first frame ends, and `busiest frame` reports the loading screen's
    // work as though one frame had done it -- turning the chapter's best-behaved scene into a
    // picture of its worst fault. Found by reading the capture rather than the code.
    //
    // markFrame() is the wrong call here and was the first thing tried: it RECORDS the work
    // it is meant to skip. startMeasuring() resets the baseline without scoring it.
    assets.startMeasuring();

    for (int f = 0; f < frames; ++f)
    {
        prof.beginFrame();

        prof.begin("simulation");
        // A little ordinary work, so the frame is not empty and the bars have a floor.
        volatile double x = 0.0;
        for (int k = 0; k < 40000; ++k) x += k * 0.5;
        (void)x;
        prof.end();

        if (f < stopAfter)
        {
            prof.begin("assets");
            if (scene == "hitch" && f == 8)
            {
                // THE MISTAKE, ON ONE FRAME. Not a slow game -- a smooth game with one frame in
                // it that takes as long as the whole manifest, at the moment the door opened.
                assets.loadAllNow();
            }
            else if (scene != "hitch" && scene != "preloaded" && scene != "nothing")
            {
                assets.pump(pumpBudget);
            }
            prof.end();
        }

        prof.endFrame();
        // Close the frame for the asset service too, so it can report the busiest one. This is
        // the count that explains the hitch, and unlike a millisecond it is the same everywhere.
        assets.markFrame();
    }

    // -- the courtyard ------------------------------------------------------------------------
    renderer.setAmbient({0.22f, 0.22f, 0.27f});
    renderer.setCamera({0.0f, 150.0f, 430.0f}, {0.0f, 40.0f, 0.0f});
    renderer.addLantern({170.0f, 190.0f, 210.0f}, {1.0f, 0.88f, 0.66f});

    const int walls = (scene == "many") ? 16 : (scene == "few") ? 4 : 9;
    for (int i = 0; i < walls; ++i)
        renderer.addBlock({ -150.0f + (300.0f / walls) * i, 40.0f, -70.0f },
                          { 0.20f, 0.42f + 0.09f * (i % 3), 0.20f }, 0.0f);

    // A row of markers showing how much of the manifest arrived: one block per ten assets.
    const int arrived = assets.loadedCount() / 10;
    for (int i = 0; i < arrived; ++i)
        renderer.addBlock({ -200.0f + 21.0f * i, 120.0f, 60.0f }, {0.07f, 0.07f, 0.07f}, 0.0f);

    // Day 7's bars, unchanged.
    const std::vector<keepstone::Profiler::Entry> es = prof.entries();
    for (std::size_t i = 0; i < es.size(); ++i)
    {
        const double share = es[i].meanMs() / prof.budgetMs();
        const float width = static_cast<float>(share) * 2.4f;
        renderer.addBlock({-280.0f + width * 100.0f, -40.0f - 34.0f * i, 40.0f},
                          {width, 0.10f, 0.22f}, 0.0f);
    }
    renderer.addBlock({-280.0f + 240.0f, -57.0f, 40.0f}, {0.035f, 0.42f, 0.24f}, 0.0f);

    // THE WORST FRAME, DRAWN AS ITS OWN BAR, ABOVE THE MEANS. This is the bar that tells the
    // two arrangements apart: the means are close enough to be indistinguishable on the page,
    // and the worst frames are not. A profiler that draws only averages would have rendered
    // `hitch` and `spread` as the same picture -- which is exactly the mistake the chapter is
    // about, made by the instrument instead of by the loader.
    {
        const double share = prof.worstFrameMs() / prof.budgetMs();
        const float width = static_cast<float>(share) * 2.4f;
        renderer.addBlock({-280.0f + width * 100.0f, 6.0f, 40.0f},
                          {width, 0.13f, 0.26f}, 0.0f);
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("manifest         %d entries\n", manifestSize);
    std::printf("loaded           %d\n", assets.loadedCount());
    std::printf("still pending    %d\n", assets.pendingCount());
    std::printf("bytes read       %zu\n", assets.bytesLoaded());
    std::printf("frames           %d\n", prof.frames());
    std::printf("frames over      %d\n", prof.framesOverBudget());
    // THE PERCENTAGES ARE NOT PRINTED, AND THAT IS THE WHOLE FIX.
    //
    // This scene measures reading files off a disk, and how long that takes is a fact about the
    // machine at that moment rather than about the loader. The same binary, run three times in a
    // row here, reported the worst frame at 311%, 324% and 347% of budget -- and once, while
    // twenty-one builds and captures were running back to back, at 1355%. Nothing was wrong on
    // any of those runs. That is simply what timing a disk is.
    //
    // A book cannot print a number like that. The chapter quoted 304.2% for months, the capture
    // then held 1355.2%, and the two disagreed with nobody at fault. So this reports what does
    // NOT move:
    //
    //   frames over   how many frames missed their deadline -- 1 for `hitch`, 0 for `spread`,
    //                 which IS this chapter's argument,
    //   bytes read    identical on every run, because it is a property of the manifest,
    //   worst frame   as a RATIO to the mean frame of the same run, so the machine's mood
    //                 divides out: a hitch is a frame many times its neighbours, whatever the
    //                 clock says that day.
    //
    // Day 20 makes the general case -- count the work, then time it. This is the same decision
    // taken nine days earlier, and the two chapters now agree.
    // A RATIO IS STEADIER THAN A PERCENTAGE AND STILL NOT STEADY ENOUGH TO PRINT. Measured here:
    // the worst frame came out 36 times the mean on three runs in a row and 34 on a fourth. That
    // is a tenth of the old swing and still enough to make a quoted number wrong later, so what
    // gets printed is the THRESHOLD it crosses, which does not move, and the mechanism behind it,
    // which is a count and cannot move at all.
    const double ratio = prof.meanFrameMs() > 0.0
                       ? prof.worstFrameMs() / prof.meanFrameMs() : 0.0;
    std::printf("worst frame      %s the mean frame of the same run\n",
                ratio >= 10.0 ? "more than 10x" : ratio >= 3.0 ? "3x to 10x" : "under 3x");
    // THE CAUSE, AS A COUNT. One frame did all two hundred loads in `hitch`; in `spread` no
    // frame does more than a handful. That is the whole chapter.
    //
    // WHY `spread` IS REPORTED AS A BAND AND `hitch` AS A NUMBER. `pump()` loads until a TIME
    // budget is spent, so how many it gets through in one frame depends on how fast the disk is
    // that morning -- measured here at 12, 13 and 13 on three consecutive runs. `hitch` calls
    // `loadAllNow()`, which has no budget and always does the lot, so its number is exact on
    // every machine. Printing an exact 13 would put a number in the book that a reader's own run
    // would contradict, for a reason that is not a defect. The band says the true thing: nothing
    // like the whole manifest landed in one frame.
    const int worst = assets.mostLoadedInOneFrame();
    if (worst * 2 >= manifestSize)
        std::printf("busiest frame    %d of %d asset(s) -- the whole manifest, in one frame\n",
                    worst, manifestSize);
    else
        std::printf("busiest frame    under %d%% of the manifest in any one frame\n",
                    ((worst * 100 / manifestSize) / 10 + 1) * 10);
    std::printf("verdict          %s\n", prof.withinBudget() ? "EVERY FRAME FIT"
                                                             : "SOME FRAMES DID NOT FIT");
    for (const keepstone::Profiler::Entry& e : es)
        std::printf("  %-12s %5d calls\n", e.name.c_str(), e.calls);

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
