// The Lantern Keep -- Day 16.
//
// The courtyard has been silent for fifteen days. Today the lanterns hum, and the hum knows
// where it is coming from.
//
// SOUND HAS NO PICTURE, so this program draws the one thing that matters and can be drawn: the
// gain each source contributes, as a bar under the lantern it belongs to. The bars are measured
// from the same `gainFor` the mixer uses -- not a separate illustration that could drift from it.
//
// It also writes `shots/<scene>.wav`. That file is the real output; the PNG is the report.

#include "keepstone/Audio.h"
#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "near",        // one lantern, close: full gain, dead centre
    "far",         // the same lantern at four times the distance
    "very_far",    // past maxDistance: cut entirely, and costing nothing
    "left",        // off to the listener's left: the pan goes with it
    "right",       // and the right
    "pair",        // two lanterns either side: the mix is the sum
    "row",         // six lanterns in a row across the courtyard
    "loud",        // six at full gain: the mix clips, and the count says so
    "flat_rolloff",// the same row with rolloff 0: everything equally loud, and wrong
    "steep_rolloff", // and with rolloff 4: only the nearest survives
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "near";
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
    keepstone::Audio audio;
    keepstone::Engine engine;
    engine.add(&renderer);
    engine.add(&audio);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    renderer.setAmbient({0.19f, 0.19f, 0.24f});
    renderer.setCamera({0.0f, 170.0f, 470.0f}, {0.0f, 40.0f, 0.0f});

    // The listener stands in the middle of the courtyard facing the back wall, so the right
    // axis is +x and a lantern at negative x is on the left ear.
    const keepstone::Vec3 ear { 0.0f, 40.0f, 140.0f };
    audio.setListener(ear, {1.0f, 0.0f, 0.0f});

    float rolloff = 1.0f;
    if (scene == "flat_rolloff")  rolloff = 0.0f;
    if (scene == "steep_rolloff") rolloff = 4.0f;
    audio.setFalloff(120.0f, rolloff, 1400.0f);

    const float gain = (scene == "loud") ? 1.0f : 0.45f;

    if (scene == "near")      audio.add({{   0.0f, 60.0f,  60.0f }, 220.0f, gain, "lantern" });
    else if (scene == "far")  audio.add({{   0.0f, 60.0f, -460.0f }, 220.0f, gain, "lantern" });
    else if (scene == "very_far") audio.add({{ 0.0f, 60.0f, -1500.0f }, 220.0f, gain, "lantern" });
    else if (scene == "left")  audio.add({{ -260.0f, 60.0f, 120.0f }, 220.0f, gain, "lantern" });
    else if (scene == "right") audio.add({{  260.0f, 60.0f, 120.0f }, 220.0f, gain, "lantern" });
    else if (scene == "pair")
    {
        audio.add({{ -240.0f, 60.0f, 120.0f }, 196.0f, gain, "left-lantern" });
        audio.add({{  240.0f, 60.0f, 120.0f }, 262.0f, gain, "right-lantern" });
    }
    else
    {
        for (int i = 0; i < 6; ++i)
            audio.add({{ -300.0f + 120.0f * i, 60.0f, 60.0f },
                       180.0f + 40.0f * i, gain, "lantern" + std::to_string(i) });
    }

    const float peak = audio.mix(0.35f);
    audio.writeWav("shots/" + scene + ".wav");

    // -- the courtyard ------------------------------------------------------------------------
    for (int i = 0; i < 7; ++i)
        renderer.addBlock({ -180.0f + 60.0f * i, 40.0f, -110.0f },
                          { 0.17f, 0.38f + 0.07f * (i % 3), 0.17f }, 0.0f);

    // The listener, as a marker on the ground.
    renderer.addBlock({ ear.x, 14.0f, ear.z }, { 0.16f, 0.06f, 0.16f }, 45.0f);

    // Each source: a lantern post where it stands, a light with its own gain, and a bar whose
    // height IS that gain. The bar is read from the mixer, so a change to the falloff moves the
    // picture and the sound together or not at all.
    for (int i = 0; i < audio.sourceCount(); ++i)
    {
        const keepstone::Audio::Source& s = audio.source(i);
        const float g = audio.gainFor(s);

        renderer.addBlock({ s.position.x, 48.0f, s.position.z }, { 0.09f, 0.26f, 0.09f }, 0.0f);
        if (g > 0.0f)
            renderer.addLantern({ s.position.x, 86.0f, s.position.z },
                                { 1.0f * g + 0.1f, 0.86f * g + 0.1f, 0.60f * g + 0.1f });

        // The gain bar, and a pan marker offset left or right by how the mixer will place it.
        const float h = 0.02f + g * 1.30f;
        renderer.addBlock({ -300.0f + 58.0f * i, 130.0f + h * 50.0f, 180.0f },
                          { 0.11f, h, 0.11f }, 0.0f);
        renderer.addBlock({ -300.0f + 58.0f * i + audio.panFor(s) * 22.0f, 108.0f, 180.0f },
                          { 0.05f, 0.05f, 0.05f }, 0.0f);
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("sources          %d\n", audio.sourceCount());
    std::printf("rolloff          %.1f  (reference distance 120)\n", rolloff);
    std::printf("sample rate      %d Hz, stereo 16-bit\n", audio.sampleRate());
    std::printf("samples written  %d\n", static_cast<int>(audio.sampleCount()));
    std::printf("peak amplitude   %.3f of full scale\n", peak);
    std::printf("clipped samples  %d\n", audio.clippedSamples());
    for (int i = 0; i < audio.sourceCount(); ++i)
    {
        const keepstone::Audio::Source& s = audio.source(i);
        std::printf("  %-14s gain %.3f  pan %+.2f\n", s.name.c_str(),
                    audio.gainFor(s), audio.panFor(s));
    }

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s and %s.wav\n", out.c_str(), scene.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
