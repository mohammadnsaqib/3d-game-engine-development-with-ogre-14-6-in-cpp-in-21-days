// Keepstone -- Input.h
//
// GAMEPLAY CODE MUST NEVER ASK WHICH KEY IS DOWN.
//
// It asks whether the warden should be walking left. Those are different questions, and the
// distance between them is the whole of today. A game that reads keys directly has the keyboard
// layout written through every behaviour it owns: rebinding means finding every place that
// mentioned a key, a gamepad means writing all of it twice, and a menu that wants the same key
// to mean something else has nowhere to say so.
//
// AN ACTION IS THE UNIT. Devices produce events; the action map turns events into actions;
// gameplay reads actions. Each layer knows only its neighbour.
//
// WHERE THE EVENTS COME FROM IS NOT THE ENGINE'S BUSINESS, and this snapshot makes use of that.
// It replays a recorded trace instead of polling a keyboard, because this book's programs render
// with no window on screen and a key press needs a focused window to exist. That is not a
// workaround for the book: a replayable input source is how input gets TESTED, and a game whose
// input can only be exercised by a person at a keyboard has an input system nobody can regress.
// The action map below does not know or care which source it is fed from.

#ifndef KEEPSTONE_INPUT_H
#define KEEPSTONE_INPUT_H

#include "keepstone/Engine.h"

#include <string>
#include <vector>

namespace keepstone {

// What a device produced. Deliberately small: a code, and whether it went down or up. Anything
// richer belongs to the device layer, not here.
struct DeviceEvent
{
    enum Kind { KeyDown, KeyUp };
    Kind kind = KeyDown;
    int  code = 0;        // a key code; the engine attaches no meaning to the number
};

// One context's worth of bindings. A context is a MODE -- playing, in a menu, reading a sign --
// and the same key honestly means different things in each. Contexts stack, and the top one
// that binds a key wins, which is why a menu can swallow Escape without the play context ever
// hearing about it.
class InputContext
{
public:
    explicit InputContext(const std::string& name) : mName(name) {}

    void bind(int code, const std::string& action);
    void unbind(int code);

    // MODAL. An exclusive context stops the search even when it binds nothing for that key, so
    // the contexts beneath it never hear it at all.
    //
    // This is not the same as binding every key you want to block, and the difference bit this
    // chapter. A menu that simply declines to bind the movement keys does NOT swallow them --
    // the stack falls through and the warden walks around behind the open menu. Blocking is a
    // property of the CONTEXT, not a gap in its bindings, and it has to be said out loud.
    void setExclusive(bool on) { mExclusive = on; }
    bool isExclusive() const { return mExclusive; }

    // Returns "" when this context does not bind the code, which is what lets the stack fall
    // through to the context below.
    std::string actionFor(int code) const;

    const std::string& name() const { return mName; }
    int bindingCount() const { return static_cast<int>(mBindings.size()); }
    std::vector<std::string> describe() const;

private:
    struct Binding { int code; std::string action; };
    std::string mName;
    std::vector<Binding> mBindings;
    bool mExclusive = false;
};

class Input : public Service
{
public:
    const char* name() const override { return "input"; }
    bool startUp() override;
    void shutDown() override;

    // Contexts are pushed and popped like a stack of transparencies: the top one that binds a
    // key answers, and the rest never see it.
    void push(InputContext* context);
    void pop();
    const InputContext* top() const;
    int contextDepth() const;

    // Feed one device event. The engine resolves it to an action, or to nothing.
    void feed(const DeviceEvent& e);

    // ACTIONS ARE STATE, NOT MESSAGES, and that is a decision worth defending. "Is the warden
    // walking left" is a question about right now, asked once a tick by whatever cares. An
    // event queue would make the caller responsible for not missing one, which is how a key
    // gets stuck down after a lost frame.
    bool isDown(const std::string& action) const;

    // Rebinding at run time, which is only possible because nothing downstream knows the code.
    void rebind(int oldCode, int newCode);

    int  unresolvedCount() const { return mUnresolved; }
    const std::vector<std::string>& log() const { return mLog; }

private:
    std::vector<InputContext*> mStack;
    std::vector<std::string>   mDown;
    std::vector<std::string>   mLog;
    int mUnresolved = 0;
};

}  // namespace keepstone

#endif
