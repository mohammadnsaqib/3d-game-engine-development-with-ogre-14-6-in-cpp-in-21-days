// The Lantern Keep -- Day 19. Tools: a console, an inspector, and OGRE 14.6's gizmos.
//
// Every scene runs a short console session against a real world and photographs the result. The
// console is Keepstone's; the manipulator arms and the orientation cube are OGRE 14.6's, reached
// through three functions on `Renderer` that name no OGRE type.
//
// THERE IS NO WINDOW AND NO MOUSE HERE, and the gizmo scenes are still real drags. OGRE's mouse
// events are plain structs, so `Renderer::dragGizmo` synthesises the press, the motion and the
// release and hands them to the gizmo directly. What the figures show is the tool responding to
// input, not a mock-up of one.

#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"
#include "keepstone/Tools.h"
#include "keepstone/World.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "list",           // the console's view of a world nobody has touched
    "inspect",        // one object, component by component
    "inspect_bare",   // an object with no transform: the inspector says so instead of crashing
    "spawn",          // a new object typed into being
    "destroy",        // and one removed
    "move",           // a console edit that goes through the dirty flag, so the picture follows
    "unknown",        // a typo: answered, not ignored
    "gizmo_translate",// 14.6's manipulator, translate arms, on the warden
    "gizmo_rotate",   // the same manipulator in rotate
    "gizmo_scale",    // and in scale
    "gizmo_drag",     // a synthetic drag that actually moves the node
    "camera_gizmo",   // the orientation cube in the corner of the view
};

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
    }
    for (int i = 0; i < 3; ++i)
    {
        keepstone::Handle h = w.create("lantern" + std::to_string(i));
        keepstone::GameObject* o = w.get(h);
        o->transform = new keepstone::Transform();
        o->transform->position = { -200.0f + 200.0f * i, 96.0f, -60.0f };
        o->transform->scale    = { 0.09f, 0.09f, 0.09f };
        o->visual = new keepstone::Visual();
        o->glow = new keepstone::Glow();
        o->glow->light = { 1.0f, 0.88f, 0.66f };
    }
    keepstone::Handle h = w.create("warden");
    keepstone::GameObject* o = w.get(h);
    o->transform = new keepstone::Transform();
    o->transform->position = { -40.0f, 30.0f, 40.0f };
    o->transform->scale    = { 0.16f, 0.50f, 0.16f };
    o->visual = new keepstone::Visual();

    // An object with a name and nothing else. Day 8's world is full of these, and the inspector
    // has to survive one.
    w.create("marker");
}

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "list";
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
    renderer.setAmbient({0.20f, 0.20f, 0.25f});
    renderer.setCamera({0.0f, 230.0f, 480.0f}, {0.0f, 30.0f, 0.0f});

    keepstone::World world;
    buildKeep(world);
    keepstone::Console console(world);

    // -- the session ---------------------------------------------------------------------------
    std::vector<std::string> session;
    if      (scene == "list")         session.push_back("list");
    else if (scene == "inspect")      session.push_back("inspect warden");
    else if (scene == "inspect_bare") session.push_back("inspect marker");
    else if (scene == "spawn")        session.push_back("spawn brazier 140 40 40");
    else if (scene == "destroy")    { session.push_back("destroy wall4");
                                      session.push_back("list"); }
    else if (scene == "move")       { session.push_back("select warden");
                                      session.push_back("move warden 150 30 40");
                                      session.push_back("inspect warden"); }
    else if (scene == "unknown")    { session.push_back("wander warden");
                                      session.push_back("inspect wallX"); }
    else                            { session.push_back("select warden");
                                      session.push_back("gizmo " +
                                          (scene == "gizmo_rotate" ? std::string("rotate")
                                         : scene == "gizmo_scale"  ? std::string("scale")
                                         : scene == "camera_gizmo" ? std::string("off")
                                                                   : std::string("translate"))); }

    for (const std::string& c : session) console.run(c);

    // -- draw the world the console left behind -------------------------------------------------
    keepstone::ObjectId wardenId = 0;
    bool haveWarden = false;
    for (keepstone::Handle h : world.all())
    {
        keepstone::GameObject* o = world.get(h);
        if (!o || !o->transform) continue;
        const keepstone::ObjectId id =
            renderer.addBlock(o->transform->position, o->transform->scale, o->transform->yaw);
        if (o->glow)
            renderer.addLantern({ o->transform->position.x, o->transform->position.y + 24.0f,
                                  o->transform->position.z }, o->glow->light);
        if (o->name == "warden") { wardenId = id; haveWarden = true; }
    }

    // -- what the console DID, drawn ------------------------------------------------------------
    //
    // FOUR OF THESE SCENES CHANGE NOTHING IN THE WORLD. `list`, `inspect`, `inspect_bare` and
    // `unknown` all read, or fail, and leave every object exactly where it was -- so the first
    // version of this program produced four byte-identical PNGs and the chapter's evidence
    // quietly collapsed from twelve pictures to eight.
    //
    // The console's real output is TEXT, and the honest fix is not to invent a difference but to
    // draw the two things a console genuinely changes even when the world does not: what is
    // SELECTED, and how many commands were refused. Both are visible state in a real editor.
    {
        keepstone::GameObject* sel = world.get(console.selected());
        if (sel && sel->transform)
        {
            // A caret floating over the selected object.
            renderer.addBlock({ sel->transform->position.x,
                                sel->transform->position.y + 70.0f,
                                sel->transform->position.z }, { 0.07f, 0.07f, 0.07f }, 45.0f);
        }
        else if (sel)
        {
            // Selected, but with nothing to draw it at -- `marker` has no transform. An editor
            // has to be able to show you that, or an object you cannot see is an object you
            // cannot tell from one that is not there.
            renderer.addBlock({ 190.0f, 150.0f, 120.0f }, { 0.07f, 0.07f, 0.07f }, 45.0f);
        }
        int refusals = 0;
        for (const std::string& line : console.transcript())
            if (line.rfind("no object", 0) == 0 || line.rfind("unknown command", 0) == 0)
                ++refusals;
        for (int i = 0; i < refusals; ++i)
            renderer.addBlock({ -190.0f + 34.0f * i, 150.0f, 120.0f },
                              { 0.10f, 0.10f, 0.10f }, 0.0f);
    }

    // -- the tools ------------------------------------------------------------------------------
    bool dragged = false;
    keepstone::Vec3 before, after;
    if (scene == "camera_gizmo")
    {
        renderer.showCameraGizmo(true);
    }
    else if (haveWarden && console.gizmoRequest() != "off")
    {
        const std::string g = console.gizmoRequest();
        renderer.showGizmo(wardenId,
            g == "rotate" ? keepstone::Renderer::GizmoKind::Rotate
          : g == "scale"  ? keepstone::Renderer::GizmoKind::Scale
                          : keepstone::Renderer::GizmoKind::Translate);

        if (scene == "gizmo_drag")
        {
            // Grab the +x arm and pull it 120 pixels right.
            //
            // WHERE 1040 CAME FROM, because it is the sort of number that looks arbitrary and is
            // not. Projecting the warden's world position through this camera -- (0, 230, 480)
            // looking at (0, 30, 0), 45-degree vertical field of view, 2400 by 1800 -- puts it at
            // pixel (1020, 970). The arms radiate from there, so the press lands just outside the
            // centre at 1040. A first attempt guessed a point near the screen's middle, grabbed
            // no axis at all, and produced a perfectly good frame of a gizmo nobody had touched.
            //
            // THESE ARE PIXELS, SO THEY MOVE WITH THE VIEWPORT. They were (510, 485) and a press
            // at 520 while this program rendered at 1200 by 900; raising the render size for
            // print doubled every one of them. A pixel constant is a fact about a window, not
            // about a scene, and it is the first thing to break when the window changes.
            //
            // `dragGizmo` returning false is what made the original miss visible. A drag that
            // silently does nothing looks exactly like a drag along an axis that is edge-on.
            before = renderer.positionOf(wardenId);
            dragged = renderer.dragGizmo(1040.0f, 970.0f, 1280.0f, 970.0f);
            after = renderer.positionOf(wardenId);
        }
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("objects          %d living, %d slot(s)\n", world.livingCount(), world.slotCount());
    std::printf("gizmo            %s\n",
                scene == "camera_gizmo" ? "camera orientation cube" : console.gizmoRequest().c_str());
    if (scene == "gizmo_drag")
    {
        std::printf("drag grabbed     %s\n", dragged ? "yes" : "no -- the press hit no axis");
        std::printf("warden x         %.1f before, %.1f after\n", before.x, after.x);
    }
    std::printf("console session  %d command(s)\n", static_cast<int>(session.size()));
    for (const std::string& line : console.transcript())
        std::printf("%s\n", line.c_str());

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    return 0;
}
