// Keepstone -- Behaviour.h  (Day 17)
//
// Behaviour that lives in a FILE, not in a function.
//
// Every day up to here put the game's decisions in C++: the warden moved because Day 14's
// character controller moved it, and changing how it moved meant changing a translation unit and
// waiting for a compiler. That is fine for one warden. It stops being fine the first time a
// designer wants to try four patrol speeds before lunch.
//
// WHAT THIS IS NOT. It is not a scripting language, and adding one is not the next step. A
// scripting language brings a parser, a garbage collector, a debugger you did not write, and a
// second set of names for everything the engine already has. What a warden needs is much
// smaller: a list of states, a speed in each, and the conditions that move it between them.
// That fits in a text file with four keywords, and the whole of it is read by Behaviour.cpp in
// about a hundred lines.
//
// THE FORMAT, in full:
//
//     state patrol           # a name, and everything indented under it belongs to it
//       speed 60             # how fast the object moves while in this state
//       goto chase when intruder < 200
//                            # a transition: target state, a variable, a comparison, a number
//
// Four keywords -- `state`, `speed`, `goto ... when` -- and three variables the game feeds in
// (`timer`, `intruder`, `home`). A designer can read the whole grammar off one example, which is
// the actual test of whether a data format is small enough.
//
// WHY THE VARIABLES COME FROM THE GAME. Behaviour does not know what an intruder is, how far
// away one might be, or what "home" means. It is given a Vars struct each tick and compares
// numbers. That is what keeps this file free of gameplay and keeps the gameplay free of parsing.
//
// LIVE RELOAD IS THE POINT, not a convenience. `reloadIfChanged()` re-reads the file mid-run.
// The warden keeps its current state and its timer; only the rules underneath it change. A
// designer edits a number, saves, and watches the running game change without restarting it --
// and a failed parse changes NOTHING, because the half-loaded state machine is the one failure
// this class refuses to have. See `load()`.

#ifndef KEEPSTONE_BEHAVIOUR_H
#define KEEPSTONE_BEHAVIOUR_H

#include <string>
#include <vector>

namespace keepstone {

// What the game measures and the behaviour compares against. Three numbers, and adding a fourth
// means adding it here and in one table in Behaviour.cpp -- deliberately a small, visible edit
// rather than an extension point.
struct Vars
{
    float timer    = 0.0f;   // seconds in the current state, reset on every transition
    float intruder = 1.0e9f; // distance to the nearest intruder, huge when there is none
    float home     = 0.0f;   // distance from where this object started
};

struct Rule
{
    std::string target;      // the state to move to
    std::string var;         // "timer", "intruder" or "home"
    bool        greater;     // true for '>', false for '<'
    float       value;
};

struct State
{
    std::string       name;
    float             speed = 0.0f;
    std::vector<Rule> rules;   // tested in file order; the FIRST match wins, see step()
};

// What one object remembers. Kept separate from the Behaviour asset because many objects share
// one asset -- two wardens reading one file have two of these and one of that.
struct BehaviourState
{
    std::string current;
    float       timer       = 0.0f;
    int         transitions = 0;
};

class Behaviour
{
public:
    // Reads the file and REPLACES the rules only if the whole file parsed.
    //
    // A parser that writes as it reads leaves a half-built state machine behind when line 40 is
    // malformed, and the object goes on running against rules that are partly old and partly
    // new. Nothing announces it; the warden simply behaves like neither version. So this parses
    // into a local vector and swaps it in at the end, which makes a bad file a no-op with a
    // message rather than a silent corruption.
    //
    // Returns false and leaves the previous rules untouched if anything failed.
    bool load(const std::string& path);

    // Re-reads the file if its modification time has moved since the last read. Returns true
    // only when new rules were actually installed -- false covers both "unchanged" and "changed
    // but would not parse", and `lastError()` tells those apart.
    bool reloadIfChanged();

    // One tick for one object. Advances the timer, tests this state's rules in file order and
    // takes the first that holds. Returns the speed the object should move at.
    float step(BehaviourState& bs, const Vars& vars, float dt) const;

    const State* find(const std::string& name) const;
    const std::vector<State>& states() const { return mStates; }
    const std::string& firstState() const { return mFirst; }
    const std::string& lastError() const { return mError; }
    bool loaded() const { return !mStates.empty(); }

private:
    std::vector<State> mStates;
    std::string        mFirst;     // the first state in the file is where an object starts
    std::string        mPath;
    std::string        mError;
    long long          mStamp = 0; // last modification time seen
};

}  // namespace keepstone

#endif
