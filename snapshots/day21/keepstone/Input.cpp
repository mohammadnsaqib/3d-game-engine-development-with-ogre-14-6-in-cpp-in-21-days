// Keepstone -- Input.cpp

#include "Input.h"

#include <algorithm>

namespace keepstone {

void InputContext::bind(int code, const std::string& action)
{
    for (Binding& b : mBindings)
    {
        if (b.code == code) { b.action = action; return; }   // one code, one meaning per context
    }
    Binding b; b.code = code; b.action = action;
    mBindings.push_back(b);
}

void InputContext::unbind(int code)
{
    for (std::size_t i = 0; i < mBindings.size(); ++i)
    {
        if (mBindings[i].code == code) { mBindings.erase(mBindings.begin() + i); return; }
    }
}

std::string InputContext::actionFor(int code) const
{
    for (const Binding& b : mBindings)
        if (b.code == code) return b.action;
    return std::string();
}

std::vector<std::string> InputContext::describe() const
{
    std::vector<std::string> out;
    for (const Binding& b : mBindings)
        out.push_back("  " + mName + ": key " + std::to_string(b.code) + " -> " + b.action);
    return out;
}

bool Input::startUp() { return true; }

void Input::shutDown()
{
    mStack.clear();
    mDown.clear();
}

void Input::push(InputContext* c) { if (c) mStack.push_back(c); }

void Input::pop()
{
    if (mStack.empty()) return;

    // EVERY ACTION THIS CONTEXT OWNED IS RELEASED ON THE WAY OUT. Leaving them down is the
    // classic bug: open a menu while walking, close it, and the warden keeps walking forever
    // because the key-up went to a context that no longer exists.
    const InputContext* going = mStack.back();
    for (std::size_t i = 0; i < mDown.size(); )
    {
        bool owned = false;
        for (int code = 0; code < 512; ++code)
            if (going->actionFor(code) == mDown[i]) { owned = true; break; }
        if (owned) { mLog.push_back("release " + mDown[i] + " (context popped)");
                     mDown.erase(mDown.begin() + i); }
        else ++i;
    }
    mStack.pop_back();
}

const InputContext* Input::top() const
{
    return mStack.empty() ? nullptr : mStack.back();
}

int Input::contextDepth() const { return static_cast<int>(mStack.size()); }

void Input::feed(const DeviceEvent& e)
{
    // TOP DOWN, FIRST MATCH WINS. The loop runs backwards because the top of the stack is the
    // end of the vector, and a menu on top must be able to take a key the play context also
    // binds without the play context ever seeing it.
    std::string action;
    for (std::size_t i = mStack.size(); i-- > 0; )
    {
        action = mStack[i]->actionFor(e.code);
        if (!action.empty()) break;

        // AN EXCLUSIVE CONTEXT STOPS THE SEARCH EVEN WHEN IT BINDS NOTHING. Without this the
        // stack falls through and a menu lets the warden keep walking behind it.
        if (mStack[i]->isExclusive()) break;
    }

    if (action.empty())
    {
        // NOT AN ERROR. Most keys mean nothing in most contexts, and a game that logged a
        // warning for each one would log hundreds a minute. Counted, so the chapter can show
        // how many went nowhere, and otherwise ignored.
        ++mUnresolved;
        return;
    }

    std::vector<std::string>::iterator it = std::find(mDown.begin(), mDown.end(), action);
    if (e.kind == DeviceEvent::KeyDown)
    {
        if (it == mDown.end())
        {
            mDown.push_back(action);
            mLog.push_back("down    " + action);
        }
        // An auto-repeating key sends KeyDown over and over; the action is already down and
        // must not be added twice, or the release below leaves it down.
    }
    else if (it != mDown.end())
    {
        mDown.erase(it);
        mLog.push_back("up      " + action);
    }
}

bool Input::isDown(const std::string& action) const
{
    return std::find(mDown.begin(), mDown.end(), action) != mDown.end();
}

void Input::rebind(int oldCode, int newCode)
{
    // REBINDING REACHES ONE PLACE. Nothing downstream stored the key, so there is nothing else
    // to update -- which is the payoff for every indirection above.
    if (mStack.empty()) return;
    InputContext* c = mStack.back();
    const std::string action = c->actionFor(oldCode);
    if (action.empty()) return;
    c->unbind(oldCode);
    c->bind(newCode, action);
    mLog.push_back("rebind  " + action + ": key " + std::to_string(oldCode) +
                   " -> " + std::to_string(newCode));
}

}  // namespace keepstone
