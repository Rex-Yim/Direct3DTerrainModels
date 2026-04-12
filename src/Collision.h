#pragma once

#include <d3dx9math.h>

struct Aabb {
    D3DXVECTOR3 center;
    D3DXVECTOR3 half;

    bool Intersects(const Aabb& o) const;
    /** Minimal translation to separate this from o along one axis (axis 0=x,1=y,2=z). */
    void ResolvePenetrationAxis(Aabb& movable, int axis, float sign) const;
};

struct Sphere {
    D3DXVECTOR3 center;
    float radius;
};

bool SphereVsAabbPushSphere(const Aabb& box, Sphere* s);
bool AabbVsAabbResolveFirstAxis(const Aabb& fixed, Aabb* movable);
