// Keepstone -- Behaviour.cpp  (Day 17)
//
// The whole parser and the whole state machine. No OGRE here, and no game either: this file
// knows about states, speeds, three named numbers and a file on disk.

#include "keepstone/Behaviour.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace keepstone {

namespace {

// The variable table. Adding a fourth variable means one line here and one field in Vars, and
// that visibility is the reason this is a table and not a map built at load time.
float valueOf(const std::string& name, const Vars& v, bool& known)
{
    known = true;
    if (name == "timer")    return v.timer;
    if (name == "intruder") return v.intruder;
    if (name == "home")     return v.home;
    known = false;
    return 0.0f;
}

long long modTimeOf(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<long long>(st.st_mtime);
}

}  // namespace

bool Behaviour::load(const std::string& path)
{
    mPath = path;
    mError.clear();

    std::ifstream in(path.c_str());
    if (!in)
    {
        mError = "cannot open " + path;
        return false;
    }

    // PARSED INTO A LOCAL, swapped in at the end. See the header: this is what makes a
    // bad file a no-op instead of a half-loaded state machine.
    std::vector<State> parsed;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line))
    {
        ++lineNo;
        // Comments and blank lines. A `#` anywhere ends the line, so a trailing note after a
        // rule is legal -- designers write those, and rejecting them teaches people to fear
        // the file.
        const std::string::size_type hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);

        std::istringstream ls(line);
        std::string word;
        if (!(ls >> word)) continue;

        if (word == "state")
        {
            State s;
            if (!(ls >> s.name))
            {
                mError = "line " + std::to_string(lineNo) + ": state with no name";
                return false;
            }
            parsed.push_back(s);
            continue;
        }

        if (parsed.empty())
        {
            mError = "line " + std::to_string(lineNo) + ": '" + word + "' before any state";
            return false;
        }

        if (word == "speed")
        {
            if (!(ls >> parsed.back().speed))
            {
                mError = "line " + std::to_string(lineNo) + ": speed with no number";
                return false;
            }
            continue;
        }

        if (word == "goto")
        {
            Rule r;
            std::string when, op;
            if (!(ls >> r.target >> when >> r.var >> op >> r.value) || when != "when")
            {
                mError = "line " + std::to_string(lineNo) +
                         ": expected 'goto <state> when <var> <  or  > <number>'";
                return false;
            }
            if (op != "<" && op != ">")
            {
                mError = "line " + std::to_string(lineNo) + ": comparison must be < or >, got '"
                         + op + "'";
                return false;
            }
            r.greater = (op == ">");

            // A VARIABLE NAME IS CHECKED HERE, NOT AT RUN TIME. A misspelt `intrudor`
            // that is only noticed during step() is a rule that silently never fires,
            // and a rule that never fires looks exactly like a rule whose condition is
            // simply not met yet.
            bool known = false;
            Vars probe;
            valueOf(r.var, probe, known);
            if (!known)
            {
                mError = "line " + std::to_string(lineNo) + ": unknown variable '" +
                         r.var + "' (timer, intruder, home)";
                return false;
            }
            parsed.back().rules.push_back(r);
            continue;
        }

        mError = "line " + std::to_string(lineNo) + ": unknown keyword '" + word + "'";
        return false;
    }

    if (parsed.empty())
    {
        mError = path + " declares no states";
        return false;
    }

    // A `goto` naming a state that does not exist is the other mistake that hides: the rule
    // fires, the object moves to a state nobody defined, and it stops moving for reasons no
    // one can see. Checked once here, so step() never has to.
    for (const State& s : parsed)
        for (const Rule& r : s.rules)
        {
            bool found = false;
            for (const State& t : parsed) if (t.name == r.target) { found = true; break; }
            if (!found)
            {
                mError = "state '" + s.name + "' goes to '" + r.target + "', which is not a "
                         "state in this file";
                return false;
            }
        }

    mStates = parsed;
    mFirst  = parsed.front().name;
    mStamp  = modTimeOf(path);
    return true;
}

bool Behaviour::reloadIfChanged()
{
    const long long now = modTimeOf(mPath);
    if (now == 0 || now == mStamp) return false;

    // Keep what we have in case the new file is bad. load() already refuses to install a broken
    // file, but the stamp must move either way -- otherwise a file that is broken and left
    // broken is re-read, and re-reported, on every single tick.
    const bool ok = load(mPath);
    mStamp = now;
    return ok;
}

const State* Behaviour::find(const std::string& name) const
{
    for (const State& s : mStates) if (s.name == name) return &s;
    return nullptr;
}

float Behaviour::step(BehaviourState& bs, const Vars& vars, float dt) const
{
    if (mStates.empty()) return 0.0f;
    if (bs.current.empty()) bs.current = mFirst;

    const State* s = find(bs.current);
    if (!s)
    {
        // The running object's state was removed by a live reload. Falling back to the
        // first state is the honest answer: the alternative is an object frozen forever
        // because a designer renamed something.
        bs.current = mFirst;
        bs.timer   = 0.0f;
        s = find(bs.current);
        if (!s) return 0.0f;
    }

    bs.timer += dt;

    Vars v = vars;
    v.timer = bs.timer;

    // FIRST MATCH WINS, IN FILE ORDER. That is a rule the designer can see, because the
    // file is the order. Testing all of them and taking the "best" would need a notion
    // of better that nothing in the file expresses.
    for (const Rule& r : s->rules)
    {
        bool known = false;
        const float value = valueOf(r.var, v, known);
        if (!known) continue;
        const bool hit = r.greater ? (value > r.value) : (value < r.value);
        if (hit)
        {
            bs.current = r.target;
            bs.timer   = 0.0f;
            ++bs.transitions;
            const State* next = find(bs.current);
            return next ? next->speed : 0.0f;
        }
    }
    return s->speed;
}

}  // namespace keepstone
