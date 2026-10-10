// Keepstone -- Engine.cpp
//
// NOTE WHAT IS NOT IN THIS FILE: OGRE. The engine object does not know a renderer from an audio
// mixer, and that is what lets Day 16 add audio without touching a line of it.

#include "Engine.h"

namespace keepstone {

Engine::Engine() {}

Engine::~Engine() { shutDown(); }

void Engine::add(Service* service)
{
    if (service) mServices.push_back(service);
}

bool Engine::startUp()
{
    for (Service* s : mServices)
    {
        if (s->startUp())
        {
            // RECORDED ONLY AFTER IT SUCCEEDS. A service that failed must not appear on
            // the started list, or the teardown below will shut down something that was
            // never up -- and a shutDown() on a half-built service is where the second
            // crash comes from, the one that hides the first.
            mStarted.push_back(s);
            mLog.push_back(std::string("up   ") + s->name());
        }
        else
        {
            mLog.push_back(std::string("FAIL ") + s->name());
            shutDown();
            return false;
        }
    }
    return true;
}

void Engine::shutDown()
{
    // REVERSE ORDER, and over the STARTED list rather than the registered one. Both
    // halves matter: reverse because a service may still be using one registered before
    // it, and the started list because anything that failed or never ran must not be
    // torn down.
    while (!mStarted.empty())
    {
        Service* s = mStarted.back();
        mStarted.pop_back();
        s->shutDown();
        mLog.push_back(std::string("down ") + s->name());
    }
}

const std::vector<std::string>& Engine::log() const
{
    return mLog;
}

}  // namespace keepstone
