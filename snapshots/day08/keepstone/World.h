// Keepstone -- World.h
//
// A GAME OBJECT IS NOT AN ENTITY, and today is the day that distinction gets a type.
//
// `Ogre::Entity` is one instance of a mesh. It has geometry, a material and a place to be drawn,
// and that is all it has. It carries no identity in the world, no behaviour, no health, no name
// anyone would recognise. A lantern is a game object; it OWNS an entity, the way it owns a
// position and might own a flicker.
//
// The engine has been able to draw since Day 1 and has had nothing to draw ABOUT until now.
//
// COMPOSITION, NOT INHERITANCE. The obvious first design is a base class with virtual methods
// and a family tree beneath it: Thing -> MovingThing -> GlowingMovingThing. It works until the
// day something needs to glow and not move, and then the tree has to be rebuilt. A game object
// here is an identity plus whichever components it happens to have, and adding a kind of object
// means picking a different set, not adding a class.
//
// ⚠ THIS FILE USES RAW POINTERS ON PURPOSE, AND DAY 9 BREAKS THEM. `destroy()` hands back a
// slot that a later `create()` will reuse, so a pointer kept across a destroy does not dangle
// into freed memory -- it quietly starts naming a DIFFERENT object. That is worse than a crash,
// it is deterministic, and Day 9 photographs it before fixing it with handles. Do not copy this
// pattern out of this chapter.

#ifndef KEEPSTONE_WORLD_H
#define KEEPSTONE_WORLD_H

#include "keepstone/Renderer.h"

#include <string>
#include <vector>

namespace keepstone {

// -- the components ----------------------------------------------------------------------
// Data, not behaviour. A component knows what it is, never what to do about it: the systems on
// Day 10 and after are where anything happens. Keeping them dumb is what lets two unrelated
// systems read the same component without either knowing the other exists.

struct Transform
{
    Vec3  position;
    Vec3  scale { 0.3f, 0.5f, 0.3f };
    float yaw = 0.0f;
};

struct Visual
{
    Colour tint { 1.0f, 1.0f, 1.0f };
    bool   drawn = true;
};

struct Mover
{
    Vec3 velocity;
};

struct Glow
{
    Colour light { 1.0f, 0.88f, 0.66f };
};

// -- the game object ---------------------------------------------------------------------
// An identity and a set of optional parts. A null pointer here is not a mistake, it is the
// whole design: a wall has a Transform and a Visual and no Mover, and that is what makes it a
// wall rather than a subclass called Wall.
struct GameObject
{
    std::string name;
    bool alive = false;

    Transform* transform = nullptr;
    Visual*    visual    = nullptr;
    Mover*     mover     = nullptr;
    Glow*      glow      = nullptr;

    bool has(const char* what) const;
};

class World
{
public:
    World();
    ~World();

    // Returns a raw pointer, which is the defect Day 9 exists to fix.
    GameObject* create(const std::string& name);

    // Marks the slot dead and frees its components. THE SLOT IS REUSED by the next create(),
    // which is exactly how a stale pointer starts naming somebody else.
    void destroy(GameObject* object);

    GameObject* find(const std::string& name);

    // Every living object, in creation order.
    std::vector<GameObject*> all();

    int livingCount() const;
    int slotCount() const;

    // A one-line description of every slot, living or dead, for the chapter to print.
    std::vector<std::string> describe() const;

private:
    std::vector<GameObject*> mSlots;
};

}  // namespace keepstone

#endif
