// Keepstone -- Tools.h  (Day 19)
//
// The console and the inspector: the two tools that turn an engine into something you can work
// in rather than only compile.
//
// WHY TOOLS ARE ENGINE CODE AND NOT A SIDE PROJECT. Every day so far, changing anything about
// the world meant editing C++ and waiting for a build. Day 17 moved the warden's decisions into
// a file and Day 18 made a world something you can write down and read back; both of those are
// worth much more once you can ALSO poke at a running world and see what is actually in it.
// That is all a console is.
//
// THE CONSOLE OPERATES ON THE WORLD, NOT ON THE RENDERER. `move warden 120 20 0` writes to a
// Transform and sets the dirty flag Day 10 built; the bridge pushes it to the renderer on the
// next tick exactly as it would for any other change. A console that reached into the renderer
// directly would move the picture and not the world, and the two would disagree the moment
// anything else touched that object -- a bug that looks like a rendering fault and is not.
//
// EVERY COMMAND RETURNS TEXT, including the ones that fail. The transcript is the tool's whole
// interface, so a command that silently does nothing is indistinguishable from one that is not
// implemented. `run()` therefore never returns an empty string.
//
// THE INSPECTOR PRINTS WHAT IS THERE, NOT WHAT SHOULD BE. It walks the component pointers on a
// GameObject and reports the ones that are non-null. An object missing its Transform is a real
// state this engine can be in -- Day 8's world is built out of exactly that -- so an inspector
// that assumed every object has one would crash on the object you most wanted to look at.

#ifndef KEEPSTONE_TOOLS_H
#define KEEPSTONE_TOOLS_H

#include "keepstone/World.h"

#include <string>
#include <vector>

namespace keepstone {

class Console
{
public:
    explicit Console(World& world) : mWorld(world) {}

    // Runs one command line and returns what it printed. Never empty: see the header.
    std::string run(const std::string& line);

    // Everything run so far, in order, each as "> command" then its reply. This is what the
    // book photographs and what a bug report should carry.
    const std::vector<std::string>& transcript() const { return mTranscript; }

    // The object the last successful `select` named, or a null handle.
    Handle selected() const { return mSelected; }

    // Set by `gizmo <translate|rotate|scale|off>` so the caller can ask the renderer for it.
    // The console does not touch the renderer itself -- it has no business knowing there is one.
    const std::string& gizmoRequest() const { return mGizmo; }

private:
    std::string inspect(Handle h) const;

    World&      mWorld;
    Handle      mSelected;
    std::string mGizmo = "off";
    std::vector<std::string> mTranscript;
};

}  // namespace keepstone

#endif
