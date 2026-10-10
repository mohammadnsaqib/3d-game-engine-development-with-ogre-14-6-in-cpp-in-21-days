// Keepstone -- Save.h  (Day 18)
//
// Writing the world down, and reading it back into a world that is not the same world.
//
// A SAVE FILE IS A CONTRACT WITH THE FUTURE, not a snapshot of memory. Everything awkward about
// this day comes from that one sentence. The file outlives the build that wrote it: a player
// saves on Tuesday, you ship a patch on Wednesday, and Thursday's executable has to make sense
// of Tuesday's bytes. Every decision below is that problem.
//
// WHY NOT JUST WRITE THE STRUCTS. `fwrite(&object, sizeof(object), 1, f)` is one line and it is
// wrong four ways at once, only one of which shows up on your own machine:
//
//   * a `std::string` member is a pointer to a heap block; you would write the pointer,
//   * the compiler's padding between members is unspecified and lands in the file,
//   * field order, alignment and size all change when the struct does -- which it will,
//   * and none of it announces itself. A memcpy'd save from last week loads, and the warden is
//     at coordinates nobody can explain.
//
// So the format is TEXT, one key per line. It is bigger on disk and that is a price worth
// paying: a save you can open in an editor is a save you can debug, diff, and paste into a bug
// report. Every figure in this chapter is a picture of a file you could have typed yourself.
//
// THE VERSION IS THE FIRST TOKEN IN THE FILE and it is not optional. Three cases, and each needs
// a deliberate answer rather than an accident:
//
//   older than us   migrate it forward. `Save.cpp` does exactly one of these, from 1 to 2.
//   equal to us     read it.
//   NEWER than us   REFUSE. This is the one people get wrong: a v3 file read by a v2 reader
//                   looks like it works, because the keys a v2 reader knows are all still there.
//                   It silently drops whatever v3 added. Refusing is the only honest answer.
//
// UNKNOWN KEYS ARE SKIPPED, NOT FATAL, and that is not a contradiction with refusing a newer
// version. A key we do not know inside a version we DO know is a field somebody added and the
// version number should have moved for; skipping it keeps one stale line from bricking a save.
// A whole version we do not know is a different promise.
//
// A TRUNCATED FILE IS REFUSED OUTRIGHT -- the same rule as Day 17's behaviour loader, for the
// same reason. Half a world is not a world, and the half you get is decided by where the disk
// ran out rather than by anything meaningful.
//
// **HANDLES ARE NEVER WRITTEN TO DISK.** Day 9's handle is an index plus a generation, and both
// are facts about one run of one process: the generation counts how many tenants a slot has had
// since the program started. Writing that down and reading it back into a fresh world produces a
// handle that is either dangling or, far worse, valid and pointing at the wrong object. Objects
// are identified in the file by NAME, and the loader hands back a fresh handle for each.

#ifndef KEEPSTONE_SAVE_H
#define KEEPSTONE_SAVE_H

#include "keepstone/World.h"

#include <string>
#include <vector>

namespace keepstone {

// Bumped whenever a field is added, removed or reinterpreted. Version 1 wrote no `scale`;
// version 2 does, and `migrate()` in Save.cpp is where a v1 file gets one.
const int SAVE_VERSION = 2;

// WHY A CODE AND NOT JUST THE MESSAGE. The message is for a person; the code is for anything
// that has to BRANCH on the refusal -- a UI offering "this save is from a newer version, update
// the game?" cannot parse English, and the first thing that tries will match on a substring and
// break the day somebody rewords the message. The chapter's figures use it too: two scenes that
// both end with an empty world are the same picture until the refusal itself is drawable.
enum class SaveRefusal
{
    None = 0,
    NotASaveFile = 1,
    NoVersion = 2,
    NewerVersion = 3,
    Truncated = 4,
    CountMismatch = 5,
    CannotOpen = 6,
    WorldNotEmpty = 7,
};

struct SaveReport
{
    bool        ok = false;
    SaveRefusal refusal = SaveRefusal::None;
    int         version = 0;        // what the file claimed
    int         objects = 0;        // how many came back
    int         migrated = 0;       // fields filled in by a migration
    int         skippedKeys = 0;    // unknown keys stepped over
    std::string error;              // empty when ok
};

// Writes every living object in `world` as text. Destroyed slots are not written at all: the
// holes Day 9 leaves in the slot table are a runtime detail, and a reloaded world compacts.
bool saveWorld(const World& world, const std::string& path, std::string& error);

// Reads a file into an EMPTY world. Anything already in `world` is left alone and the load is
// refused -- loading on top of a populated world is how two saves end up merged into one
// impossible state.
SaveReport loadWorld(World& world, const std::string& path);

// Field-by-field comparison of two worlds, by object name. Returns the number of differences and
// fills `lines` with one human-readable line per difference.
//
// THIS IS THE CHAPTER'S TEST. A save format is correct when a world, written and read back, is
// the same world -- and "the same" is only a claim until something compares them. A round trip
// that reports zero differences is evidence; a screenshot that looks similar is not.
int diffWorlds(World& a, World& b, std::vector<std::string>& lines);

}  // namespace keepstone

#endif
