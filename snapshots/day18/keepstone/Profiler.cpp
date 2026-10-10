// Keepstone -- Profiler.cpp

#include "Profiler.h"

#include <algorithm>
#include <chrono>

namespace keepstone {

namespace {

// STEADY_CLOCK, NOT SYSTEM_CLOCK. system_clock is the wall calendar and can jump backwards when
// the machine syncs its time; a negative duration in a profiler is a number that makes no sense
// and propagates into every average after it. steady_clock only ever moves forward.
long long nowUs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

Profiler::Profiler(double budgetMs) : mBudgetMs(budgetMs) {}

void Profiler::begin(const std::string& name)
{
    Open o;
    o.name = name;
    o.startUs = nowUs();
    mOpen.push_back(o);
}

void Profiler::end()
{
    if (mOpen.empty()) return;
    const Open o = mOpen.back();
    mOpen.pop_back();
    const double ms = (nowUs() - o.startUs) / 1000.0;

    for (Entry& e : mEntries)
    {
        if (e.name == o.name)
        {
            e.totalMs += ms;
            ++e.calls;
            return;
        }
    }
    Entry e;
    e.name = o.name;
    e.totalMs = ms;
    e.calls = 1;
    mEntries.push_back(e);
}

void Profiler::beginFrame() { mFrameStartUs = nowUs(); }

void Profiler::endFrame()
{
    const double ms = (nowUs() - mFrameStartUs) / 1000.0;
    mFrameTotalMs += ms;
    ++mFrames;
    if (ms > mBudgetMs) ++mOver;
    if (ms > mWorstMs) mWorstMs = ms;
}

double Profiler::meanFrameMs() const
{
    return mFrames ? mFrameTotalMs / mFrames : 0.0;
}

std::vector<Profiler::Entry> Profiler::entries() const
{
    std::vector<Entry> out = mEntries;
    std::sort(out.begin(), out.end(),
              [](const Entry& a, const Entry& b) { return a.totalMs > b.totalMs; });
    return out;
}

}  // namespace keepstone
