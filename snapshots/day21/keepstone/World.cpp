// Keepstone -- World.cpp  (Day 9)

#include "World.h"

namespace keepstone {

World::World() {}

World::~World()
{
    for (Slot& s : mSlots)
    {
        if (!s.object) continue;
        delete s.object->transform;
        delete s.object->visual;
        delete s.object->mover;
        delete s.object->glow;
        delete s.object;
    }
}

Handle World::create(const std::string& name)
{
    for (std::size_t i = 0; i < mSlots.size(); ++i)
    {
        if (mSlots[i].object && !mSlots[i].object->alive)
        {
            // The slot is reused exactly as it was on Day 8 -- that has not changed and does
            // not need to. What changed is that the slot's generation was bumped when the last
            // tenant died, so every handle to that tenant already stopped resolving.
            mSlots[i].object->name = name;
            mSlots[i].object->alive = true;
            Handle h;
            h.index = static_cast<std::uint32_t>(i);
            h.generation = mSlots[i].generation;
            return h;
        }
    }

    Slot s;
    s.object = new GameObject();
    s.object->name = name;
    s.object->alive = true;
    s.generation = 1;
    mSlots.push_back(s);

    Handle h;
    h.index = static_cast<std::uint32_t>(mSlots.size() - 1);
    h.generation = 1;
    return h;
}

GameObject* World::get(Handle h)
{
    if (h.isNull()) return nullptr;
    if (h.index >= mSlots.size()) return nullptr;

    Slot& s = mSlots[h.index];

    // THE ONE COMPARISON THAT DOES ALL THE WORK. Two integers, no memory read, no guessing at
    // whether an allocation is still live. If the slot has moved on, this handle named somebody
    // who is gone, and saying so is the entire job.
    if (s.generation != h.generation) return nullptr;
    if (!s.object || !s.object->alive) return nullptr;
    return s.object;
}

void World::destroy(Handle h)
{
    GameObject* o = get(h);
    if (!o) return;

    delete o->transform; o->transform = nullptr;
    delete o->visual;    o->visual    = nullptr;
    delete o->mover;     o->mover     = nullptr;
    delete o->glow;      o->glow      = nullptr;

    o->alive = false;
    o->name = "(dead)";

    // AND THIS IS THE LINE. Every handle anybody is still holding to this object now carries a
    // generation the slot has left behind, so every one of them resolves to nullptr from here
    // on -- without the world knowing how many there are or where they live.
    ++mSlots[h.index].generation;
}

Handle World::find(const std::string& name)
{
    for (std::size_t i = 0; i < mSlots.size(); ++i)
    {
        if (mSlots[i].object && mSlots[i].object->alive && mSlots[i].object->name == name)
        {
            Handle h;
            h.index = static_cast<std::uint32_t>(i);
            h.generation = mSlots[i].generation;
            return h;
        }
    }
    return Handle();
}

std::vector<Handle> World::all()
{
    std::vector<Handle> out;
    for (std::size_t i = 0; i < mSlots.size(); ++i)
    {
        if (mSlots[i].object && mSlots[i].object->alive)
        {
            Handle h;
            h.index = static_cast<std::uint32_t>(i);
            h.generation = mSlots[i].generation;
            out.push_back(h);
        }
    }
    return out;
}

int World::livingCount() const
{
    int n = 0;
    for (const Slot& s : mSlots) if (s.object && s.object->alive) ++n;
    return n;
}

int World::slotCount() const { return static_cast<int>(mSlots.size()); }

std::vector<std::string> World::describe() const
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < mSlots.size(); ++i)
    {
        const Slot& s = mSlots[i];
        out.push_back("  slot " + std::to_string(i) +
                      "  generation " + std::to_string(s.generation) +
                      "  " + (s.object && s.object->alive ? "live " : "dead ") +
                      (s.object ? s.object->name : "(empty)"));
    }
    return out;
}

}  // namespace keepstone
