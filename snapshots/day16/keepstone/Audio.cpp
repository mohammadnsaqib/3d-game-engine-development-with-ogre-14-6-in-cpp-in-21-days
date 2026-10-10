// Keepstone -- Audio.cpp

#include "Audio.h"

#include <cmath>
#include <cstdio>

namespace keepstone {

namespace {
const float PI = 3.14159265358979323846f;

float distance(Vec3 a, Vec3 b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}  // namespace

bool Audio::startUp() { return true; }

void Audio::shutDown() { mSources.clear(); mSamples.clear(); }

void Audio::setListener(Vec3 position, Vec3 rightAxis)
{
    mListener = position;
    mRight = rightAxis;
}

int Audio::add(const Source& s)
{
    mSources.push_back(s);
    return static_cast<int>(mSources.size() - 1);
}

void Audio::setFalloff(float refDistance, float rolloff, float maxDistance)
{
    mRefDistance = refDistance;
    mRolloff = rolloff;
    mMaxDistance = maxDistance;
}

float Audio::gainFor(const Source& s) const
{
    const float d = distance(s.position, mListener);
    // A hard cut, so distant sources cost nothing.
    if (d >= mMaxDistance) return 0.0f;

    // INVERSE DISTANCE WITH A REFERENCE. Inside refDistance the gain is 1 and does not
    // climb to infinity as the distance goes to zero -- which is what a bare 1/d does,
    // and why a source the listener walks onto would otherwise blow the mix apart.
    if (d <= mRefDistance) return s.gain;
    return s.gain * mRefDistance / (mRefDistance + mRolloff * (d - mRefDistance));
}

float Audio::panFor(const Source& s) const
{
    const Vec3 to { s.position.x - mListener.x,
                    s.position.y - mListener.y,
                    s.position.z - mListener.z };
    const float len = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
    if (len < 1e-4f) return 0.0f;            // on top of the listener: dead centre

    // The component along the listener's right axis, normalised. Nothing else is
    // needed for stereo: up and forward do not move a sound between two speakers.
    const float dot = (to.x * mRight.x + to.y * mRight.y + to.z * mRight.z) / len;
    return dot < -1.0f ? -1.0f : (dot > 1.0f ? 1.0f : dot);
}

float Audio::mix(float seconds)
{
    const int frames = static_cast<int>(seconds * mRate);
    mSamples.assign(static_cast<std::size_t>(frames) * 2, 0);
    mClipped = 0;

    float peak = 0.0f;
    for (int i = 0; i < frames; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(mRate);
        float left = 0.0f, right = 0.0f;

        for (const Source& s : mSources)
        {
            const float g = gainFor(s);
            if (g <= 0.0f) continue;
            const float pan = panFor(s);

            // EQUAL-POWER PANNING, not a linear crossfade. A linear pan loses about three
            // decibels in the middle, so a source sweeping past the listener audibly dips as it
            // crosses the centre. Sine and cosine of the same angle keep the total power flat.
            const float angle = (pan + 1.0f) * 0.25f * PI;   // -1..1 -> 0..PI/2
            const float sample = std::sin(2.0f * PI * s.frequency * t) * g;
            left  += sample * std::cos(angle);
            right += sample * std::sin(angle);
        }

        if (std::fabs(left) > peak)  peak = std::fabs(left);
        if (std::fabs(right) > peak) peak = std::fabs(right);

        // CLAMPED, AND COUNTED. Samples past full scale wrap rather than clip if they are simply
        // cast, which turns a loud moment into a burst of noise -- the worst-sounding failure in
        // audio and one of the least obvious in code. Counting them means the mix can say it
        // happened instead of the reader having to hear it.
        auto toPcm = [&](float v) -> short {
            if (v > 1.0f)  { ++mClipped; v = 1.0f; }
            if (v < -1.0f) { ++mClipped; v = -1.0f; }
            return static_cast<short>(v * 32767.0f);
        };
        mSamples[static_cast<std::size_t>(i) * 2]     = toPcm(left);
        mSamples[static_cast<std::size_t>(i) * 2 + 1] = toPcm(right);
    }
    return peak;
}

bool Audio::writeWav(const std::string& path) const
{
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    const int channels = 2, bits = 16;
    const int dataBytes = static_cast<int>(mSamples.size() * sizeof(short));
    const int byteRate = mRate * channels * bits / 8;
    const int riff = 36 + dataBytes;

    auto u32 = [&](int v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](short v) { std::fwrite(&v, 2, 1, f); };

    std::fwrite("RIFF", 1, 4, f); u32(riff); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(static_cast<short>(channels));
    u32(mRate); u32(byteRate);
    u16(static_cast<short>(channels * bits / 8)); u16(static_cast<short>(bits));
    std::fwrite("data", 1, 4, f); u32(dataBytes);
    if (!mSamples.empty()) std::fwrite(&mSamples[0], 1, dataBytes, f);

    std::fclose(f);
    return true;
}

}  // namespace keepstone
