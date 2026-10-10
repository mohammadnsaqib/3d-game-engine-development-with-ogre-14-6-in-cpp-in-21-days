// Keepstone -- World.cpp
//
// No OGRE here either. The world does not know how anything is drawn; Day 10 builds the one
// system that carries a Transform across to the renderer, and it is the only place the two
// sides meet.

#include "World.h"

#include <algorithm>

namespace keepstone {

bool GameObject::has(const char* what) const
{
    const std::string w(what);
    if (w == "transform") return transform != nullptr;
    if (w == "visual")    return visual != nullptr;
    if (w == "mover")     return mover != nullptr;
    if (w == "glow")      return glow != nullptr;
    return false;
}

World::World() {}

World::~World()
{
    for (GameObject* o : mSlots)
    {
        delete o->transform;
        delete o->visual;
        delete o->mover;
        delete o->glow;
        delete o;
    }
}

GameObject* World::create(const std::string& name)
{
    // REUSE A DEAD SLOT IF THERE IS ONE. This is the ordinary, sensible thing to do -- it keeps
    // the vector from growing without bound as objects come and go -- and it is precisely what
    // makes a stale pointer dangerous rather than merely wrong. The memory is valid, the object
    // is alive, and it is not the one the caller remembers.
    for (GameObject* o : mSlots)
    {
        if (!o->alive)
        {
            o->name = name;
            o->alive = true;
            return o;
        }
    }

    GameObject* o = new GameObject();
    o->name = name;
    o->alive = true;
    mSlots.push_back(o);
    return o;
}

void World::destroy(GameObject* object)
{
    if (!object || !object->alive) return;

    delete object->transform; object->transform = nullptr;
    delete object->visual;    object->visual    = nullptr;
    delete object->mover;     object->mover     = nullptr;
    delete object->glow;      object->glow      = nullptr;

    object->alive = false;
    object->name = "(dead)";
    // AND THE CALLER STILL HOLDS THE POINTER. Nothing here can reach into the caller and blank
    // it; nothing here even knows how many copies of it exist. That is the hole Day 9 closes.
}

GameObject* World::find(const std::string& name)
{
    for (GameObject* o : mSlots)
        if (o->alive && o->name == name) return o;
    return nullptr;
}

std::vector<GameObject*> World::all()
{
    std::vector<GameObject*> out;
    for (GameObject* o : mSlots)
        if (o->alive) out.push_back(o);
    return out;
}

int World::livingCount() const
{
    int n = 0;
    for (const GameObject* o : mSlots) if (o->alive) ++n;
    return n;
}

int World::slotCount() const { return static_cast<int>(mSlots.size()); }

std::vector<std::string> World::describe() const
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < mSlots.size(); ++i)
    {
        const GameObject* o = mSlots[i];
        std::string parts;
        if (o->transform) parts += " transform";
        if (o->visual)    parts += " visual";
        if (o->mover)     parts += " mover";
        if (o->glow)      parts += " glow";
        if (parts.empty()) parts = " (none)";
        out.push_back("  slot " + std::to_string(i) + "  " +
                      (o->alive ? "live " : "dead ") + o->name + " :" + parts);
    }
    return out;
}

}  // namespace keepstone
