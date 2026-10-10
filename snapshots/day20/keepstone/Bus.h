// Keepstone -- Bus.h
//
// THE PROBLEM THIS SOLVES IS A SHAPE, NOT A FEATURE.
//
// The warden steps on a pressure plate. The gate should open, the lanterns should flare, a sound
// should play, and the tutorial should tick off a step. Written directly, the plate now holds a
// pointer to the gate, to every lantern, to the audio service and to the tutorial -- and every
// one of those is a thing the plate has to be given, kept up to date, and told about when it is
// destroyed. Add a fifth listener and the plate changes again.
//
// None of those systems needs to know the plate exists. They need to know the plate FIRED.
//
// TWO DECISIONS WORTH DEFENDING.
//
// **Deferred, not immediate.** Publishing appends to a queue that is drained once a tick, at a
// point the engine chooses. Immediate delivery means a subscriber runs inside the publisher's
// call stack, which is how a lantern ends up modifying the world halfway through the collision
// pass that published the event -- and how two systems end up mutually recursing through a
// bus that looks like it decoupled them.
//
// **Typed by a small enum, not by inheritance.** A base Event class with virtual dispatch and
// a downcast in every handler costs an allocation per event and reads worse. The kinds a game
// has are few and known.

#ifndef KEEPSTONE_BUS_H
#define KEEPSTONE_BUS_H

#include "keepstone/Handle.h"

#include <string>
#include <vector>

namespace keepstone {

struct Event
{
    enum Kind
    {
        PlateStepped,
        LanternLit,
        LanternOut,
        WardenBlocked,
        GateOpened,
    };

    Kind   kind = PlateStepped;
    Handle who;            // which game object it happened to, when that makes sense
    float  value = 0.0f;   // whatever the kind means by a number
};

// A thing that wants to hear. Subclassing is the cheapest way to say "I have state and I want
// events", and the engine never has to own it.
class Listener
{
public:
    virtual ~Listener() {}
    virtual const char* listenerName() const = 0;
    virtual void onEvent(const Event& e) = 0;
};

class Bus
{
public:
    // Subscribing to a KIND, not to everything. A listener that filters inside its own handler
    // still gets woken for every event in the game, which on a busy frame is most of the cost
    // of having a bus at all.
    void subscribe(Event::Kind kind, Listener* who);

    // Queues. Nothing runs yet -- see the note above about publisher call stacks.
    void publish(const Event& e);

    // Delivers everything queued, in publish order, and returns how many were delivered.
    //
    // EVENTS PUBLISHED DURING DELIVERY GO TO THE NEXT DRAIN, not this one. Draining a queue
    // that can grow while you drain it is an unbounded loop wearing a helpful face: a lantern
    // that publishes when it lights, lighting a lantern that publishes, and the frame never
    // ends. One tick's events are delivered this tick; what they cause happens next tick, and
    // that delay is a feature a reader can reason about.
    int deliver();

    int queuedCount() const { return static_cast<int>(mQueue.size()); }
    int deliveredTotal() const { return mDelivered; }
    int droppedNoListener() const { return mNoListener; }
    const std::vector<std::string>& log() const { return mLog; }

    static const char* kindName(Event::Kind k);

private:
    struct Sub { Event::Kind kind; Listener* who; };
    std::vector<Sub>   mSubs;
    std::vector<Event> mQueue;
    std::vector<std::string> mLog;
    int mDelivered = 0;
    int mNoListener = 0;
};

}  // namespace keepstone

#endif
