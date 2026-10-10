// Keepstone -- Handle.h
//
// THE BUG DAY 8 LEFT ON PURPOSE, AND THE TWELVE BYTES THAT CLOSE IT.
//
// Day 8's World hands back a `GameObject*`. Keep one, destroy the object, create another, and
// the world reuses the slot -- so the pointer you are still holding is not dangling into freed
// memory. It is pointing at a perfectly valid, perfectly alive object that is SOMEBODY ELSE.
//
// That is worse than a crash in every way that matters. A crash stops at the mistake. This
// keeps running, and the game moves the wrong thing, lights the wrong thing, or hurts the wrong
// thing, and the symptom shows up somewhere with no visible connection to the destroy that
// caused it. It is also completely deterministic, which means it will reproduce on a reader's
// machine exactly as it does here.
//
// A HANDLE IS AN INDEX PLUS A GENERATION. The index says which slot; the generation says which
// TENANT of that slot. Destroying an object bumps the slot's generation, so every handle to the
// old tenant stops resolving -- not by luck, and not by reading freed memory to find out, but
// by comparing two integers the world still owns.
//
// Cheap enough to have no argument against it: eight bytes, one comparison on lookup, and no
// allocation ever.

#ifndef KEEPSTONE_HANDLE_H
#define KEEPSTONE_HANDLE_H

#include <cstdint>

namespace keepstone {

struct Handle
{
    std::uint32_t index = 0;
    // GENERATION 0 MEANS "NOTHING". Slots start their life at generation 1, so a
    // default-constructed Handle is null without anyone having to remember to set it -- which
    // matters because the commonest way to hold a bad handle is to never have set a good one.
    std::uint32_t generation = 0;

    bool isNull() const { return generation == 0; }

    bool operator==(const Handle& o) const
    {
        return index == o.index && generation == o.generation;
    }
    bool operator!=(const Handle& o) const { return !(*this == o); }
};

}  // namespace keepstone

#endif
