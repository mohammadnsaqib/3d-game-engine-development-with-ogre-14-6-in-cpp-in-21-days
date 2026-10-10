// Day 2 -- The Renderer in a Day: Scene Graph, Meshes, Materials.
//
// Still one file. Still no engine. Today is the tour of what OGRE already does, because you
// cannot decide what to wrap until you know what is there.
//
// Run it as:   lanternkeep --scene <name>

#include <Ogre.h>
#include <OgreRTShaderSystem.h>

#include <cstdlib>
#include <iostream>
#include <string>

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
    "one_entity",       // one mesh instance on one node
    "three_things",     // three entities, three nodes, one root
    "child_attached",   // a child node: turn the parent and the child comes with it
    "child_detached",   // the same two objects with no parent link -- the contrast
    "scaled",           // scale is per node, and it is not always uniform
    "material_colour",  // a material written by hand, not BaseWhite
    "material_texture", // the same material with blockwork on it
    "texture_tiled",    // the same texture, asked to repeat four times
    "two_materials",    // two entities, two materials, one mesh
    "no_shaderlib",     // RTShaderLib never added -- this one is MEANT to fail
};

int main(int argc, char** argv)
{
    std::string scene = "one_entity";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
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

        // The SDK's own resources.cfg puts these two in [OgreInternal], and that is the right
        // group to use. It is worth knowing WHY it is the right one rather than copying it:
        // RTShaderLib holds OgreUnifiedShader.h, which every shader the RTSS writes includes.
        //
        // MEASURED, because the obvious guess is wrong: putting them in General instead
        // changes nothing here -- the frame comes out byte-identical. OGRE will look in other
        // groups when a resource is not in the one it was asked for. What actually breaks is
        // not adding RTShaderLib AT ALL, which is what `no_shaderlib` does.
        rgm.addResourceLocation(media + "/Main", "FileSystem", Ogre::RGN_INTERNAL);
        if (scene != "no_shaderlib")
            rgm.addResourceLocation(media + "/RTShaderLib", "FileSystem", Ogre::RGN_INTERNAL);

        // The book's own textures. A resource location is just a folder OGRE is willing to
        // look in; nothing is loaded until something asks for it by name.
        rgm.addResourceLocation(LK_BOOK_MEDIA, "FileSystem", Ogre::RGN_DEFAULT);
        rgm.initialiseAllResourceGroups();

        Ogre::SceneManager* sceneMgr = root->createSceneManager();

        Ogre::RTShader::ShaderGenerator::initialize();
        Ogre::RTShader::ShaderGenerator* shaderGen =
            Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        shaderGen->addSceneManager(sceneMgr);
        Ogre::MaterialManager::getSingleton().addListener(new ShaderResolver(shaderGen));

        sceneMgr->setAmbientLight(Ogre::ColourValue(0.28f, 0.28f, 0.33f));

        Ogre::Light* lantern = sceneMgr->createLight("lantern");
        lantern->setType(Ogre::Light::LT_POINT);
        lantern->setDiffuseColour(1.0f, 0.88f, 0.66f);
        Ogre::SceneNode* lightNode = sceneMgr->getRootSceneNode()->createChildSceneNode();
        lightNode->setPosition(140, 180, 190);
        lightNode->attachObject(lantern);

        Ogre::Camera* camera = sceneMgr->createCamera("view");
        camera->setNearClipDistance(0.5f);
        camera->setAutoAspectRatio(true);
        Ogre::SceneNode* camNode = sceneMgr->getRootSceneNode()->createChildSceneNode();
        camNode->setPosition(0, 120, 300);
        camNode->lookAt(Ogre::Vector3(0, 30, 0), Ogre::Node::TS_WORLD);
        camNode->attachObject(camera);

        // -- materials written in code, so the reader can see every property that is set ----
        auto makeMaterial = [&](const std::string& name, Ogre::ColourValue diffuse,
                                const std::string& texture, float tile) {
            Ogre::MaterialPtr m = Ogre::MaterialManager::getSingleton().create(
                name, Ogre::RGN_DEFAULT);
            Ogre::Pass* pass = m->getTechnique(0)->getPass(0);
            pass->setDiffuse(diffuse);
            pass->setSpecular(Ogre::ColourValue(0.25f, 0.25f, 0.25f));
            pass->setShininess(24.0f);
            if (!texture.empty())
            {
                Ogre::TextureUnitState* tu = pass->createTextureUnitState(texture);
                if (tile != 1.0f) tu->setTextureScale(1.0f / tile, 1.0f / tile);
            }
            return m;
        };

        Ogre::SceneNode* rootNode = sceneMgr->getRootSceneNode();

        if (scene == "three_things")
        {
            const char* meshes[] = { Ogre::SceneManager::PT_CUBE,
                                     Ogre::SceneManager::PT_SPHERE,
                                     Ogre::SceneManager::PT_CUBE };
            const float xs[] = { -130.0f, 0.0f, 130.0f };
            for (int i = 0; i < 3; ++i)
            {
                Ogre::Entity* e = sceneMgr->createEntity(meshes[i]);
                Ogre::SceneNode* n = rootNode->createChildSceneNode();
                n->attachObject(e);
                n->setPosition(xs[i], 40, 0);
                n->setScale(0.45f, 0.45f, 0.45f);
            }
        }
        else if (scene == "child_attached" || scene == "child_detached")
        {
            // The SAME two objects in both scenes. The only difference is which node the
            // second one hangs from -- and that is the whole idea of a scene graph.
            Ogre::SceneNode* parent = rootNode->createChildSceneNode();
            parent->setPosition(-40, 40, 0);
            parent->attachObject(sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE));
            parent->setScale(0.5f, 0.5f, 0.5f);

            Ogre::SceneNode* second = (scene == "child_attached")
                                      ? parent->createChildSceneNode()
                                      : rootNode->createChildSceneNode();
            second->attachObject(sceneMgr->createEntity(Ogre::SceneManager::PT_SPHERE));
            second->setScale(0.30f, 0.30f, 0.30f);
            second->setPosition(scene == "child_attached" ? Ogre::Vector3(190, 60, 0)
                                                          : Ogre::Vector3(55, 70, 0));
            // Turning the PARENT is what separates the two pictures.
            parent->yaw(Ogre::Degree(35));
        }
        else if (scene == "scaled")
        {
            const Ogre::Vector3 scales[] = { {0.35f, 0.35f, 0.35f},
                                             {0.35f, 0.85f, 0.35f},
                                             {0.85f, 0.25f, 0.35f} };
            for (int i = 0; i < 3; ++i)
            {
                Ogre::SceneNode* n = rootNode->createChildSceneNode();
                n->attachObject(sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE));
                n->setScale(scales[i]);
                n->setPosition(-130.0f + 130.0f * i, 40, 0);
            }
        }
        else if (scene == "two_materials")
        {
            makeMaterial("keep/stone", Ogre::ColourValue(0.72f, 0.70f, 0.66f), "blockwork.png", 1);
            makeMaterial("keep/timber", Ogre::ColourValue(0.55f, 0.38f, 0.24f), "timber.png", 1);
            const char* mats[] = { "keep/stone", "keep/timber" };
            for (int i = 0; i < 2; ++i)
            {
                Ogre::Entity* e = sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE);
                e->setMaterialName(mats[i]);
                Ogre::SceneNode* n = rootNode->createChildSceneNode();
                n->attachObject(e);
                n->setScale(0.5f, 0.5f, 0.5f);
                n->setPosition(-80.0f + 160.0f * i, 40, 0);
            }
        }
        else
        {
            // one_entity, material_colour, material_texture, texture_tiled, no_shaderlib
            Ogre::Entity* e = sceneMgr->createEntity(Ogre::SceneManager::PT_CUBE);
            if (scene == "material_colour")
                e->setMaterialName(makeMaterial("keep/plain",
                    Ogre::ColourValue(0.35f, 0.52f, 0.62f), "", 1)->getName());
            else if (scene == "material_texture")
                e->setMaterialName(makeMaterial("keep/stone",
                    Ogre::ColourValue(1, 1, 1), "blockwork.png", 1)->getName());
            else if (scene == "texture_tiled")
                e->setMaterialName(makeMaterial("keep/stone4",
                    Ogre::ColourValue(1, 1, 1), "blockwork.png", 4)->getName());

            Ogre::SceneNode* n = rootNode->createChildSceneNode();
            n->attachObject(e);
            n->setScale(0.6f, 0.6f, 0.6f);
            n->setPosition(0, 40, 0);
            n->yaw(Ogre::Degree(24));
        }

        Ogre::TexturePtr target = Ogre::TextureManager::getSingleton().createManual(
            "shot", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
            2400, 1800, 0, Ogre::PF_B8G8R8A8, Ogre::TU_RENDERTARGET);

        Ogre::RenderTexture* rt = target->getBuffer()->getRenderTarget();
        Ogre::Viewport* vp = rt->addViewport(camera);
        vp->setBackgroundColour(Ogre::ColourValue(0.05f, 0.06f, 0.09f));
        vp->setMaterialScheme(Ogre::MSN_SHADERGEN);

        rt->update();

        Ogre::String out = "shots/" + scene + ".png";
        rt->writeContentsToFile(out);
        std::cout << "wrote " << out << "\n";

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
