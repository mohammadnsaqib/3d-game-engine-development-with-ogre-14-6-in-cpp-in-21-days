// The Lantern Keep -- Day 18. Writing the world down and reading it back.
//
// Every scene here does the same three things in a different order: build a world, put it
// through `saveWorld`/`loadWorld`, and DIFF the result against the original. The diff is the
// point. A save format is correct when a world survives the round trip unchanged, and "unchanged"
// is a claim until something compares the two worlds field by field.
//
// THE PICTURE IS THE REPORT. Five bars along the front of the courtyard carry the numbers out of
// `SaveReport` and `diffWorlds` -- objects, differences, migrated fields, skipped keys and a
// status code. Two scenes that end with the same courtyard still produce different pictures,
// because the thing being photographed is what the loader DID, not what the world looks like.

#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"
#include "keepstone/Save.h"
#include "keepstone/World.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "built",        // the keep as the game builds it, before any file exists
    "moved",        // sixty ticks later: the warden has walked, the world has changed
    "round_trip",   // that world saved and loaded into a FRESH world: zero differences
    "diff_seen",    // one object nudged after loading, so the differ has something to find
    "v1_file",      // an older save with no `scale` key: migrated forward on load
    "unknown_key",  // a key this build does not know: stepped over, counted, not fatal
    "holes",        // four objects destroyed before saving: the reload compacts
    "future_file",  // version 99: REFUSED, and the world stays empty
    "truncated",    // the trailer cut off: REFUSED, and the world stays empty
};

// Build the keep. Nine wall blocks, four lanterns and a warden -- fourteen objects, the same
// fourteen Day 12 assembled from a prefab.
void buildKeep(keepstone::World& w)
{
    for (int i = 0; i < 9; ++i)
    {
        keepstone::Handle h = w.create("wall" + std::to_string(i));
        keepstone::GameObject* o = w.get(h);
        o->transform = new keepstone::Transform();
        o->transform->position = { -320.0f + 80.0f * i, 40.0f, -190.0f };
        o->transform->scale    = { 0.17f, 0.36f + 0.06f * (i % 3), 0.17f };
        o->visual = new keepstone::Visual();
        o->visual->tint = { 0.62f, 0.60f, 0.55f };
    }
    for (int i = 0; i < 4; ++i)
    {
        keepstone::Handle h = w.create("lantern" + std::to_string(i));
        keepstone::GameObject* o = w.get(h);
        o->transform = new keepstone::Transform();
        o->transform->position = { -240.0f + 160.0f * i, 96.0f, -60.0f };
        o->transform->scale    = { 0.09f, 0.09f, 0.09f };
        o->visual = new keepstone::Visual();
        o->glow = new keepstone::Glow();
        o->glow->light = { 1.0f, 0.88f, 0.66f };
    }
    keepstone::Handle h = w.create("warden");
    keepstone::GameObject* o = w.get(h);
    o->transform = new keepstone::Transform();
    o->transform->position = { -150.0f, 20.0f, 0.0f };
    o->transform->scale    = { 0.13f, 0.40f, 0.13f };
    o->visual = new keepstone::Visual();
    o->mover = new keepstone::Mover();
    o->mover->velocity = { 70.0f, 0.0f, 0.0f };
}

// Sixty ticks of the Day 6 loop, so the saved world is not the world the game started with.
void run(keepstone::World& w, int ticks)
{
    const float dt = 0.02f;
    for (int t = 0; t < ticks; ++t)
        for (keepstone::Handle h : w.all())
        {
            keepstone::GameObject* o = w.get(h);
            if (o && o->mover && o->transform)
                o->transform->moveTo({ o->transform->position.x + o->mover->velocity.x * dt,
                                       o->transform->position.y,
                                       o->transform->position.z });
        }
}

// Rewrite a save file on disk to make one of the failure scenes. Text is the format, so every
// one of these is a few lines rather than a bit-twiddling exercise -- which is itself an
// argument for the format.
void damage(const std::string& path, const std::string& how)
{
    std::ifstream in(path.c_str());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    in.close();

    std::ofstream out(path.c_str(), std::ios::trunc);
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        if (how == "version" && i == 0) { out << "keepstone-save 99\n"; continue; }
        if (how == "truncate" && lines[i].rfind("end-of-save", 0) == 0) continue;
        if (how == "v1")
        {
            if (i == 0) { out << "keepstone-save 1\n"; continue; }
            if (lines[i].find("  scale ") == 0) continue;   // v1 had no scale key
        }
        out << lines[i] << "\n";
        if (how == "unknown" && lines[i].find("  name ") == 0)
            // a key from a build that is not this one
            out << "  sparkle 3\n";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "round_trip";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }
    renderer.setAmbient({0.19f, 0.19f, 0.24f});
    renderer.setCamera({0.0f, 250.0f, 520.0f}, {0.0f, 20.0f, 0.0f});

    keepstone::World original;
    buildKeep(original);
    if (scene != "built") run(original, 60);

    if (scene == "holes")
        for (int i = 1; i < 9; i += 2)      // four of the nine walls
            original.destroy(original.find("wall" + std::to_string(i)));

    const std::string path = "keep.save";
    std::string saveError;
    // `built` and `moved` write nothing: they are the two BEFORE pictures, and a scene that
    // saves and reloads produces the same courtyard as `round_trip` with the same report bars,
    // which is to say the same PNG. The first version of this file saved in `moved` and the two
    // frames came out byte-identical.
    const bool wrote = (scene == "built" || scene == "moved")
                     ? false : saveWorld(original, path, saveError);

    if (scene == "future_file")   damage(path, "version");
    if (scene == "truncated")     damage(path, "truncate");
    if (scene == "v1_file")       damage(path, "v1");
    if (scene == "unknown_key")   damage(path, "unknown");

    keepstone::World reloaded;
    keepstone::SaveReport report;
    if (wrote) report = loadWorld(reloaded, path);

    if (scene == "diff_seen")
    {
        // Nudge one object AFTER the load, so the differ has exactly one thing to find. This is
        // the control for `round_trip`: a differ that reports zero differences has only proved
        // something if it can also report one.
        keepstone::GameObject* o = reloaded.get(reloaded.find("lantern2"));
        if (o && o->transform) o->transform->moveTo({ o->transform->position.x + 60.0f,
                                                      o->transform->position.y,
                                                      o->transform->position.z });
    }

    std::vector<std::string> diffLines;
    const int differences = report.ok ? diffWorlds(original, reloaded, diffLines) : 0;

    // -- draw whichever world the scene is about -----------------------------------------------
    keepstone::World& shown = (!wrote || !report.ok) ? original : reloaded;
    if (!(scene == "future_file" || scene == "truncated"))
        for (keepstone::Handle h : shown.all())
        {
            keepstone::GameObject* o = shown.get(h);
            if (!o || !o->transform) continue;
            renderer.addBlock(o->transform->position, o->transform->scale, o->transform->yaw);
            if (o->glow)
                renderer.addLantern({ o->transform->position.x, o->transform->position.y + 20.0f,
                                      o->transform->position.z }, o->glow->light);
        }

    // -- the report, as five bars --------------------------------------------------------------
    // Objects, differences, migrated fields, skipped keys, status. A scene whose courtyard looks
    // like another scene's still differs here, because this is a picture of what the LOADER did.
    // THE REFUSAL CODE, NOT A BARE "IT FAILED". `future_file` and `truncated` both end with an
    // empty world and both used to draw an identical status bar, so two different refusals were
    // one picture. The code makes the reason itself drawable.
    const int status = report.ok ? 1 : (10 + static_cast<int>(report.refusal));
    const int values[5] = { report.objects, differences, report.migrated, report.skippedKeys,
                            status };
    for (int i = 0; i < 5; ++i)
    {
        const float h = 0.04f + 0.055f * static_cast<float>(values[i]);
        renderer.addBlock({ -180.0f + 90.0f * i, 20.0f + h * 45.0f, 150.0f },
                          { 0.13f, h, 0.13f }, 0.0f);
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("original objects %d\n", original.livingCount());
    std::printf("save written     %s\n", wrote ? path.c_str() : "(none)");
    std::printf("load ok          %s\n", report.ok ? "yes" : "no");
    if (!report.ok && wrote)
        std::printf("load error       %s\n", report.error.c_str());
    if (report.ok)
    {
        std::printf("file version     %d  (this build reads %d)\n", report.version,
                    keepstone::SAVE_VERSION);
        std::printf("objects loaded   %d\n", report.objects);
        std::printf("migrated fields  %d\n", report.migrated);
        std::printf("skipped keys     %d\n", report.skippedKeys);
        std::printf("differences      %d\n", differences);
        for (const std::string& d : diffLines) std::printf("  %s\n", d.c_str());
    }

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    return 0;
}
