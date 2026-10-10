// Keepstone -- Prefab.h
//
// A PREFAB IS A GAME OBJECT DESCRIBED IN A FILE INSTEAD OF IN CODE.
//
// Every object in the Lantern Keep so far has been built by a `for` loop in main(): create,
// new Transform, set a position, new Visual. That works, and it means changing where a wall
// stands is a recompile -- which means the person who decides where walls stand has to be a
// person who can compile the game.
//
// The change today is not technical. A prefab file is a few lines of text with no cleverness in
// it at all. What it buys is that the courtyard stops being a consequence of the program and
// becomes an input to it, and those are different things to own.
//
// THE FORMAT IS DELIBERATELY DULL. One directive a line, whitespace separated, `#` for a
// comment. No brackets to balance, no indentation to get wrong, and every parse failure names
// the line it happened on. A format a reader can fix in Notepad at midnight is worth more than
// one that round-trips perfectly.

#ifndef KEEPSTONE_PREFAB_H
#define KEEPSTONE_PREFAB_H

#include "keepstone/Handle.h"
#include "keepstone/World.h"

#include <string>
#include <vector>

namespace keepstone {

class Prefab
{
public:
    struct Definition
    {
        std::string name;
        Vec3   position;
        Vec3   scale { 0.2f, 0.4f, 0.2f };
        float  yaw = 0.0f;
        Colour tint { 1.0f, 1.0f, 1.0f };
        bool   visual = true;
        bool   glow = false;
        Vec3   velocity;
        bool   mover = false;
        int    count = 1;        // spawn this many, spaced by `spacing`
        Vec3   spacing;
    };

    // Reads a file into definitions. Returns false and fills `error` on the first bad line --
    // and NAMES THE LINE, because "parse error" in a data file is the least useful sentence a
    // program can print.
    bool loadFile(const std::string& path, std::string& error);

    // Builds the definitions into the world. Returns the handles it made, so the caller can
    // keep them -- handles, not pointers, because Day 9.
    std::vector<Handle> spawnAll(World& world) const;

    const std::vector<Definition>& definitions() const { return mDefs; }
    int totalObjects() const;

private:
    std::vector<Definition> mDefs;
};

}  // namespace keepstone

#endif
