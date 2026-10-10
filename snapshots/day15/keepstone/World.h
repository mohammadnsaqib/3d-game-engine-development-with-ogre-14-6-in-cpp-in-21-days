// Keepstone -- World.h  (Day 9)
//
// Day 8's world handed out `GameObject*`. Today it hands out `Handle`, and the raw pointer
// becomes something you borrow for the length of one expression rather than something you keep.
//
// THE RULE, AND IT IS THE WHOLE CHAPTER: store handles, borrow pointers.
// A pointer from `get()` is valid until the next `create()` or `destroy()` and not one line
// longer. A handle is valid forever, and answers honestly once its object is gone.

#ifndef KEEPSTONE_WORLD_H
#define KEEPSTONE_WORLD_H

#include "keepstone/Handle.h"
#include "keepstone/Renderer.h"

#include <string>
#include <vector>

namespace keepstone {

struct Transform
{
    Vec3  position;
    Vec3  scale { 0.3f, 0.5f, 0.3f };
    float yaw = 0.0f;

    // SET BY WHOEVER MOVES THE OBJECT, CLEARED BY THE BRIDGE. Day 10's whole subject.
    //
    // It starts true so that a freshly created object is pushed to the renderer once, without
    // the caller having to remember. The expensive mistake is the other default: an object that
    // is born clean is an object that never appears until something moves it.
    bool dirty = true;

    // Movement goes through here rather than through `position` directly, so that marking and
    // moving cannot come apart. A system that sets position and forgets to set dirty is the
    // defect this chapter photographs.
    void moveTo(Vec3 p) { position = p; dirty = true; }
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

struct GameObject
{
    std::string name;
    bool alive = false;

    Transform* transform = nullptr;
    Visual*    visual    = nullptr;
    Mover*     mover     = nullptr;
    Glow*      glow      = nullptr;
};

class World
{
public:
    World();
    ~World();

    Handle create(const std::string& name);

    // Resolves a handle, or returns nullptr if the object it named is gone. THE NULL IS THE
    // FEATURE: a caller that forgets to check gets a null dereference at the exact line that
    // made the wrong assumption, instead of silently operating on a stranger.
    GameObject* get(Handle h);

    void destroy(Handle h);

    Handle find(const std::string& name);

    std::vector<Handle> all();

    int livingCount() const;
    int slotCount() const;

    // What generation each slot is on, for the chapter to print. A slot on generation 3 has
    // held three different objects.
    std::vector<std::string> describe() const;

private:
    struct Slot
    {
        GameObject*   object = nullptr;
        std::uint32_t generation = 1;   // 1, not 0: a default Handle must be null
    };
    std::vector<Slot> mSlots;
};

}  // namespace keepstone

#endif
