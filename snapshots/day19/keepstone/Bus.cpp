// Keepstone -- Bus.cpp

#include "Bus.h"

namespace keepstone {

const char* Bus::kindName(Event::Kind k)
{
    switch (k)
    {
        case Event::PlateStepped:  return "PlateStepped";
        case Event::LanternLit:    return "LanternLit";
        case Event::LanternOut:    return "LanternOut";
        case Event::WardenBlocked: return "WardenBlocked";
        case Event::GateOpened:    return "GateOpened";
    }
    return "?";
}

void Bus::subscribe(Event::Kind kind, Listener* who)
{
    if (!who) return;
    Sub s; s.kind = kind; s.who = who;
    mSubs.push_back(s);
}

void Bus::publish(const Event& e)
{
    mQueue.push_back(e);
}

int Bus::deliver()
{
    // TAKE THE QUEUE, THEN DELIVER FROM THE COPY. Anything published by a handler lands in the
    // now-empty member queue and waits for the next drain. Iterating the member directly would
    // let a handler extend the range being iterated -- an unbounded frame, and on a vector an
    // invalidated iterator as well.
    std::vector<Event> batch;
    batch.swap(mQueue);

    int delivered = 0;
    for (const Event& e : batch)
    {
        bool heard = false;

        // IN SUBSCRIPTION ORDER, and that is a promise the engine makes rather than an accident
        // of how the vector happens to be laid out. Two listeners on one kind will sometimes
        // both act on the same state, and "whoever subscribed first sees it first" is at least
        // a rule somebody can rely on and write down.
        for (const Sub& s : mSubs)
        {
            if (s.kind != e.kind) continue;
            s.who->onEvent(e);
            mLog.push_back(std::string("  ") + kindName(e.kind) + " -> " + s.who->listenerName());
            heard = true;
            ++delivered;
        }

        if (!heard)
        {
            // AN EVENT NOBODY WANTED IS NOT AN ERROR. A game publishes plenty of things that
            // only matter in some modes. Counted so the chapter can show the number, and
            // otherwise dropped in silence.
            ++mNoListener;
            mLog.push_back(std::string("  ") + kindName(e.kind) + " -> (nobody)");
        }
    }
    mDelivered += delivered;
    return delivered;
}

}  // namespace keepstone
