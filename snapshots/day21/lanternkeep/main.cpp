// The Lantern Keep -- Day 21. Shipping: a folder somebody else can unzip and run.
//
// The last day does not add a feature. It answers the question the other twenty have
// been quietly deferring: what do you actually HAND somebody?
//
// HOW THIS PROGRAM PHOTOGRAPHS A PACKAGING FAILURE. A program that will not start
// cannot render its own picture, so the scenes here do not fail -- they build a folder,
// LAUNCH the shipped executable inside it as a separate process, and photograph the
// result. Every frame is the outcome of a real launch: files staged, exit code, and
// whether the thing came up.
//
// `--selftest` is what the staged copy is run with. It starts the engine, prints one
// line and exits, which is exactly the part of the program that depends on the folder
// being right.
//
// THE FALLBACK IS THE VILLAIN OF THIS CHAPTER. `Renderer.cpp` looks beside the
// executable first and falls back to the SDK path compiled in at build time. On this
// machine that fallback finds a real SDK, so a folder missing every one of its media
// files still runs -- and lies. Setting `LK_STRICT` turns the fallback off, which is
// the only honest way to test a package on the machine that built it. `plus_plugins`
// and `dev_fallback` stage the SAME folder and differ only in whether LK_STRICT is set;
// one fails and one does not, and the one that does not is the one lying to you.

#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"

#include <cstdio>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

const char* SCENES[] = {
    "exe_only",      // the executable alone, strict: it cannot even start
    "plus_core",     // + the engine libraries: starts, then cannot find a render system
    "plus_plugins",  // + the render system and codec: starts, then cannot find media
    "plus_media",    // + the engine media: comes up
    "cooked",        // everything cook.py copies: comes up
    "missing_one",   // the cooked folder with ONE library deleted (the shader system)
    "moved",         // the cooked folder renamed and run from a different path
    "dev_fallback",  // plus_plugins WITHOUT strict: it runs anyway, and that is the bug
};

const char* CORE_DLLS[] = { "OgreMain.dll", "OgreRTShaderSystem.dll", "OgreBites.dll",
                            "OgreOverlay.dll", "z.dll" };
const char* PLUGIN_DLLS[] = { "RenderSystem_Direct3D11.dll", "Codec_STBI.dll" };

// WHERE THE COOK TOOL'S PATHS COME FROM, and why they are not compiled in.
//
// The first version of this file wrote `LK_OGRE_MEDIA` and did not compile, because
// that is a PRIVATE compile definition on the `keepstone` target -- the Day 4 boundary
// refusing a game file that reached for the renderer's business. It was right to
// refuse. A cook tool does need those paths, so CMake writes them beside the executable
// at configure time and the tool reads them at run time. The paths belong to the build,
// not to the program.
std::string cookPath(const std::string& key, const std::string& fromDir)
{
    std::ifstream in((fromDir + "/cook.cfg").c_str());
    std::string line;
    while (std::getline(in, line))
    {
        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos) continue;
        if (line.substr(0, eq) == key)
        {
            std::string v = line.substr(eq + 1);
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
            return v;
        }
    }
    return std::string();
}

bool copyFile(const std::string& from, const std::string& to)
{
    std::ifstream in(from.c_str(), std::ios::binary);
    if (!in) return false;
    std::ofstream out(to.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << in.rdbuf();
    return true;
}

bool exists(const std::string& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

void makeDir(const std::string& p) { _mkdir(p.c_str()); }

// Recursively copy a directory using the shell, because this is a book about engines
// and not about directory walking on Windows. The cook script does it properly in
// Python.
bool copyTree(const std::string& from, const std::string& to)
{
    const std::string cmd =
        "xcopy /E /I /Q /Y \"" + from + "\" \"" + to + "\" >nul 2>&1";
    return std::system(cmd.c_str()) == 0;
}

int countFiles(const std::string& dir)
{
    const std::string listing = dir + "\\__count.txt";
    std::system(("dir /S /B /A-D \"" + dir + "\" > \"" + listing + "\" 2>nul").c_str());
    std::ifstream in(listing.c_str());
    int n = 0;
    std::string line;
    while (std::getline(in, line)) if (!line.empty()) ++n;
    in.close();
    std::remove(listing.c_str());
    return n > 0 ? n - 1 : 0;   // the listing file counts itself
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "cooked";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        // THE STAGED COPY IS RUN WITH THIS. It is the smallest thing that still proves
        // the folder is complete: the engine either comes up or it does not.
        if (a == "--selftest")
        {
            keepstone::Renderer renderer;
            renderer.setSize(64, 64);
            keepstone::Engine engine;
            engine.add(&renderer);
            if (!engine.startUp())
            {
                std::printf("selftest FAILED to start\n");
                return 2;
            }
            // STARTING IS NOT ENOUGH, and assuming it was cost this harness a rewrite.
            // OGRE comes up perfectly well with no media at all -- a missing resource
            // location is a warning, not an error -- so a selftest that only called
            // startUp() reported a folder with no materials in it as good, and
            // `plus_plugins` "passed" while being exactly the broken package the scene
            // exists to show. It has to DRAW something.
            renderer.setCamera({0.0f, 40.0f, 120.0f}, {0.0f, 0.0f, 0.0f});
            renderer.addBlock({0.0f, 0.0f, 0.0f}, {0.2f, 0.2f, 0.2f}, 0.0f);
            renderer.renderTo("selftest.png");
            std::printf("selftest ok\n");
            engine.shutDown();
            return 0;
        }
        if (a == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    // Where CMake put the executable and the DLLs beside it.
    std::string from = ".";
    {
        const std::string self = argv[0];
        const std::string::size_type slash = self.find_last_of("\\/");
        if (slash != std::string::npos) from = self.substr(0, slash);
    }

    const std::string stage = "stage_" + scene;
    std::system(("rmdir /S /Q \"" + stage + "\" >nul 2>&1").c_str());
    makeDir(stage);

    int staged = 0;
    if (copyFile(from + "/lanternkeep.exe", stage + "/lanternkeep.exe")) ++staged;

    // `dev_fallback` STAGES EXACTLY WHAT `plus_plugins` STAGES. The two differ in one
    // thing only: whether LK_STRICT is set. That is what makes the pair worth
    // photographing -- same folder, same files, one comes up and one does not, and the
    // one that comes up is the one lying to you.
    //
    // An earlier version staged the executable ALONE for this scene and expected the
    // fallback to rescue it. It cannot: the fallback chooses where to look for plugins
    // and media, and an executable with no OgreMain.dll beside it fails in the loader
    // before a line of our code runs. The fallback hides missing DATA, not missing
    // LIBRARIES.
    const bool wantCore    = (scene != "exe_only");
    const bool wantPlugins = (scene == "plus_plugins" || scene == "plus_media" ||
                              scene == "cooked" || scene == "missing_one" ||
                              scene == "moved" || scene == "dev_fallback");
    const bool wantMedia   = (scene == "plus_media" || scene == "cooked" ||
                              scene == "missing_one" || scene == "moved");
    const bool wantGame    = (scene == "cooked" || scene == "missing_one" ||
                              scene == "moved");

    if (wantCore)
        for (const char* d : CORE_DLLS)
        {
            // `missing_one` is the cooked folder with exactly one library left out. It
            // is the single commonest way a package reaches somebody broken, and the
            // message it produces names a plugin rather than the library that is
            // actually absent.
            if (scene == "missing_one" &&
                std::string(d) == "OgreRTShaderSystem.dll") continue;
            if (copyFile(from + "/" + d, stage + "/" + d)) ++staged;
        }
    if (wantPlugins)
        for (const char* d : PLUGIN_DLLS)
            if (copyFile(from + "/" + d, stage + "/" + d)) ++staged;

    if (wantMedia)
    {
        makeDir(stage + "/media");
        const std::string om = cookPath("ogre_media", from);
        if (copyTree(om + "/Main", stage + "/media/Main"))               ++staged;
        if (copyTree(om + "/RTShaderLib", stage + "/media/RTShaderLib")) ++staged;
    }
    if (wantGame)
    {
        if (copyTree(cookPath("game_media", from), stage + "/gamemedia")) ++staged;
        if (copyTree(cookPath("game_data", from), stage + "/data"))       ++staged;
    }

    std::string runDir = stage;
    if (scene == "moved")
    {
        // A cooked folder that only works where it was cooked has not been cooked.
        // Renaming it is the cheapest possible test of that, and it catches an absolute
        // path immediately.
        const std::string moved = "shipped_elsewhere";
        std::system(("rmdir /S /Q \"" + moved + "\" >nul 2>&1").c_str());
        std::system(("move \"" + stage + "\" \"" + moved + "\" >nul 2>&1").c_str());
        runDir = moved;
    }

    // -- launch it ----------------------------------------------------------------
    const std::string logFile = "launch_" + scene + ".txt";
    // LAUNCHED FROM INSIDE THE FOLDER, and that `cd` is not cosmetic.
    //
    // The first version ran the staged executable by full path while leaving the
    // working directory in the build tree -- and Windows searches the current directory
    // for DLLs. So `missing_one`, a folder with the shader system deliberately deleted,
    // started perfectly: it was loading the copy sitting in the build output next door.
    // The test reported a broken package as good, which is precisely the failure this
    // whole chapter is about, reproduced inside the harness meant to detect it.
    //
    // Running from inside the folder is the closest this machine can get to being
    // somebody else's. It is still not a clean machine -- see the chapter on what that
    // does and does not prove.
    const std::string strict = (scene == "dev_fallback") ? "" : "set LK_STRICT=1&& ";
    // No outer `cmd /c "..."` wrapper: std::system already runs this through cmd, and
    // wrapping it meant nested quotes that cmd stripped in the wrong order -- every
    // scene then reported `'lanternkeep.exe' is not recognized`, which reads exactly
    // like a missing executable and was a quoting bug. None of these paths contain
    // spaces, so none of them need quoting.
    const std::string cmd = "cd /d " + runDir + " && " + strict +
                            ".\\lanternkeep.exe --selftest > " + logFile + " 2>&1";
    const int rc = std::system(cmd.c_str());

    std::string firstLine = "(no output)";
    {
        std::ifstream in((runDir + "/" + logFile).c_str());
        std::string line;
        while (std::getline(in, line))
            if (!line.empty()) { firstLine = line; break; }
    }
    const bool cameUp = (rc == 0 && firstLine.find("selftest ok") != std::string::npos);
    const int filesInFolder = countFiles(runDir);

    // -- the picture --------------------------------------------------------------
    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }
    renderer.setAmbient({0.20f, 0.20f, 0.25f});
    renderer.setCamera({0.0f, 250.0f, 520.0f}, {0.0f, 40.0f, 0.0f});

    // Four bars: files in the folder, libraries staged, whether it came up, and the
    // strictness. A launch that failed and a launch that succeeded are different
    // pictures even when the folder looks similar, which is the only way these scenes
    // can be told apart.
    const int values[5] = { filesInFolder / 4, staged, cameUp ? 12 : 0,
                            (scene == "dev_fallback") ? 0 : 6,
                            (scene == "moved") ? 9 : 0 };
    for (int i = 0; i < 5; ++i)
    {
        const float h = 0.05f + 0.06f * static_cast<float>(values[i]);
        renderer.addBlock({ -180.0f + 90.0f * i, 20.0f + h * 45.0f, 150.0f },
                          { 0.15f, h, 0.15f }, 0.0f);
    }
    // The keep itself, drawn only when the staged copy actually came up.
    if (cameUp)
        for (int i = 0; i < 9; ++i)
            renderer.addBlock({ -320.0f + 80.0f * i, 60.0f, -190.0f },
                              { 0.17f, 0.40f, 0.17f }, 0.0f);

    std::printf("scene            %s\n", scene.c_str());
    std::printf("strict           %s\n",
                (scene == "dev_fallback") ? "no -- fallback allowed"
                                           : "yes -- beside the exe only");
    std::printf("files staged     %d item(s), %d file(s) in the folder\n",
                staged, filesInFolder);
    std::printf("exit code        %d\n", rc);
    std::printf("first line       %s\n", firstLine.c_str());
    std::printf("came up          %s\n", cameUp ? "yes" : "no");

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    return 0;
}
