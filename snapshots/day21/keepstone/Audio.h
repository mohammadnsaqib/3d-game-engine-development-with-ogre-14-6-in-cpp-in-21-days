// Keepstone -- Audio.h
//
// SOUND IS THE ONLY SUBSYSTEM IN THIS BOOK WITH NO PICTURE, and that shapes the design rather
// than excusing it.
//
// A mixer you can only exercise by listening is a mixer nobody regression-tests. So the mixing
// here renders into a BUFFER, and where that buffer goes afterwards is a separate decision: a
// file while the book measures it, a device when a game plays it. Windows supplies XAudio2 for
// the device path and it takes the same samples this produces; nothing above the sink changes.
//
// That split is not a concession to a headless build. It is how the attenuation curve below
// gets checked at all: peak amplitude at a known distance is a number, and a number can be
// asserted. "It sounded about right" cannot.
//
// WHAT IS HERE: positioned sources, distance attenuation, stereo panning from the listener's
// left-right axis, and a mix down to interleaved 16-bit stereo.
// WHAT IS NOT: reverb, occlusion, doppler, streaming, or a voice limiter. Each is a chapter of
// its own and none of them is needed to make a lantern sound like it is over there.

#ifndef KEEPSTONE_AUDIO_H
#define KEEPSTONE_AUDIO_H

#include "keepstone/Engine.h"
#include "keepstone/Renderer.h"

#include <string>
#include <vector>

namespace keepstone {

class Audio : public Service
{
public:
    struct Source
    {
        Vec3  position;
        float frequency = 220.0f;   // a tone, so the mix is checkable without an asset
        float gain = 1.0f;
        std::string name;
    };

    const char* name() const override { return "audio"; }
    bool startUp() override;
    void shutDown() override;

    void setListener(Vec3 position, Vec3 rightAxis);
    int  add(const Source& s);

    // THE FALLOFF, AND IT IS A CHOICE RATHER THAN A LAW. Real sound falls off with 1/distance,
    // which in a game means a source is either deafening or inaudible and the interesting band
    // is a few metres wide. `refDistance` sets where the gain is 1, and `rolloff` how sharply
    // it drops after that -- the same three-parameter shape every audio middleware exposes,
    // because the physical answer is unusable.
    void setFalloff(float refDistance, float rolloff, float maxDistance);

    float gainFor(const Source& s) const;     // the attenuation alone, 0..1
    float panFor(const Source& s) const;      // -1 fully left, +1 fully right

    // Mixes `seconds` of every source into interleaved stereo. Returns peak amplitude, which is
    // the number the chapter asserts against.
    float mix(float seconds);

    bool writeWav(const std::string& path) const;

    int  sourceCount() const { return static_cast<int>(mSources.size()); }
    const Source& source(int i) const { return mSources[i]; }
    int  sampleRate() const { return mRate; }
    std::size_t sampleCount() const { return mSamples.size(); }
    int  clippedSamples() const { return mClipped; }

private:
    std::vector<Source> mSources;
    std::vector<short>  mSamples;
    Vec3  mListener;
    Vec3  mRight { 1.0f, 0.0f, 0.0f };
    float mRefDistance = 120.0f;
    float mRolloff = 1.0f;
    float mMaxDistance = 1400.0f;
    int   mRate = 22050;
    int   mClipped = 0;
};

}  // namespace keepstone

#endif
