// Keepstone -- Tools.cpp  (Day 19)

#include "keepstone/Tools.h"

#include <sstream>

namespace keepstone {

namespace {

std::string fmt(float v)
{
    std::ostringstream o;
    o.precision(1);
    o << std::fixed << v;
    return o.str();
}

std::string vecText(const Vec3& v)
{
    return fmt(v.x) + " " + fmt(v.y) + " " + fmt(v.z);
}

}  // namespace

std::string Console::inspect(Handle h) const
{
    World& w = const_cast<World&>(mWorld);
    GameObject* o = w.get(h);
    if (!o) return "nothing selected";

    // COMPONENT BY COMPONENT, AND ONLY THE ONES THAT ARE THERE. An object with no Transform is
    // a legal object in this engine; printing "position: (null)" would be a lie dressed as data.
    std::string s = o->name;
    s += "\n  components:";
    if (o->transform) s += " transform";
    if (o->visual)    s += " visual";
    if (o->mover)     s += " mover";
    if (o->glow)      s += " glow";
    if (!o->transform && !o->visual && !o->mover && !o->glow) s += " (none)";
    if (o->transform)
    {
        s += "\n  position " + vecText(o->transform->position);
        s += "\n  scale    " + vecText(o->transform->scale);
        s += "\n  dirty    " + std::string(o->transform->dirty ? "yes" : "no");
    }
    if (o->mover) s += "\n  velocity " + vecText(o->mover->velocity);
    return s;
}

std::string Console::run(const std::string& line)
{
    std::istringstream in(line);
    std::string verb;
    in >> verb;

    std::string reply;

    if (verb.empty())
    {
        reply = "nothing to do";
    }
    else if (verb == "list")
    {
        std::vector<Handle> all = mWorld.all();
        reply = std::to_string(all.size()) + " object(s)";
        for (Handle h : all)
        {
            GameObject* o = mWorld.get(h);
            if (o) reply += "\n  " + o->name;
        }
    }
    else if (verb == "select" || verb == "inspect")
    {
        std::string name;
        in >> name;
        Handle h = mWorld.find(name);
        if (mWorld.get(h) == nullptr)
        {
            // NAMING A MISSING OBJECT IS THE COMMONEST MISTAKE AT A CONSOLE, so it gets a real
            // answer rather than silence. The selection is left alone: a typo should not throw
            // away what you had selected.
            reply = "no object named '" + name + "'";
        }
        else
        {
            mSelected = h;
            reply = (verb == "select") ? ("selected " + name) : inspect(h);
        }
    }
    else if (verb == "move" || verb == "tint")
    {
        std::string name;
        float a = 0.0f, b = 0.0f, c = 0.0f;
        in >> name >> a >> b >> c;
        Handle h = mWorld.find(name);
        GameObject* o = mWorld.get(h);
        if (!o) reply = "no object named '" + name + "'";
        else if (verb == "move")
        {
            if (!o->transform) reply = name + " has no transform to move";
            else
            {
                // moveTo, NOT position = . Day 10's dirty flag is set by the setter and a
                // console that assigned the field directly would move the object in the world
                // and leave the picture behind -- the exact defect Day 10 photographs.
                o->transform->moveTo(Vec3{ a, b, c });
                reply = name + " moved to " + vecText(o->transform->position);
            }
        }
        else
        {
            if (!o->visual) reply = name + " has no visual to tint";
            else { o->visual->tint = Colour{ a, b, c }; reply = name + " tinted"; }
        }
    }
    else if (verb == "spawn")
    {
        std::string name;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        in >> name >> x >> y >> z;
        if (mWorld.get(mWorld.find(name)) != nullptr)
        {
            reply = "'" + name + "' already exists";
        }
        else
        {
            Handle h = mWorld.create(name);
            GameObject* o = mWorld.get(h);
            o->transform = new Transform();
            o->transform->position = Vec3{ x, y, z };
            o->transform->scale    = Vec3{ 0.12f, 0.30f, 0.12f };
            o->visual = new Visual();
            mSelected = h;
            reply = "spawned " + name + " at " + vecText(o->transform->position);
        }
    }
    else if (verb == "destroy")
    {
        std::string name;
        in >> name;
        Handle h = mWorld.find(name);
        if (mWorld.get(h) == nullptr) reply = "no object named '" + name + "'";
        else
        {
            mWorld.destroy(h);
            // The selection is a handle, and Day 9 made a handle answer honestly once its
            // object is gone -- so there is nothing to clear here, and clearing it would be the
            // one line that quietly reintroduces a stale pointer.
            reply = "destroyed " + name;
        }
    }
    else if (verb == "gizmo")
    {
        std::string which;
        in >> which;
        if (which != "translate" && which != "rotate" && which != "scale" && which != "off")
            reply = "gizmo takes translate, rotate, scale or off";
        else { mGizmo = which; reply = "gizmo " + which; }
    }
    else
    {
        // A CONSOLE THAT IGNORES WHAT IT DOES NOT UNDERSTAND TEACHES PEOPLE TO DISTRUST IT.
        reply = "unknown command '" + verb + "'";
    }

    mTranscript.push_back("> " + line);
    mTranscript.push_back(reply);
    return reply;
}

}  // namespace keepstone
