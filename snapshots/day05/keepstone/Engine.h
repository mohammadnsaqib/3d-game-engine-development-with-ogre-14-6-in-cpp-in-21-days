// Keepstone -- Engine.h
//
// One object that owns every subsystem and the order they come up in. Still no OGRE in this
// header: the boundary drawn on Day 4 holds, and everything added from here sits behind it.
//
// WHY AN ORDER AT ALL. A service is not independent of the others. Audio needs to know the
// frame rate before it can size its buffers; a profiler cannot report on a renderer that does
// not exist; nothing can load an asset before the asset service knows where to look. Start-up
// order is not a preference, it is the dependency graph written down -- and writing it down in
// one place is the difference between an engine and a sequence of calls in main().
//
// SHUTDOWN IS THE REVERSE, ALWAYS. Day 5 shows what happens when it is not.

#ifndef KEEPSTONE_ENGINE_H
#define KEEPSTONE_ENGINE_H

#include <string>
#include <vector>

namespace keepstone {

// Everything the engine owns implements this. Three functions is the whole contract, and that
// is deliberate: a service that needs more than this from the engine is usually a service that
// should own the thing it is reaching for.
class Service
{
public:
    virtual ~Service() {}

    // Named, because every message this engine prints about start-up names the service
    // it is talking about. A failure that says "service 2 failed" is a failure you have
    // to go and count brackets to understand.
    virtual const char* name() const = 0;

    // Returns false to stop the engine coming up. It must leave nothing half-built
    // behind: whatever it allocated before it failed, it releases before it returns.
    virtual bool startUp() = 0;

    virtual void shutDown() = 0;
};

class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // The engine does NOT take ownership: the caller keeps the service alive. That keeps this
    // class free of any opinion about how services are allocated, which matters because Day 7's
    // profiler lives on the stack of main() and Day 11's asset service will not.
    void add(Service* service);

    // Starts every service in the order they were added, stopping at the first failure
    // and shutting down everything already up, in reverse. Returns false if any service
    // refused.
    bool startUp();

    // Reverse order, always, and safe to call when startUp() failed or was never
    // called.
    void shutDown();

    // What actually happened, in order, as lines. The book prints this rather than describing
    // it, and Day 5's whole lesson is visible in the difference between two of these lists.
    const std::vector<std::string>& log() const;

private:
    std::vector<Service*> mServices;
    // Only the ones that came up, for an honest teardown.
    std::vector<Service*> mStarted;
    std::vector<std::string> mLog;
};

}  // namespace keepstone

#endif
