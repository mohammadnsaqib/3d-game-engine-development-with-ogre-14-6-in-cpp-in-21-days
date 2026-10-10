// Keepstone -- Loop.cpp
//
// Twenty lines of arithmetic that decide how the whole engine behaves. No OGRE here either:
// the loop does not know what rendering is, only that something wants to be told when.

#include "Loop.h"

namespace keepstone {

Loop::Loop(float ticksPerSecond)
    : mDt(1.0f / (ticksPerSecond > 0.0f ? ticksPerSecond : 50.0f))
{
}

void Loop::advance(Simulation& sim, float elapsed)
{
    // THE ACCUMULATOR IS THE WHOLE IDEA. Real time arrives in irregular lumps; the accumulator
    // holds the unspent remainder so that no time is lost and none is invented. Over any number
    // of frames the simulation advances by exactly the real time that passed, in whole ticks.
    mAccumulator += elapsed;

    int ran = 0;
    while (mAccumulator >= mDt)
    {
        if (ran >= mMaxTicks)
        {
            // Out of budget for this frame. THROW THE REMAINDER AWAY rather than carry it: the
            // alternative is to owe the next frame even more time, which is the spiral. The
            // simulation now runs slower than the wall clock, and that is the honest outcome --
            // a game in slow motion is playable; a game that has stopped answering is not.
            mAccumulator = 0.0f;
            ++mClamped;
            break;
        }
        sim.tick(mDt);
        mAccumulator -= mDt;
        ++mTicks;
        ++ran;
    }

    // WHAT IS LEFT OVER IS NOT WASTE, IT IS THE ANSWER. The renderer is between two ticks, and
    // this fraction says where. Divide before the cast so the value is 0..1 rather than a
    // number of seconds nobody downstream knows the scale of.
    const float alpha = mAccumulator / mDt;
    sim.render(alpha);
    ++mRenders;
}

}  // namespace keepstone
