// Keepstone -- Loop.h
//
// THE SIMULATION RUNS AT A FIXED RATE AND THE RENDERER DOES NOT. That is the whole of today,
// and it is the decision every other system in this engine is built on top of.
//
// Why fixed: a simulation stepped by however long the last frame took gives a different answer
// on a fast machine than a slow one. The same jump is higher, the same fall is further, and a
// replay recorded on one machine desynchronises on another. Physics stepped by a variable dt is
// not a bug that shows up in testing -- it is a bug that shows up in somebody else's hands.
//
// Why the renderer is not fixed: frames arrive when the display asks for them, which is not the
// same clock and not a multiple of it. The renderer therefore usually wants a moment BETWEEN
// two simulated states, and the fraction of the way between them is the alpha below.

#ifndef KEEPSTONE_LOOP_H
#define KEEPSTONE_LOOP_H

namespace keepstone {

// What the loop drives. The two halves are separate because they run at different rates and
// ask different questions: tick() is told how much time to advance and nothing about drawing;
// render() is told where between two ticks it is and nothing about time.
class Simulation
{
public:
    virtual ~Simulation() {}

    // Always called with the SAME dt. That is the guarantee.
    virtual void tick(float dt) = 0;

    // alpha runs 0..1 between the previous tick's state and the current one. Passing it on to
    // the drawing code is what turns a stack of discrete states back into smooth motion.
    virtual void render(float alpha) = 0;
};

class Loop
{
public:
    explicit Loop(float ticksPerSecond = 50.0f);

    // Advances by `elapsed` seconds of real time, running as many fixed ticks as fit and then
    // rendering once with whatever fraction is left over.
    void advance(Simulation& sim, float elapsed);

    float tickLength() const { return mDt; }
    int   ticksRun() const { return mTicks; }
    int   rendersRun() const { return mRenders; }

    // THE SPIRAL OF DEATH, AND THE ONE LINE THAT PREVENTS IT. If a frame takes longer than the
    // tick, the accumulator grows; the loop then runs more ticks, which takes longer still.
    // Left alone the program stops responding while insisting it is keeping up. The cure is to
    // refuse to run more than this many ticks for one frame and let the simulation fall behind
    // real time honestly, which is visible and survivable.
    void setMaxTicksPerFrame(int n) { mMaxTicks = n; }
    int  clampedFrames() const { return mClamped; }

private:
    float mDt;
    float mAccumulator = 0.0f;
    int   mTicks = 0;
    int   mRenders = 0;
    int   mMaxTicks = 5;
    int   mClamped = 0;
};

}  // namespace keepstone

#endif
