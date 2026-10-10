// Day 3 -- Cameras, Lights, and What One Frame Costs.
//
// The last day of the tour. After today the renderer stops being the subject and starts being
// the thing the engine sits on top of.
//
// Run it as:   lanternkeep --scene <name>
//              lanternkeep --cost            (the measurement, no picture)

#include <Ogre.h>
#include <OgreRTShaderSystem.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

class ShaderResolver : public Ogre::MaterialManager::Listener
{
public:
    explicit ShaderResolver(Ogre::RTShader::ShaderGenerator* gen) : mGen(gen) {}

    Ogre::Technique* handleSchemeNotFound(unsigned short, const Ogre::String& schemeName,
                                          Ogre::Material* mat, unsigned short,
                                          const Ogre::Renderable*) override
    {
        if (schemeName != Ogre::MSN_SHADERGEN) return nullptr;
        if (!mGen->createShaderBasedTechnique(*mat, Ogre::MSN_DEFAULT, schemeName))
            return nullptr;
        mGen->validateMaterial(schemeName, mat->getName(), mat->getGroup());
        return mat->getTechnique(1);
    }

private:
    Ogre::RTShader::ShaderGenerator* mGen;
};

static const char* SCENES[] = {
    // `courtyard` is the BASELINE every other frame in this chapter is compared against.
    // It used to be two scenes, `cam_back` and `light_point`, and they rendered byte-identical
    // pictures -- the default camera scene simply IS the point-light scene. Two names for one
    // image is a comparison of nothing, so there is one name.
    "courtyard",       // the baseline: camera back, one point light
    "cam_close",       // the camera moved in; nothing else changes
    "fov_narrow",      // a 35 degree field of view from the baseline position
    "fov_wide",        // a 75 degree field of view from the same place
    "light_directional", // a directional light: no position, no falloff
    "light_spot",      // a spot light: a cone, an inner angle and an outer one
    "atten_short",     // the same point light with its range cut down
    "many_lights",     // twenty-four lanterns at once
    "clip_near",       // the near plane pushed out until it cuts the courtyard
};

int main(int argc, char** argv)
{
    std::string scene = "courtyard";
    bool cost = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--list-scenes") { for (const char* s : SCENES) std::cout << s << "\n"; return 0; }
        if (a == "--cost") cost = true;
        if (a == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    try
    {
        Ogre::LogManager* logMgr = new Ogre::LogManager();
        logMgr->createLog("lanternkeep.log", true, false, false);

        Ogre::Root* root = new Ogre::Root("", "", "");
        root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/RenderSystem_Direct3D11");
        root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/Codec_STBI");

        Ogre::RenderSystem* rs = root->getRenderSystemByName("Direct3D11 Rendering Subsystem");
        rs->setConfigOption("Video Mode", "640 x 480 @ 32-bit colour");
        root->setRenderSystem(rs);
        root->initialise(false, "Lantern Keep");

        Ogre::NameValuePairList params;
        params["hidden"] = "true";
        Ogre::RenderWindow* hidden = root->createRenderWindow("hidden", 64, 64, false, &params);
        hidden->setActive(false);

        Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();
        const std::string media = LK_OGRE_MEDIA;
        rgm.addResourceLocation(media + "/Main",        "FileSystem", Ogre::RGN_INTERNAL);
        rgm.addResourceLocation(media + "/RTShaderLib", "FileSystem", Ogre::RGN_INTERNAL);
        rgm.addResourceLocation(LK_BOOK_MEDIA,          "FileSystem", Ogre::RGN_DEFAULT);
        rgm.initialiseAllResourceGroups();

        Ogre::SceneManager* sceneMgr = root->createSceneManager();

        Ogre::RTShader::ShaderGenerator::initialize();
        Ogre::RTShader::ShaderGenerator* shaderGen =
            Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        shaderGen->addSceneManager(sceneMgr);
        Ogre::MaterialManager::getSingleton().addListener(new ShaderResolver(shaderGen));

        sceneMgr->setAmbientLight(Ogre::ColourValue(0.16f, 0.16f, 0.21f));
        Ogre::SceneNode* rootNode = sceneMgr->getRootSceneNode();

        // -- a courtyard of blocks, so there is something for a light to fall across --------
        Ogre::MaterialPtr stone = Ogre::MaterialManager::getSingleton().create(
            "keep/stone", Ogre::RGN_DEFAULT);
        {
            Ogre::Pass* pass = stone->getTechnique(0)->getPass(0);
            pass->setDiffuse(Ogre::ColourValue(0.80f, 0.78f, 0.74f));
            pass->setSpecular(Ogre::ColourValue(0.2f, 0.2f, 0.2f));
            pass->setShininess(18.0f);
            pass->createTextureUnitState("blockwork.png");
        }

        Ogre::Entity* ground = sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE);
        ground->setMaterialName("keep/stone");
        Ogre::SceneNode* groundNode = rootNode->createChildSceneNode();
        groundNode->attachObject(ground);
        groundNode->setScale(6.0f, 0.06f, 6.0f);
        groundNode->setPosition(0, -3, 0);

        for (int i = 0; i < 9; ++i)
        {
            Ogre::Entity* e = sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE);
            e->setMaterialName("keep/stone");
            Ogre::SceneNode* n = rootNode->createChildSceneNode();
            n->attachObject(e);
            n->setScale(0.30f, 0.55f + 0.12f * (i % 3), 0.30f);
            n->setPosition(-160.0f + 40.0f * i, 25, -40.0f + 30.0f * (i % 4));
            n->yaw(Ogre::Degree(11.0f * i));
        }

        // -- the light, whichever kind this scene asked for ---------------------------------
        if (scene == "many_lights")
        {
            // 14.6 added clustered light culling. Twenty-four lights is where the difference
            // between "every light tested against every object" and a clustered pass starts
            // to be something you can measure rather than something you read about.
            for (int i = 0; i < 24; ++i)
            {
                Ogre::Light* l = sceneMgr->createLight("lantern" + std::to_string(i));
                l->setType(Ogre::Light::LT_POINT);
                l->setDiffuseColour(Ogre::ColourValue(1.0f, 0.70f + 0.01f * i, 0.45f));
                l->setAttenuation(220, 1.0f, 0.010f, 0.0f);
                Ogre::SceneNode* n = rootNode->createChildSceneNode();
                n->setPosition(-170.0f + 15.0f * i,
                               45 + 25 * (i % 3), -30.0f + 22.0f * (i % 5));
                n->attachObject(l);
            }
        }
        else
        {
            Ogre::Light* l = sceneMgr->createLight("lantern");
            Ogre::SceneNode* n = rootNode->createChildSceneNode();

            if (scene == "light_directional")
            {
                // A directional light has NO position. It is a direction and nothing else,
                // which is why moving its node does not change the picture.
                l->setType(Ogre::Light::LT_DIRECTIONAL);
                n->setDirection(-0.5f, -0.9f, -0.45f, Ogre::Node::TS_WORLD);
            }
            else if (scene == "light_spot")
            {
                l->setType(Ogre::Light::LT_SPOTLIGHT);
                l->setSpotlightRange(Ogre::Degree(22), Ogre::Degree(46));
                n->setPosition(20, 240, 120);
                n->setDirection(-0.08f, -1.0f, -0.42f, Ogre::Node::TS_WORLD);
            }
            else
            {
                l->setType(Ogre::Light::LT_POINT);
                n->setPosition(20, 150, 130);
                // Range, constant, linear, quadratic. Cut the range and the far blocks simply
                // stop being lit -- the falloff is not gradual once the range runs out.
                if (scene == "atten_short") l->setAttenuation(150, 1.0f, 0.020f, 0.0f);
                else                        l->setAttenuation(900, 1.0f, 0.0018f, 0.0f);
            }
            l->setDiffuseColour(1.0f, 0.86f, 0.62f);
            l->setSpecularColour(1.0f, 0.95f, 0.80f);
            n->attachObject(l);
        }

        // -- the camera ---------------------------------------------------------------------
        Ogre::Camera* camera = sceneMgr->createCamera("view");
        camera->setAutoAspectRatio(true);
        camera->setNearClipDistance(scene == "clip_near" ? 260.0f : 0.5f);
        if (scene == "fov_narrow") camera->setFOVy(Ogre::Degree(35));
        if (scene == "fov_wide")   camera->setFOVy(Ogre::Degree(75));

        Ogre::SceneNode* camNode = rootNode->createChildSceneNode();
        camNode->setPosition(scene == "cam_close" ? Ogre::Vector3(0, 70, 170)
                                                  : Ogre::Vector3(0, 150, 360));
        camNode->lookAt(Ogre::Vector3(0, 30, 0), Ogre::Node::TS_WORLD);
        camNode->attachObject(camera);

        Ogre::TexturePtr target = Ogre::TextureManager::getSingleton().createManual(
            "shot", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
            2400, 1800, 0, Ogre::PF_B8G8R8A8, Ogre::TU_RENDERTARGET);
        Ogre::RenderTexture* rt = target->getBuffer()->getRenderTarget();
        Ogre::Viewport* vp = rt->addViewport(camera);
        vp->setBackgroundColour(Ogre::ColourValue(0.04f, 0.05f, 0.08f));
        vp->setMaterialScheme(Ogre::MSN_SHADERGEN);

        if (cost)
        {
            // WARM UP FIRST. The first frame compiles shaders, uploads textures and builds
            // state every later frame reuses, so timing it measures the start-up, not the
            // frame. Ten throwaway frames, then the ones that count.
            for (int i = 0; i < 10; ++i) rt->update();

            Ogre::Timer timer;
            const int N = 200;
            timer.reset();
            for (int i = 0; i < N; ++i) rt->update();
            const double ms = timer.getMicroseconds() / 1000.0 / N;

            // WHAT IS SAFE TO PRINT AND WHAT IS NOT.
            //
            // batchCount and triangleCount are decided by the scene, so they are the same on
            // every run and on every machine: they can be printed as facts.
            //
            // The millisecond figure is NOT. Measured here across three runs of the same
            // build, one lantern came out at 0.05, 0.06 and 0.05 ms, and twenty-four came out
            // at 0.16, 0.19 and 0.12. A book that prints "0.16 ms" is printing a number its
            // own next run will not reproduce, and its output gate will fail on the truth.
            //
            // So the timing is printed to two decimal places AND labelled as varying, and the
            // claim the chapter actually makes is the VERDICT below -- a comparison against a
            // declared limit, which is stable across runs and across machines fast enough to
            // matter.
            const Ogre::RenderTarget::FrameStats& st = rt->getStatistics();
            const double limit = 1.00;
            std::cout << "scene            " << scene << "\n";
            std::cout << "batches          " << st.batchCount << "\n";
            std::cout << "triangles        " << st.triangleCount << "\n";
            std::cout << "frames measured  " << N << " (after 10 warm-up frames)\n";
            std::cout << "mean frame       " << std::fixed << std::setprecision(2) << ms
                      << " ms   (varies run to run -- read the verdict, not the number)\n";
            std::cout << "verdict          " << (ms < limit ? "UNDER" : "OVER")
                      << " the " << std::setprecision(2) << limit << " ms budget\n";
        }
        else
        {
            rt->update();
            Ogre::String out = "shots/" + scene + ".png";
            rt->writeContentsToFile(out);
            std::cout << "wrote " << out << "\n";
        }

        root->shutdown();
        std::cout.flush();
        std::_Exit(0);
    }
    catch (const Ogre::Exception& e)
    {
        std::cerr << "OGRE: " << e.getFullDescription() << "\n";
        return 1;
    }
}
