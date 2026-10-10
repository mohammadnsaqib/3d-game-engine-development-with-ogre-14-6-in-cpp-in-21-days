// Day 1 -- What OGRE Hands You, and What It Does Not.
//
// One file, top to bottom, with no structure at all. That is deliberate: today is about
// finding out what the renderer already does for you. The engine starts on Day 4.
//
// Run it as:   lanternkeep --scene <name>
// and it writes shots/<name>.png. Every figure in this chapter is produced that way, which is
// what makes them reproducible rather than illustrative.

#include <Ogre.h>
#include <OgreRTShaderSystem.h>
#include <OgreApplicationContext.h>   // OgreBites, for SGTechniqueResolverListener

#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------------------
// The RTSS needs somebody to notice a material that has no runnable technique and ask it to
// generate one. Without this listener the scene configures, renders, and comes out untextured
// and unlit -- or throws, depending on the material. Day 1 hits this; it is not an advanced
// topic, it is the first one.
// ---------------------------------------------------------------------------------------
class ShaderResolver : public Ogre::MaterialManager::Listener
{
public:
    explicit ShaderResolver(Ogre::RTShader::ShaderGenerator* gen) : mGen(gen) {}

    Ogre::Technique* handleSchemeNotFound(unsigned short,
                                          const Ogre::String& schemeName,
                                          Ogre::Material* originalMaterial,
                                          unsigned short,
                                          const Ogre::Renderable*) override
    {
        if (schemeName != Ogre::MSN_SHADERGEN)
            return nullptr;

        // Ask the RTSS to build a technique for this material under the shader scheme.
        if (!mGen->createShaderBasedTechnique(*originalMaterial,
                                              Ogre::MSN_DEFAULT, schemeName))
            return nullptr;

        mGen->validateMaterial(schemeName, originalMaterial->getName(),
                               originalMaterial->getGroup());

        return originalMaterial->getTechnique(1);
    }

private:
    Ogre::RTShader::ShaderGenerator* mGen;
};

// EVERY SCENE THIS DAY CAN RENDER, DECLARED IN ONE PLACE. `--list-scenes` prints it, and the
// capture harness reads that list rather than carrying its own copy. A harness with its own
// list drifts from the program the moment a scene is renamed, and the symptom is a figure that
// silently stops being regenerated while the old PNG stays on disk looking current.
static const char* SCENES[] = {
    "first_light",     // the day's result: one lit block, written with no window on screen
    "empty",           // the same program with nothing in the scene
    "no_light",        // an object, but no light: ambient only
    "ambient_black",   // no ambient at all: only what the lantern reaches
    "ambient_bright",  // ambient turned up until the modelling washes out
    "light_low",       // the same light, moved low and to the other side
    "target_small",    // a 320x240 render texture: the size is yours, not the screen's
    "bg_pale",         // a different viewport background, to show what it is
    "wide_angle",      // a wider field of view from the same place
    "no_scheme",       // the RTSS is running; the viewport is left on the default scheme
    "no_rtss",         // the RTSS is not set up at all -- this one is MEANT to fail
};

int main(int argc, char** argv)
{
    std::string scene = "first_light";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES)
                std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc)
            scene = argv[i + 1];
    }

    try
    {
        // -- 1 -------------------------------------------------------------------------
        // Root with NO automatic window. The 14.6 signature is initialise(bool, String);
        // the third customCapabilities argument that older tutorials pass does not exist
        // and will not compile.
        // The log goes to the FILE and not to the console. Create it before Root, or Root
        // makes a default one that prints every line of driver detection to stdout -- roughly
        // two hundred lines before this program's own first word. The log is where you read
        // what OGRE did; stdout is where you read what YOUR program did, and mixing them
        // makes both useless.
        Ogre::LogManager* logMgr = new Ogre::LogManager();
        logMgr->createLog("lanternkeep.log", true, false, false);

        Ogre::Root* root = new Ogre::Root("", "", "");

        root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/RenderSystem_Direct3D11");
        // Rendering works without an image codec. WRITING the PNG does not, and the
        // failure is a crash inside writeContentsToFile long after the render that
        // looked fine.
        root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/Codec_STBI");

        Ogre::RenderSystem* rs =
            root->getRenderSystemByName("Direct3D11 Rendering Subsystem");
        rs->setConfigOption("Video Mode", "640 x 480 @ 32-bit colour");
        root->setRenderSystem(rs);
        root->initialise(false, "Lantern Keep");

        // -- 2 -------------------------------------------------------------------------
        // A device still needs a window to exist on D3D11. Nothing says it has to be shown,
        // so it is created hidden at 64x64 and never drawn into.
        Ogre::NameValuePairList params;
        params["hidden"] = "true";
        Ogre::RenderWindow* hidden =
            root->createRenderWindow("hidden", 64, 64, false, &params);
        hidden->setActive(false);

        // -- 3 -------------------------------------------------------------------------
        // The SDK's own resources.cfg is the authority here and it says [OgreInternal].
        // Put these in General instead and everything configures, an EMPTY scene renders and
        // writes correctly, and the first real object throws FileNotFoundException from
        // inside update() because OgreUnifiedShader.h is looked up by group.
        Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();
        const std::string media = LK_OGRE_MEDIA;
        rgm.addResourceLocation(media + "/Main",
                                 "FileSystem", Ogre::RGN_INTERNAL);
        rgm.addResourceLocation(media + "/RTShaderLib",
                                 "FileSystem", Ogre::RGN_INTERNAL);
        rgm.initialiseAllResourceGroups();

        Ogre::SceneManager* sceneMgr = root->createSceneManager();

        // -- 4 -------------------------------------------------------------------------
        // D3D11 has no fixed-function pipeline, so a plain colour material has no shader at
        // all until the RTSS makes one.
        const bool wantRtss = (scene != "no_rtss");
        if (wantRtss)
        {
            Ogre::RTShader::ShaderGenerator::initialize();
            Ogre::RTShader::ShaderGenerator* shaderGen =
                Ogre::RTShader::ShaderGenerator::getSingletonPtr();
            shaderGen->addSceneManager(sceneMgr);
            ShaderResolver* resolver = new ShaderResolver(shaderGen);
            Ogre::MaterialManager::getSingleton().addListener(resolver);
        }

        // -- the scene ------------------------------------------------------------------
        // Ambient is the light that arrives from nowhere in particular. It is not physics; it
        // is the amount of the object you are willing to see where the lantern does not reach.
        Ogre::ColourValue ambient(0.25f, 0.25f, 0.30f);
        if (scene == "ambient_black")  ambient = Ogre::ColourValue(0.0f, 0.0f, 0.0f);
        if (scene == "ambient_bright") ambient = Ogre::ColourValue(0.85f, 0.85f, 0.88f);
        sceneMgr->setAmbientLight(ambient);

        Ogre::Camera* camera = sceneMgr->createCamera("view");
        camera->setNearClipDistance(0.5f);
        camera->setAutoAspectRatio(true);
        if (scene == "wide_angle")
            camera->setFOVy(Ogre::Degree(70));
        Ogre::SceneNode* camNode = sceneMgr->getRootSceneNode()->createChildSceneNode();
        // Aimed at the middle of the block, not at the floor. Pointing the camera below
        // what you are photographing is the commonest reason a first frame has the
        // subject stranded in the top half with nothing under it.
        camNode->setPosition(0, 78, 190);
        camNode->lookAt(Ogre::Vector3(0, 45, 0), Ogre::Node::TS_WORLD);
        camNode->attachObject(camera);

        const bool wantLight  = (scene != "no_light");
        const bool wantObject = (scene != "empty");

        if (wantLight)
        {
            Ogre::Light* lantern = sceneMgr->createLight("lantern");
            lantern->setType(Ogre::Light::LT_POINT);
            lantern->setDiffuseColour(1.0f, 0.86f, 0.62f);
            lantern->setSpecularColour(1.0f, 0.95f, 0.80f);
            Ogre::SceneNode* lightNode =
                sceneMgr->getRootSceneNode()->createChildSceneNode();
            // Where a light stands decides which faces you can read. Same block, same
            // camera, same material: only this vector changes between two of this
            // chapter's figures.
            lightNode->setPosition(scene == "light_low" ? Ogre::Vector3(-110, 25, 90)
                                                        : Ogre::Vector3(80, 140, 120));
            lightNode->attachObject(lantern);
        }

        if (wantObject)
        {
            // Ogre::Entity is an INSTANCE OF A MESH. It is not a game object -- it carries no
            // behaviour, no identity in the world, and nothing the game would call gameplay.
            // Day 8 is about that distinction; today it is just the thing on the screen.
            Ogre::Entity* keep = sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE);
            Ogre::SceneNode* keepNode =
                sceneMgr->getRootSceneNode()->createChildSceneNode();
            keepNode->attachObject(keep);
            keepNode->setScale(0.6f, 0.9f, 0.6f);
            keepNode->setPosition(0, 45, 0);
            keepNode->yaw(Ogre::Degree(28));
        }

        // -- the render target we actually draw into -------------------------------------
        // Not the window. The window exists only because the device needs one; the picture is
        // made in a texture, which is the arrangement the whole book keeps.
        // NOTHING HERE ASKS THE SCREEN HOW BIG IT IS. The picture is whatever size you say it
        // is, which is why a figure in this book is the same on every machine.
        const unsigned w = (scene == "target_small") ? 640u : 2400u;
        const unsigned h = (scene == "target_small") ? 480u : 1800u;

        Ogre::TexturePtr target = Ogre::TextureManager::getSingleton().createManual(
            "shot", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
            w, h, 0, Ogre::PF_B8G8R8A8, Ogre::TU_RENDERTARGET);

        Ogre::RenderTexture* rt = target->getBuffer()->getRenderTarget();
        Ogre::Viewport* vp = rt->addViewport(camera);
        // The background is not "the sky" and it is not nothing: it is the colour the
        // viewport is cleared to before anything is drawn. Day 9 puts a real sky in
        // front of it.
        vp->setBackgroundColour(scene == "bg_pale"
                                ? Ogre::ColourValue(0.78f, 0.76f, 0.70f)
                                : Ogre::ColourValue(0.05f, 0.06f, 0.09f));

        // Without this the viewport never asks for the shader scheme, so the listener
        // above is never called and the RTSS might as well not be running.
        if (wantRtss && scene != "no_scheme")
            vp->setMaterialScheme(Ogre::MSN_SHADERGEN);

        rt->update();

        Ogre::String out = "shots/" + scene + ".png";
        rt->writeContentsToFile(out);
        std::cout << "wrote " << out << "\n";

        Ogre::LogManager::getSingleton().logMessage("shot written: " + out);

        root->shutdown();
        std::cout.flush();

        // ~Root faults on this build after a clean shutdown(), on BOTH D3D11 and GL3Plus, so
        // it is not the renderer. Everything is written and every device is released by here.
        // A tool whose exit code is always failure cannot report a real failure.
        std::_Exit(0);
    }
    catch (const Ogre::Exception& e)
    {
        std::cerr << "OGRE: " << e.getFullDescription() << "\n";
        return 1;
    }
}
