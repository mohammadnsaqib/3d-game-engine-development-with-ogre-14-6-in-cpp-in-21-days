// Keepstone -- TransformSystem.h
//
// THE ONE PLACE THE TWO WORLDS TOUCH.
//
// The world has game objects with `Transform` components. The renderer has scene nodes. They
// are separate on purpose -- the world can be ticked, saved and tested with no renderer in the
// room -- and something has to carry a position from one to the other. This is that something,
// and it is the ONLY code in Keepstone allowed to do it.
//
// Keeping it to one file is what makes the separation real. The moment a second place also
// pushes transforms across, "where did this object's position come from" stops having one
// answer, and the two paths disagree the first time one of them is changed.
//
// WHAT IT IS NOT: a renderer, and not a place for gameplay. It reads transforms, writes node
// positions, and has no opinion about either.

#ifndef KEEPSTONE_TRANSFORMSYSTEM_H
#define KEEPSTONE_TRANSFORMSYSTEM_H

#include "keepstone/Handle.h"
#include "keepstone/World.h"

#include <unordered_map>

namespace keepstone {

class Renderer;

class TransformSystem
{
public:
    TransformSystem(World& world, Renderer& renderer);

    // Gives an object a node in the renderer and remembers the pairing. Called once per object,
    // not once per frame: the whole point of a bridge is that the scene is BUILT once and then
    // kept, instead of being thrown away and rebuilt every time anything moves.
    void attach(Handle h);

    // Pushes changed transforms across. Objects whose transform has not been touched since the
    // last call are skipped, which is what the dirty flag is for.
    void sync();

    int attachedCount() const;
    int pushedLastSync() const { return mPushed; }
    int skippedLastSync() const { return mSkipped; }

    // ⚠ THE DELIBERATE DEFECT, and Day 10 photographs it. With this false, `sync()` still runs
    // and still skips every clean transform -- but nothing ever marks a transform dirty, so
    // every transform is clean forever and nothing is ever pushed. The objects move in the
    // world and stand still on the screen.
    //
    // It is left switchable rather than removed because the failure is worth seeing: the world
    // is right, the renderer is right, the bridge runs, and the picture is wrong.
    void setMarkingEnabled(bool on) { mMarking = on; }
    bool markingEnabled() const { return mMarking; }

private:
    World&    mWorld;
    Renderer& mRenderer;
    std::unordered_map<std::uint32_t, ObjectId> mNodes;   // slot index -> renderer object
    bool mMarking = true;
    int  mPushed = 0;
    int  mSkipped = 0;
};

}  // namespace keepstone

#endif
