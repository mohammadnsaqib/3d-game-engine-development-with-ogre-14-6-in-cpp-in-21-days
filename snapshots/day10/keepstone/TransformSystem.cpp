// Keepstone -- TransformSystem.cpp
//
// Thirty lines, and they are the seam the whole engine is built around.

#include "TransformSystem.h"

#include "keepstone/Renderer.h"

namespace keepstone {

TransformSystem::TransformSystem(World& world, Renderer& renderer)
    : mWorld(world), mRenderer(renderer)
{
}

void TransformSystem::attach(Handle h)
{
    GameObject* o = mWorld.get(h);
    if (!o || !o->transform) return;

    if (o->visual)
    {
        const ObjectId id = mRenderer.addBlock(o->transform->position,
                                               o->transform->scale, o->transform->yaw);
        mNodes[h.index] = id;
    }
    else if (o->glow)
    {
        // A lantern gets a node too, so that moving a light goes through the same one bridge
        // as moving a wall. Two paths for "a thing that has a position" is how they drift.
        const ObjectId id =
            mRenderer.addLantern(o->transform->position, o->glow->light);
        mNodes[h.index] = id;
    }
    o->transform->dirty = false;
}

void TransformSystem::sync()
{
    mPushed = 0;
    mSkipped = 0;

    for (Handle h : mWorld.all())
    {
        GameObject* o = mWorld.get(h);
        if (!o || !o->transform) continue;

        auto it = mNodes.find(h.index);
        if (it == mNodes.end()) continue;      // nothing attached: not this system's business

        // THE SKIP THAT MAKES THIS WORTH DOING. A courtyard is mostly walls, and walls do not
        // move. Pushing every transform every frame costs the same whether anything changed or
        // not; pushing only what moved costs nothing when nothing did.
        if (!o->transform->dirty)
        {
            ++mSkipped;
            continue;
        }

        mRenderer.setPosition(it->second, o->transform->position);
        o->transform->dirty = false;
        ++mPushed;
    }
}

int TransformSystem::attachedCount() const
{
    return static_cast<int>(mNodes.size());
}

}  // namespace keepstone
