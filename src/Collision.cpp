#include "Collision.h"

#include <algorithm>
#include <cmath>

bool Aabb::Intersects(const Aabb& o) const {
    return std::fabs(center.x - o.center.x) <= half.x + o.half.x &&
           std::fabs(center.y - o.center.y) <= half.y + o.half.y &&
           std::fabs(center.z - o.center.z) <= half.z + o.half.z;
}

void Aabb::ResolvePenetrationAxis(Aabb& movable, int axis, float sign) const {
    if (axis == 0) {
        const float pen = (half.x + movable.half.x) - std::fabs(center.x - movable.center.x);
        if (pen > 0.f) {
            movable.center.x += sign * pen;
        }
    } else if (axis == 1) {
        const float pen = (half.y + movable.half.y) - std::fabs(center.y - movable.center.y);
        if (pen > 0.f) {
            movable.center.y += sign * pen;
        }
    } else {
        const float pen = (half.z + movable.half.z) - std::fabs(center.z - movable.center.z);
        if (pen > 0.f) {
            movable.center.z += sign * pen;
        }
    }
}

bool AabbVsAabbResolveFirstAxis(const Aabb& fixed, Aabb* movable) {
    if (!fixed.Intersects(*movable)) {
        return false;
    }
    const float pen_x = (fixed.half.x + movable->half.x) - std::fabs(fixed.center.x - movable->center.x);
    const float pen_z = (fixed.half.z + movable->half.z) - std::fabs(fixed.center.z - movable->center.z);
    if (pen_x < pen_z) {
        const float sign = (movable->center.x >= fixed.center.x) ? 1.f : -1.f;
        fixed.ResolvePenetrationAxis(*movable, 0, sign);
    } else {
        const float sign = (movable->center.z >= fixed.center.z) ? 1.f : -1.f;
        fixed.ResolvePenetrationAxis(*movable, 2, sign);
    }
    return true;
}

bool SphereVsAabbPushSphere(const Aabb& box, Sphere* s) {
    const D3DXVECTOR3 min_pt(box.center.x - box.half.x, box.center.y - box.half.y,
                             box.center.z - box.half.z);
    const D3DXVECTOR3 max_pt(box.center.x + box.half.x, box.center.y + box.half.y,
                             box.center.z + box.half.z);
    const float cx = std::clamp(s->center.x, min_pt.x, max_pt.x);
    const float cy = std::clamp(s->center.y, min_pt.y, max_pt.y);
    const float cz = std::clamp(s->center.z, min_pt.z, max_pt.z);
    D3DXVECTOR3 closest(cx, cy, cz);
    D3DXVECTOR3 delta = s->center - closest;
    float dist_sq = D3DXVec3LengthSq(&delta);
    if (dist_sq >= s->radius * s->radius) {
        return false;
    }
    if (dist_sq < 1e-10f) {
        delta = s->center - box.center;
        dist_sq = D3DXVec3LengthSq(&delta);
        if (dist_sq < 1e-10f) {
            delta = D3DXVECTOR3(1.f, 0.f, 0.f);
            dist_sq = 1.f;
        }
    }
    const float dist = std::sqrt(dist_sq);
    D3DXVECTOR3 n = delta * (1.f / dist);
    s->center = closest + n * (s->radius + 0.02f);
    return true;
}
