// Keepstone -- Collision.cpp

#include "Collision.h"

#include <algorithm>
#include <cmath>

namespace keepstone {

namespace {
float mn(float a, float b) { return a < b ? a : b; }
float mx(float a, float b) { return a > b ? a : b; }
}  // namespace

int Collision::add(const Box& b, const std::string& name)
{
    mBoxes.push_back(b);
    mNames.push_back(name);
    return static_cast<int>(mBoxes.size() - 1);
}

void Collision::clear() { mBoxes.clear(); mNames.clear(); }

bool Collision::overlaps(const Box& a, const Box& b)
{
    // Separating axis, the cheap case: two boxes overlap unless there is an axis on
    // which they do not. Written as the negation because that is the early-out.
    if (a.maxX() <= b.minX() || a.minX() >= b.maxX()) return false;
    if (a.maxY() <= b.minY() || a.minY() >= b.maxY()) return false;
    if (a.maxZ() <= b.minZ() || a.minZ() >= b.maxZ()) return false;
    return true;
}

Hit Collision::raycast(Vec3 from, Vec3 dir, float maxDistance) const
{
    Hit best;
    best.distance = maxDistance;

    for (std::size_t i = 0; i < mBoxes.size(); ++i)
    {
        const Box& b = mBoxes[i];

        // The slab method. For each axis, the ray enters the box's slab at one t and leaves at
        // another; the box is hit when the latest entry is before the earliest exit.
        float tmin = 0.0f, tmax = maxDistance;
        const float o[3] = { from.x, from.y, from.z };
        const float d[3] = { dir.x, dir.y, dir.z };
        const float lo[3] = { b.minX(), b.minY(), b.minZ() };
        const float hi[3] = { b.maxX(), b.maxY(), b.maxZ() };

        bool miss = false;
        for (int a = 0; a < 3; ++a)
        {
            if (std::fabs(d[a]) < 1e-6f)
            {
                // PARALLEL TO THIS SLAB. Dividing here would be a division by zero, and the
                // answer it would produce (an infinity) happens to work on some axes and not
                // others. Say what is true instead: if the origin is outside the slab and the
                // ray never approaches it, there is no hit at all.
                if (o[a] < lo[a] || o[a] > hi[a]) { miss = true; break; }
                continue;
            }
            const float inv = 1.0f / d[a];
            float t1 = (lo[a] - o[a]) * inv;
            float t2 = (hi[a] - o[a]) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = mx(tmin, t1);
            tmax = mn(tmax, t2);
            if (tmin > tmax) { miss = true; break; }
        }
        if (miss || tmin >= best.distance) continue;

        best.hit = true;
        best.distance = tmin;
        best.which = static_cast<int>(i);
        best.point.x = from.x + dir.x * tmin;
        best.point.y = from.y + dir.y * tmin;
        best.point.z = from.z + dir.z * tmin;
    }
    return best;
}

Vec3 Collision::moveDiscrete(const Box& mover, Vec3 delta, bool* blocked) const
{
    // MOVE FIRST, ASK AFTERWARDS. If the destination overlaps something, refuse the
    // move.
    //
    // This is correct for slow things and silently wrong for fast ones: a mover that
    // travels further in one tick than the wall is thick starts on one side, ends on
    // the other, and overlaps nothing at either end. Nothing in this function ever
    // looks at the space between.
    Box end = mover;
    end.centre.x += delta.x;
    end.centre.y += delta.y;
    end.centre.z += delta.z;

    for (const Box& b : mBoxes)
    {
        if (overlaps(end, b))
        {
            if (blocked) *blocked = true;
            return mover.centre;
        }
    }
    if (blocked) *blocked = false;
    return end.centre;
}

Vec3 Collision::moveSwept(const Box& mover, Vec3 delta, bool* blocked, float* toi) const
{
    // THE WHOLE PATH, NOT THE TWO ENDS. Expand each wall by the mover's half-extents
    // and cast a ray from the mover's centre: a box sweeping against a box is the same
    // question as a point against a box that has grown by the first one's size. That
    // trick is why this is short.
    float earliest = 1.0f;
    bool  any = false;

    const float len =
        std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (len < 1e-6f)
    {
        if (blocked) *blocked = false;
        if (toi) *toi = 1.0f;
        return mover.centre;
    }

    for (const Box& b : mBoxes)
    {
        Box grown = b;
        grown.half.x += mover.half.x;
        grown.half.y += mover.half.y;
        grown.half.z += mover.half.z;

        Collision one;
        one.mBoxes.push_back(grown);
        const Vec3 dir { delta.x / len, delta.y / len, delta.z / len };
        const Hit h = one.raycast(mover.centre, dir, len);
        if (h.hit)
        {
            const float f = h.distance / len;
            if (f < earliest) { earliest = f; any = true; }
        }
    }

    if (blocked) *blocked = any;
    if (toi) *toi = earliest;

    // A SLIVER SHORT OF CONTACT. Landing exactly on the surface leaves the mover
    // touching, and the next tick's test then starts from inside floating-point noise
    // -- which is how a character gets stuck against a wall it is already resting on.
    const float stop = any ? mx(0.0f, earliest - 0.001f) : 1.0f;
    Vec3 out = mover.centre;
    out.x += delta.x * stop;
    out.y += delta.y * stop;
    out.z += delta.z * stop;
    return out;
}

}  // namespace keepstone
