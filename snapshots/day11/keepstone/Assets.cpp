// Keepstone -- Assets.cpp
//
// The reads below are REAL. This service opens the book's own texture files and reads every
// byte, so the milliseconds the chapter prints are milliseconds a disk actually spent. A timing
// demonstration built on a sleep measures the scheduler; one built on a loop measures the CPU;
// only one built on the work measures the work.

#include "Assets.h"

#include <chrono>
#include <fstream>

namespace keepstone {

namespace {

double nowMs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() / 1000.0;
}

}  // namespace

Assets::Assets() : mRoot(LK_BOOK_MEDIA) {}

bool Assets::startUp()
{
    // Nothing is read here. A service coming up should make itself ready, not do the work --
    // an engine whose start-up loads every asset is an engine with a long black screen.
    return true;
}

void Assets::shutDown()
{
    mEntries.clear();
    mQueue.clear();
    mBytes = 0;
}

void Assets::declare(const std::string& name, const std::string& file)
{
    Entry e;
    e.name = name;
    e.file = file;
    mEntries.push_back(e);
}

bool Assets::loadOne(Entry& e)
{
    if (e.loaded) return false;

    std::ifstream in((mRoot + "/" + e.file).c_str(), std::ios::binary);
    if (!in)
    {
        // A MISSING ASSET IS NOT A CRASH AND NOT A SILENCE. It is marked loaded so the engine
        // stops asking, and its size stays zero so the report shows what happened.
        e.loaded = true;
        return true;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    e.bytes = buffer.size();
    e.loaded = true;
    mBytes += e.bytes;
    return true;
}

void Assets::markFrame()
{
    const int now = loadedCount();
    const int thisFrame = now - mLoadedAtFrameStart;
    if (thisFrame > mWorstFrameLoads) mWorstFrameLoads = thisFrame;
    mLoadedAtFrameStart = now;
}

void Assets::startMeasuring()
{
    mLoadedAtFrameStart = loadedCount();
    mWorstFrameLoads = 0;
}

void Assets::loadAllNow()
{
    for (Entry& e : mEntries) loadOne(e);
    mQueue.clear();
}

void Assets::request(const std::string& name)
{
    for (const Entry& e : mEntries)
        if (e.name == name && !e.loaded) { mQueue.push_back(name); return; }
}

int Assets::pump(double budgetMs)
{
    const double start = nowMs();
    int done = 0;

    while (!mQueue.empty())
    {
        // CHECKED BEFORE EACH LOAD, NOT AFTER. Checking after means the budget is always
        // overspent by one asset, and one asset is exactly the thing that was too expensive.
        if (nowMs() - start >= budgetMs) break;

        const std::string want = mQueue.front();
        mQueue.erase(mQueue.begin());
        for (Entry& e : mEntries)
        {
            if (e.name == want) { if (loadOne(e)) ++done; break; }
        }
    }
    return done;
}

bool Assets::isLoaded(const std::string& name) const
{
    for (const Entry& e : mEntries)
        if (e.name == name) return e.loaded;
    return false;
}

int Assets::loadedCount() const
{
    int n = 0;
    for (const Entry& e : mEntries) if (e.loaded) ++n;
    return n;
}

int Assets::pendingCount() const { return static_cast<int>(mQueue.size()); }

std::size_t Assets::bytesLoaded() const { return mBytes; }

}  // namespace keepstone
