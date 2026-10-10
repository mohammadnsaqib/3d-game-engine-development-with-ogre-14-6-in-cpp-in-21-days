// Keepstone -- Prefab.cpp

#include "Prefab.h"

#include <fstream>
#include <sstream>

namespace keepstone {

bool Prefab::loadFile(const std::string& path, std::string& error)
{
    std::ifstream in(path.c_str());
    if (!in)
    {
        error = "cannot open " + path;
        return false;
    }

    mDefs.clear();
    std::string line;
    int lineNo = 0;
    Definition* current = nullptr;

    while (std::getline(in, line))
    {
        ++lineNo;

        // Comments and blank lines, stripped first so every rule below can assume real content.
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);

        std::istringstream ls(line);
        std::string word;
        if (!(ls >> word)) continue;

        if (word == "object")
        {
            Definition d;
            if (!(ls >> d.name))
            {
                error = "line " + std::to_string(lineNo) + ": object needs a name";
                return false;
            }
            mDefs.push_back(d);
            current = &mDefs.back();
            continue;
        }

        // EVERY DIRECTIVE BELOW NEEDS AN OBJECT TO BELONG TO. Saying so by name beats letting
        // the first stray line write into whatever happens to be last.
        if (!current)
        {
            error = "line " + std::to_string(lineNo) + ": '" + word +
                    "' before any object was declared";
            return false;
        }

        if      (word == "at")       ls >> current->position.x >> current->position.y
                                        >> current->position.z;
        else if (word == "scale")    ls >> current->scale.x >> current->scale.y
                                        >> current->scale.z;
        else if (word == "yaw")      ls >> current->yaw;
        else if (word == "tint")     ls >> current->tint.r >> current->tint.g >> current->tint.b;
        else if (word == "glow")   { current->glow = true; current->visual = false;
                                     ls >> current->tint.r >> current->tint.g >> current->tint.b; }
        else if (word == "invisible") current->visual = false;
        else if (word == "moves")  { current->mover = true;
                                     ls >> current->velocity.x >> current->velocity.y
                                        >> current->velocity.z; }
        else if (word == "repeat") { ls >> current->count >> current->spacing.x
                                        >> current->spacing.y >> current->spacing.z; }
        else
        {
            // AN UNKNOWN DIRECTIVE IS AN ERROR, NOT A SHRUG. A file format that ignores what it
            // does not understand is a format where a typo costs an afternoon: the level looks
            // wrong, nothing is reported, and the misspelled line looks perfectly fine.
            error = "line " + std::to_string(lineNo) + ": unknown directive '" + word + "'";
            return false;
        }
    }
    return true;
}

std::vector<Handle> Prefab::spawnAll(World& world) const
{
    std::vector<Handle> made;
    for (const Definition& d : mDefs)
    {
        for (int i = 0; i < d.count; ++i)
        {
            Handle h = world.create(d.count > 1 ? d.name + std::to_string(i) : d.name);
            GameObject* o = world.get(h);

            o->transform = new Transform();
            o->transform->position.x = d.position.x + d.spacing.x * i;
            o->transform->position.y = d.position.y + d.spacing.y * i;
            o->transform->position.z = d.position.z + d.spacing.z * i;
            o->transform->scale = d.scale;
            o->transform->yaw = d.yaw;

            if (d.visual) { o->visual = new Visual(); o->visual->tint = d.tint; }
            if (d.glow)   { o->glow = new Glow(); o->glow->light = d.tint; }
            if (d.mover)  { o->mover = new Mover(); o->mover->velocity = d.velocity; }

            made.push_back(h);
        }
    }
    return made;
}

int Prefab::totalObjects() const
{
    int n = 0;
    for (const Definition& d : mDefs) n += d.count;
    return n;
}

}  // namespace keepstone
