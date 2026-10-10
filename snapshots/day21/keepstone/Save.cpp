// Keepstone -- Save.cpp  (Day 18)

#include "keepstone/Save.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace keepstone {

namespace {

// The tolerance a round trip is allowed. Positions are printed with six decimals, so a float
// survives the trip exactly in practice -- but comparing floats with == in a test that is
// SUPPOSED to pass is how a test becomes flaky on a compiler you have not met yet.
const float EPS = 1.0e-4f;

bool near(float a, float b) { return std::fabs(a - b) < EPS; }

bool nearVec(const Vec3& a, const Vec3& b)
{
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

// One object as it came out of the file, before any world is touched.
struct Parsed
{
    std::string name;
    Vec3   position;
    Vec3   scale { 0.3f, 0.5f, 0.3f };
    float  yaw = 0.0f;
    Colour tint { 1.0f, 1.0f, 1.0f };
    bool   drawn = true;
    bool   hasGlow = false;
    Colour glow { 1.0f, 0.88f, 0.66f };
    bool   hasMover = false;
    Vec3   velocity;
    bool   sawScale = false;   // so migrate() knows whether this came from a v1 file
};

// THE ONLY MIGRATION IN THIS BOOK, and it is deliberately trivial, because the point is the
// SHAPE of a migration and not its cleverness: version 1 had no `scale` key, so a v1 object gets
// the default one. Written as a function taking the version so that adding a second migration
// means adding a second `if`, in order, each one moving the data forward exactly one step.
int migrate(std::vector<Parsed>& objects, int from)
{
    int filled = 0;
    if (from <= 1)
    {
        for (Parsed& p : objects)
            if (!p.sawScale) { p.scale = Vec3{ 0.3f, 0.5f, 0.3f }; ++filled; }
    }
    return filled;
}

}  // namespace

bool saveWorld(const World& world, const std::string& path, std::string& error)
{
    std::ofstream out(path.c_str(), std::ios::trunc);
    if (!out) { error = "cannot write " + path; return false; }

    World& w = const_cast<World&>(world);   // all() and get() are non-const; nothing is modified
    std::vector<Handle> handles = w.all();

    out << "keepstone-save " << SAVE_VERSION << "\n";
    out << "objects " << handles.size() << "\n";

    for (Handle h : handles)
    {
        GameObject* o = w.get(h);
        if (!o) continue;
        out << "object\n";
        out << "  name " << o->name << "\n";
        if (o->transform)
        {
            const Transform& t = *o->transform;
            out << "  pos " << t.position.x << " " << t.position.y << " " << t.position.z << "\n";
            out << "  scale " << t.scale.x << " " << t.scale.y << " " << t.scale.z << "\n";
            out << "  yaw " << t.yaw << "\n";
        }
        if (o->visual)
        {
            out << "  tint " << o->visual->tint.r << " " << o->visual->tint.g << " "
                << o->visual->tint.b << "\n";
            out << "  drawn " << (o->visual->drawn ? 1 : 0) << "\n";
        }
        if (o->glow)
            out << "  glow " << o->glow->light.r << " " << o->glow->light.g << " "
                << o->glow->light.b << "\n";
        if (o->mover)
            out << "  velocity " << o->mover->velocity.x << " " << o->mover->velocity.y << " "
                << o->mover->velocity.z << "\n";
        out << "end\n";
    }
    // THE TRAILER IS WHAT MAKES TRUNCATION DETECTABLE. Without it a file that stopped early is
    // just a file with fewer objects in it, and there is no way to tell that from a world that
    // genuinely had fewer objects.
    out << "end-of-save " << handles.size() << "\n";
    return true;
}

SaveReport loadWorld(World& world, const std::string& path)
{
    SaveReport r;

    if (world.livingCount() != 0)
    {
        r.error = "refusing to load into a world that already has " +
                  std::to_string(world.livingCount()) + " object(s)";
        r.refusal = SaveRefusal::WorldNotEmpty;
        return r;
    }

    std::ifstream in(path.c_str());
    if (!in)
    {
        r.error = "cannot open " + path;
        r.refusal = SaveRefusal::CannotOpen;
        return r;
    }

    std::string word;
    if (!(in >> word) || word != "keepstone-save")
    {
        r.error = "not a keepstone save file";
        r.refusal = SaveRefusal::NotASaveFile;
        return r;
    }
    if (!(in >> r.version))
    {
        r.error = "save file has no version";
        r.refusal = SaveRefusal::NoVersion;
        return r;
    }
    if (r.version > SAVE_VERSION)
    {
        // See the header. A newer file read by an older reader loses whatever is new, silently.
        r.error = "save is version " + std::to_string(r.version) + "; this build reads up to " +
                  std::to_string(SAVE_VERSION);
        r.refusal = SaveRefusal::NewerVersion;
        return r;
    }

    std::vector<Parsed> parsed;
    int declared = -1, trailer = -1;
    std::string line;
    std::getline(in, line);   // finish the version line

    while (std::getline(in, line))
    {
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;

        if (key == "objects")      { ls >> declared; continue; }
        if (key == "end-of-save")  { ls >> trailer;  continue; }
        if (key == "object")       { parsed.push_back(Parsed()); continue; }
        if (key == "end")          { continue; }

        if (parsed.empty()) { ++r.skippedKeys; continue; }
        Parsed& p = parsed.back();

        if      (key == "name")     ls >> p.name;
        else if (key == "pos")      ls >> p.position.x >> p.position.y >> p.position.z;
        else if (key == "scale")  { ls >> p.scale.x >> p.scale.y >> p.scale.z; p.sawScale = true; }
        else if (key == "yaw")      ls >> p.yaw;
        else if (key == "tint")     ls >> p.tint.r >> p.tint.g >> p.tint.b;
        else if (key == "drawn")  { int d = 1; ls >> d; p.drawn = (d != 0); }
        else if (key == "glow")   { ls >> p.glow.r >> p.glow.g >> p.glow.b; p.hasGlow = true; }
        else if (key == "velocity")
        { ls >> p.velocity.x >> p.velocity.y >> p.velocity.z; p.hasMover = true; }
        else
        {
            // A key from a future minor change, or a typo. Counted and stepped over: see the
            // header on why this is not the same decision as refusing a newer VERSION.
            ++r.skippedKeys;
        }
    }

    if (trailer < 0)
    {
        r.error = "save file ends without its trailer -- truncated";
        r.refusal = SaveRefusal::Truncated;
        return r;
    }
    if (declared >= 0 && declared != static_cast<int>(parsed.size()))
    {
        r.refusal = SaveRefusal::CountMismatch;
        r.error = "header says " + std::to_string(declared) + " object(s), file holds " +
                  std::to_string(parsed.size());
        return r;
    }
    if (trailer != static_cast<int>(parsed.size()))
    {
        r.refusal = SaveRefusal::CountMismatch;
        r.error = "trailer says " + std::to_string(trailer) + " object(s), file holds " +
                  std::to_string(parsed.size());
        return r;
    }

    r.migrated = migrate(parsed, r.version);

    // NOTHING TOUCHED THE WORLD UNTIL HERE. Every refusal above returns with the world exactly
    // as it was found -- the same discipline as Day 17's behaviour loader, and for the same
    // reason: a half-loaded world is worse than no world, because it looks like one.
    for (const Parsed& p : parsed)
    {
        Handle h = world.create(p.name);
        GameObject* o = world.get(h);
        o->transform = new Transform();
        o->transform->position = p.position;
        o->transform->scale    = p.scale;
        o->transform->yaw      = p.yaw;
        o->visual = new Visual();
        o->visual->tint  = p.tint;
        o->visual->drawn = p.drawn;
        if (p.hasGlow)  { o->glow = new Glow();  o->glow->light = p.glow; }
        if (p.hasMover) { o->mover = new Mover(); o->mover->velocity = p.velocity; }
    }

    r.objects = static_cast<int>(parsed.size());
    r.ok = true;
    return r;
}

int diffWorlds(World& a, World& b, std::vector<std::string>& lines)
{
    int differences = 0;

    std::vector<Handle> ah = a.all();
    std::vector<Handle> bh = b.all();

    if (ah.size() != bh.size())
    {
        lines.push_back("object count " + std::to_string(ah.size()) + " vs " +
                        std::to_string(bh.size()));
        ++differences;
    }

    for (Handle h : ah)
    {
        GameObject* x = a.get(h);
        if (!x) continue;

        Handle other = b.find(x->name);
        GameObject* y = b.get(other);
        if (!y)
        {
            lines.push_back(x->name + ": missing after reload");
            ++differences;
            continue;
        }
        if (x->transform && y->transform)
        {
            if (!nearVec(x->transform->position, y->transform->position))
            {
                lines.push_back(x->name + ": position moved");
                ++differences;
            }
            if (!nearVec(x->transform->scale, y->transform->scale))
            {
                lines.push_back(x->name + ": scale changed");
                ++differences;
            }
            if (!near(x->transform->yaw, y->transform->yaw))
            {
                lines.push_back(x->name + ": yaw changed");
                ++differences;
            }
        }
        const bool xg = (x->glow != nullptr), yg = (y->glow != nullptr);
        if (xg != yg)
        {
            lines.push_back(x->name + std::string(": glow ") + (xg ? "lost" : "gained"));
            ++differences;
        }
    }
    return differences;
}

}  // namespace keepstone
