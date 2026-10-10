// Keepstone -- Profiler.h
//
// AN ENGINE THAT CANNOT SAY WHERE ITS TIME WENT IS AN ENGINE YOU OPTIMISE BY GUESSING. This is
// the smallest instrument that answers the question, and it goes in on Day 7 rather than on
// Day 20 for one reason: a measurement you only take when something is already slow has no
// history to compare against, so it cannot tell you what changed.
//
// THE BUDGET IS THE POINT, NOT THE MILLISECONDS. At 60 frames a second a frame is 16.67 ms and
// that is the whole allowance -- simulation, rendering, audio, everything. A number without the
// budget beside it ("4.2 ms") tells the reader nothing; the same number against the budget
// ("4.2 of 16.67, a quarter spent") tells them whether to care.

#ifndef KEEPSTONE_PROFILER_H
#define KEEPSTONE_PROFILER_H

#include <string>
#include <vector>

namespace keepstone {

class Profiler
{
public:
    struct Entry
    {
        std::string name;
        double totalMs = 0.0;
        int    calls = 0;
        double meanMs() const { return calls ? totalMs / calls : 0.0; }
    };

    explicit Profiler(double budgetMs = 1000.0 / 60.0);

    // Opens a named section. Sections do not nest in this version, and that is a deliberate
    // limit rather than an oversight: a flat list is readable at a glance, and Day 20 is where
    // a tree earns its complexity.
    void begin(const std::string& name);
    void end();

    void beginFrame();
    void endFrame();

    double budgetMs() const { return mBudgetMs; }
    int    frames() const { return mFrames; }
    double meanFrameMs() const;
    int    framesOverBudget() const { return mOver; }

    // Sorted by cost, because the first question is always "what is the biggest thing".
    std::vector<Entry> entries() const;

    // A verdict, not a number. Timings move between runs; whether a frame fits its budget does
    // not, and it is the claim a book can print and a reader can reproduce.
    bool withinBudget() const { return mOver == 0; }

private:
    struct Open { std::string name; long long startUs; };

    double mBudgetMs;
    std::vector<Entry> mEntries;
    std::vector<Open>  mOpen;
    long long mFrameStartUs = 0;
    double    mFrameTotalMs = 0.0;
    int       mFrames = 0;
    int       mOver = 0;
};

}  // namespace keepstone

#endif
