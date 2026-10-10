// Keepstone -- Assets.h
//
// A MANIFEST, AND A BUDGET. Those are the two ideas, and the second one is the one that keeps
// the frame rate.
//
// OGRE already knows how to find a file: resource locations and groups do that, and Day 2 used
// them. What OGRE does not decide is WHEN. Ask it for twenty textures at the moment a door
// opens and it will fetch twenty textures, right then, on the thread that was drawing -- and
// the frame that door opened on takes as long as twenty file reads.
//
// That is the hitch. Not a slow frame rate: a single frame of several hundred milliseconds, in
// the middle of an otherwise smooth game, at the exact moment the player did something. It is
// the most noticeable performance fault a game can have and it never shows up in an average.
//
// THE FIX IS NOT FASTER LOADING. It is loading a little at a time, with a ceiling on how much
// any one frame is allowed to spend. `pump()` below takes that ceiling as an argument.

#ifndef KEEPSTONE_ASSETS_H
#define KEEPSTONE_ASSETS_H

#include "keepstone/Engine.h"

#include <string>
#include <vector>

namespace keepstone {

class Assets : public Service
{
public:
    struct Entry
    {
        std::string name;
        std::string file;
        bool        loaded = false;
        std::size_t bytes = 0;
    };

    // NO ROOT DIRECTORY IN THIS SIGNATURE, and the first draft had one. Passing the asset root
    // in from the game meant the game had to know where assets live -- and it does not, and
    // must not: `LK_BOOK_MEDIA` is PRIVATE on the engine target, so `lanternkeep` could not
    // even compile the line. The build refused it before any gate looked at it, which is the
    // boundary from Day 4 doing exactly the job it was drawn for.
    //
    // Where the files are is the engine's business. Day 21 moves them into an archive and this
    // signature does not change.
    Assets();

    const char* name() const override { return "assets"; }
    bool startUp() override;
    void shutDown() override;

    // THE MANIFEST. A name the game uses, and a file the engine finds. Everything above this
    // line says "blockwork" and nothing above it says where blockwork lives -- which is what
    // lets Day 21 move every file into an archive without a gameplay file noticing.
    void declare(const std::string& name, const std::string& file);

    // Everything, right now, on this thread. Honest and sometimes correct -- a loading screen
    // wants exactly this -- and catastrophic in the middle of a frame.
    void loadAllNow();

    // Queue it. Nothing is read until pump() is called.
    void request(const std::string& name);

    // Load queued assets until the budget is spent, then stop, EVEN IF MORE ARE WAITING.
    // Returns how many were loaded this call. The stopping is the whole feature.
    int pump(double budgetMs);

    // DAY 11 REPORTS A COUNT BECAUSE IT CANNOT HONESTLY REPORT A TIME.
    //
    // This chapter measures reading files off a disk, and how long that takes belongs to the
    // machine and the moment: the same binary reported its worst frame at 311, 324, 347 and once
    // 1355 per cent of budget without anything being wrong. The CAUSE of the hitch is not a
    // duration at all -- it is how many loads landed in one frame -- and that is identical on
    // every machine that ever runs this.
    //
    // `markFrame()` is called once per frame by the game. Everything loaded between two marks
    // belongs to the frame between them.
    void markFrame();

    // Begin measuring from HERE, without counting what came before as a frame's work. A loading
    // screen runs outside the frame loop, and calling markFrame() to skip it would record the
    // loading screen itself as the busiest frame -- which is what the first version of this did.
    void startMeasuring();
    int  mostLoadedInOneFrame() const { return mWorstFrameLoads; }

    bool isLoaded(const std::string& name) const;
    int  loadedCount() const;
    int  pendingCount() const;
    std::size_t bytesLoaded() const;
    const std::vector<Entry>& entries() const { return mEntries; }

private:
    bool loadOne(Entry& e);

    std::string mRoot;
    std::vector<Entry> mEntries;
    std::vector<std::string> mQueue;
    std::size_t mBytes = 0;
    int mLoadedAtFrameStart = 0;
    int mWorstFrameLoads = 0;
};

}  // namespace keepstone

#endif
