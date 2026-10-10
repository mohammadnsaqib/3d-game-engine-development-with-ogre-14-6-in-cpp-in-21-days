// The Lantern Keep -- Day 6.
//
// A still picture cannot show jitter, so the warden leaves a trail. Every time the loop renders,
// the game drops a marker where the warden appears to be at that instant; the finished frame is
// therefore a record of every position the renderer was ever asked to draw.
//
// Evenly spaced markers mean the renderer was given a position for the moment it was actually
// drawing. Markers clumped in pairs and threes mean it was given the last tick's position and
// the same one twice -- which on a moving screen is the stutter this day exists to remove.

#include "keepstone/Engine.h"
#include "keepstone/Loop.h"
#include "keepstone/Renderer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "snapped",        // alpha ignored: the renderer is handed the last tick's position
    "interpolated",   // alpha used: the renderer is handed where the warden is NOW
    "slow_render",    // the same walk rendered half as often
    "fast_render",    // the same walk rendered twice as often
    "coarse_tick",    // a 10 Hz simulation, interpolated
    "fine_tick",      // a 100 Hz simulation, interpolated
    "uneven",         // frames of wildly uneven length, interpolated
    "spiral",         // frames longer than the tick, with the clamp ON
    "unclamped",      // the same, clamp raised: the tick count runs away
};

// The warden walks the wall at a constant speed. TWO POSITIONS ARE KEPT, not one: interpolation
// needs where it was as well as where it is, and a simulation that only stores `now` has thrown
// away the information the renderer needs.
class Warden : public keepstone::Simulation
{
public:
    Warden(keepstone::Renderer& r, bool interpolate)
        : mRenderer(r), mInterpolate(interpolate) {}

    void tick(float dt) override
    {
        mPrevX = mX;
        mX += mSpeed * dt;
    }

    void render(float alpha) override
    {
        // The whole difference between the two headline frames is this line.
        const float x = mInterpolate ? (mPrevX + (mX - mPrevX) * alpha) : mX;
        mSampled.push_back(x);
    }

    const std::vector<float>& sampled() const { return mSampled; }
    float x() const { return mX; }

private:
    keepstone::Renderer& mRenderer;
    bool  mInterpolate;
    float mX = -300.0f;
    float mPrevX = -300.0f;
    float mSpeed = 260.0f;          // units a second
    std::vector<float> mSampled;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "interpolated";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    const bool interpolate = (scene != "snapped");
    float tickRate = 50.0f;
    if (scene == "coarse_tick") tickRate = 10.0f;
    if (scene == "fine_tick")   tickRate = 100.0f;

    // Frame lengths in seconds. Deliberately NOT a multiple of the tick: a renderer that lands
    // exactly on tick boundaries makes interpolation look pointless, which is the mistake that
    // gets it left out.
    float frame = 0.0233f;
    if (scene == "slow_render") frame = 0.0466f;
    if (scene == "fast_render") frame = 0.0117f;
    if (scene == "spiral" || scene == "unclamped") frame = 0.140f;

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp())
    {
        std::cout << "engine did not come up\n";
        return 1;
    }

    renderer.setAmbient({0.22f, 0.22f, 0.27f});
    renderer.setCamera({0.0f, 150.0f, 430.0f}, {0.0f, 40.0f, 0.0f});
    renderer.addLantern({160.0f, 200.0f, 220.0f}, {1.0f, 0.88f, 0.66f});

    keepstone::Loop loop(tickRate);
    if (scene == "unclamped") loop.setMaxTicksPerFrame(1000);

    Warden warden(renderer, interpolate);

    for (int f = 0; f < 24; ++f)
    {
        // `uneven` alternates short and long frames, which is what a real machine does when
        // another program wakes up.
        const float thisFrame = (scene == "uneven" && (f % 2)) ? frame * 3.0f : frame;
        loop.advance(warden, thisFrame);
    }

    // The trail: one marker per render, at the position the renderer was given.
    for (float x : warden.sampled())
        renderer.addBlock({x, 30.0f, 0.0f}, {0.055f, 0.30f, 0.055f}, 0.0f);

    // The wall behind it, so the frame is a courtyard and not a row of dots.
    for (int i = 0; i < 5; ++i)
        renderer.addBlock({-280.0f + 140.0f * i, 46.0f, -150.0f}, {0.30f, 0.62f, 0.30f}, 0.0f);

    std::cout << "scene          " << scene << "\n";
    std::cout << "tick rate      " << static_cast<int>(tickRate) << " Hz\n";
    std::cout << "ticks run      " << loop.ticksRun() << "\n";
    std::cout << "renders run    " << loop.rendersRun() << "\n";
    std::cout << "markers        " << warden.sampled().size() << "\n";
    std::cout << "frames clamped " << loop.clampedFrames() << "\n";

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::cout << "wrote " << out << "\n";

    engine.shutDown();
    std::cout.flush();
    std::_Exit(0);
}
