// Keepstone -- Collision.h
//
// WHAT THIS IS, AND MORE IMPORTANTLY WHAT IT IS NOT.
//
// This is collision: boxes, rays, and a swept test that answers "did this thing pass through
// that thing between where it was and where it wants to be". It is what a game needs to stop a
// character at a wall, to ask what is under the cursor, and to know when two things touched.
//
// IT IS NOT A PHYSICS SOLVER. There are no rigid bodies here, no mass, no restitution,
// no constraints, no stacking, no joints, and no attempt at any of them. Those are a
// solver, a solver is a large and subtle piece of work, and a bad one written in half a
// day is worse than none -- it looks like it works until something rests on something
// else.
//
// **OGRE 14.6 ALREADY SHIPS THE ANSWER FOR THAT HALF.** The optional Bullet component
// (`OGRE_BUILD_COMPONENT_BULLET`, on by default when Bullet is found, and built into
// the SDK this book uses) puts a real solver behind `Ogre::Bullet::DynamicsWorld`.
// Reach for it when the keep needs barrels that roll. Today's job is the half a game
// needs even when nothing
// rolls: knowing where the walls are.
//
// THE DELIBERATE DEFECT. `moveDiscrete` below moves first and checks afterwards, which is what
// almost everybody writes first. It works perfectly until something moves further in one tick
// than the wall is thick, and then it walks straight through. Day 14 photographs that before
// fixing it with `moveSwept`.

#ifndef KEEPSTONE_COLLISION_H
#define KEEPSTONE_COLLISION_H

#include "keepstone/Renderer.h"

#include <string>
#include <vector>

namespace keepstone {

// An axis-aligned box, stored as centre and half-extents. Half-extents rather than min/max
// because every test below wants them, and converting on every test is how a hot loop gets slow
// for no reason anybody can point at.
struct Box
{
    Vec3 centre;
    Vec3 half { 0.5f, 0.5f, 0.5f };

    float minX() const { return centre.x - half.x; }
    float maxX() const { return centre.x + half.x; }
    float minY() const { return centre.y - half.y; }
    float maxY() const { return centre.y + half.y; }
    float minZ() const { return centre.z - half.z; }
    float maxZ() const { return centre.z + half.z; }
};

struct Hit
{
    bool  hit = false;
    float distance = 0.0f;     // along the ray, or the fraction 0..1 along a sweep
    Vec3  point;
    int   which = -1;          // index of the box that was hit
};

class Collision
{
public:
    int  add(const Box& box, const std::string& name);
    void clear();

    int boxCount() const { return static_cast<int>(mBoxes.size()); }
    const Box& box(int i) const { return mBoxes[i]; }
    const std::string& boxName(int i) const { return mNames[i]; }

    static bool overlaps(const Box& a, const Box& b);

    // A ray against every box, nearest hit wins. Linear, and honest about it: a keep with a few
    // hundred boxes does not need a tree, and a tree written before it is needed is a tree whose
    // bugs arrive before its benefits.
    Hit raycast(Vec3 from, Vec3 direction, float maxDistance) const;

    // MOVE THEN CHECK. The version almost everybody writes first, kept because it is the one
    // the reader will have written too.
    Vec3 moveDiscrete(const Box& mover, Vec3 delta, bool* blocked) const;

    // CHECK THE PATH, THEN MOVE. Sweeps the mover's box along delta and stops at the first
    // thing in the way, so speed cannot tunnel through thickness.
    Vec3 moveSwept(const Box& mover, Vec3 delta, bool* blocked, float* toi) const;

private:
    std::vector<Box> mBoxes;
    std::vector<std::string> mNames;
};

}  // namespace keepstone

#endif
