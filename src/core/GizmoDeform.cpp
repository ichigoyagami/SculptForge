// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/GizmoDeform.h"
#include "core/Commands.h"
#include "core/Deform.h"
#include "core/Document.h"
#include "core/MeshTransform.h"
#include "core/Multires.h"
#include "core/PlaneTools.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace sf {
const char* gizmoDeformName(GizmoDeformOp op) {
    static const char* names[] = {"Bend Arc", "Bend Curve", "Deformer",   "Deformer Hard", "Deformer Soft",
                                  "Flatten",  "Inflate",    "Offset",     "Rotate",        "Scale",
                                  "Skew",     "Smooth",     "Smooth All", "Stretch",       "Taper",
                                  "Twist",    "Slice",      "Multi Slice"};
    const int i = int(op);
    return i >= 0 && i < 18 ? names[i] : "Deformer";
}
bool gizmoDeform(const Mesh& m, GizmoDeformOp op, const GizmoDeformOptions& o, Mesh& out, std::string& error) {
    if (m.vertexCount() == 0) {
        error = "The SubTool is empty";
        return false;
    }
    if (m.micro || m.multires || !m.zspheres.empty()) {
        error = "Bake/delete Micro and subdivision levels, or make the armature a mesh first";
        return false;
    }
    if (o.axis < 0 || o.axis > 2 || !std::isfinite(o.amount + o.exponent) || std::abs(o.amount) > 10000) {
        error = "Invalid deformer settings";
        return false;
    }
    const glm::mat3 inv = glm::transpose(o.frame);
    if (std::abs(glm::determinant(o.frame) - 1) > 0.01f) {
        error = "Deformer axes must be orthonormal";
        return false;
    }
    auto weights = transformWeights(m, o.useMask, 0);
    AABB bounds;
    for (uint32_t i = 0; i < m.vertexCount(); ++i)
        if (weights.empty() || weights[i] > 0) bounds.expand(inv * (m.positions[i] - o.pivot));
    if (!bounds.valid()) {
        error = "Everything is masked or hidden";
        return false;
    }
    const glm::vec3 lo = bounds.min, hi = bounds.max, extent = glm::max(hi - lo, glm::vec3(1e-6f));
    const int a = o.axis, b = (a + 1) % 3, c = (a + 2) % 3;
    const float length = extent[a], factor = o.amount * 0.01f;
    const float sign = o.negativeSide ? -1.0f : 1.0f;
    const float anchor = o.negativeSide ? hi[a] : lo[a];
    if (op == GizmoDeformOp::Slice || op == GizmoDeformOp::MultiSlice) {
        if (m.hasHidden() || (o.useMask && std::any_of(m.mask.begin(), m.mask.end(), [](float v) { return v > 0; }))) {
            error = "Slice modifiers require a fully visible, unmasked SubTool";
            return false;
        }
        const int count = op == GizmoDeformOp::Slice ? 1 : std::clamp(o.slices, 1, 12);
        out = m;
        for (int i = 0; i < count; ++i) {
            if (out.triangleCount() > 2000000) {
                error = "Slice preview exceeds two million triangles; reduce the mesh first";
                return false;
            }
            const float t = count == 1 ? .5f : float(i + 1) / float(count + 1);
            const float position = glm::mix(lo[a], hi[a], t) + factor * length * .45f;
            Mesh cut = slicePlane(out, o.frame[a], glm::dot(o.frame[a], o.pivot) + position, error);
            if (cut.triangleCount() == 0) return false;
            out = std::move(cut);
        }
        return true;
    }
    std::vector<glm::vec3> smooth;
    if (op == GizmoDeformOp::Smooth || op == GizmoDeformOp::SmoothAll) {
        DeformOptions d;
        d.amount = std::clamp(std::abs(o.amount), 0.0f, 100.0f);
        d.useMask = false;
        d.keepVolume = o.keepVolume;
        smooth = deformPositions(m, DeformOp::Smooth, d);
    }
    out = m;
    for (uint32_t i = 0; i < m.vertexCount(); ++i) {
        const float w = weights.empty() ? 1.0f : weights[i];
        if (w <= 0) continue;
        const glm::vec3 p = inv * (m.positions[i] - o.pivot);
        glm::vec3 q = p;
        const glm::vec3 uv = glm::clamp((p - lo) / extent, glm::vec3(0), glm::vec3(1));
        const float t = o.negativeSide ? 1 - uv[a] : uv[a];
        const float s = sign * (p[a] - anchor);
        switch (op) {
        case GizmoDeformOp::Slice:
        case GizmoDeformOp::MultiSlice:
            break;
        case GizmoDeformOp::BendArc: {
            const float angle = glm::radians(o.amount);
            if (std::abs(angle) < 1e-6f) break;
            const float k = angle / length, theta = k * s, R = 1 / k;
            q[a] = anchor + sign * std::sin(theta) * (R + p[b]);
            q[b] = std::cos(theta) * (R + p[b]) - R;
            break;
        }
        case GizmoDeformOp::BendCurve: {
            const float u = 1 - t;
            q += o.curve[0] * (u * u * u) + o.curve[1] * (3 * u * u * t) + o.curve[2] * (3 * u * t * t) +
                 o.curve[3] * (t * t * t);
            break;
        }
        case GizmoDeformOp::Cage:
        case GizmoDeformOp::CageHard:
        case GizmoDeformOp::CageSoft: {
            glm::vec3 v = uv;
            if (op == GizmoDeformOp::CageSoft) v = v * v * (3.0f - 2.0f * v);
            if (op == GizmoDeformOp::CageHard)
                for (int j = 0; j < 3; ++j)
                    v[j] = std::clamp((v[j] - .25f) * 2.0f, 0.0f, 1.0f);
            for (int j = 0; j < 8; ++j) {
                float k = 1;
                for (int d = 0; d < 3; ++d)
                    k *= j & (1 << d) ? v[d] : 1 - v[d];
                q += o.cage[j] * k;
            }
            break;
        }
        case GizmoDeformOp::Flatten: {
            const float plane = anchor + sign * length * (1 - std::clamp(std::abs(factor), 0.0f, 1.0f));
            q[a] = o.negativeSide ? std::max(p[a], plane) : std::min(p[a], plane);
            break;
        }
        case GizmoDeformOp::Inflate:
            q += inv * m.normals[i] * (factor * length * 0.25f * t);
            break;
        case GizmoDeformOp::Offset:
            q[a] += factor * length;
            break;
        case GizmoDeformOp::Rotate:
        case GizmoDeformOp::Twist: {
            float angle = glm::radians(o.amount) * (op == GizmoDeformOp::Twist ? t : 1.0f);
            q[b] = p[b] * std::cos(angle) - p[c] * std::sin(angle);
            q[c] = p[b] * std::sin(angle) + p[c] * std::cos(angle);
            break;
        }
        case GizmoDeformOp::Scale:
            q[a] *= std::max(0.01f, 1 + factor);
            break;
        case GizmoDeformOp::Skew:
            q[b] += factor * length * t;
            break;
        case GizmoDeformOp::Smooth:
        case GizmoDeformOp::SmoothAll:
            q = inv * (glm::mix(m.positions[i], smooth[i], op == GizmoDeformOp::Smooth ? t : 1.0f) - o.pivot);
            break;
        case GizmoDeformOp::Stretch:
            q[a] = anchor + (p[a] - anchor) * std::max(0.01f, 1 + factor);
            break;
        case GizmoDeformOp::Taper: {
            float k = std::max(0.01f, 1 + factor * std::pow(t, std::clamp(o.exponent, 0.1f, 8.0f)));
            q[b] *= k;
            q[c] *= k;
            break;
        }
        }
        const glm::vec3 delta = (o.pivot + o.frame * q - m.positions[i]) * w;
        if (!std::isfinite(delta.x + delta.y + delta.z)) {
            error = "The deformer produced an invalid position";
            return false;
        }
        out.positions[i] += delta;
        if (out.base.size() == m.vertexCount()) out.base[i] += delta; // preserve the evaluated layer sum
    }
    out.recomputeAllNormals();
    out.markAllDirty();
    return true;
}
void registerGizmoDeformCommands(CommandRegistry& r) {
    using T = ParamSpec::Type;
    r.add({"gizmo_deform",
           "Apply a Gizmo deformer as one undo step. Angles in degrees, other amounts in percent.",
           {{"type", T::String, "Deformer name (Bend Arc, Twist, Taper, etc.)", true},
            {"axis", T::String, "x, y or z", false},
            {"amount", T::Number, "Degrees or percent", false},
            {"pivot", T::String, "Pivot x,y,z", false},
            {"orient", T::String, "Local Euler angles in degrees", false},
            {"negative", T::Bool, "Work from the negative box side", false},
            {"masked", T::Bool, "Respect the mask", false}},
           [](Document& d, const Args& a) -> CommandResult {
               GizmoDeformOp op = GizmoDeformOp::Twist;
               bool found = false;
               for (int i = 0; i < 18; ++i)
                   if (argString(a, "type") == gizmoDeformName(GizmoDeformOp(i))) {
                       op = GizmoDeformOp(i);
                       found = true;
                   }
               if (!found) return {false, "Unknown deformer"};
               GizmoDeformOptions o;
               o.amount = float(argNumber(a, "amount", 0));
               o.negativeSide = argBool(a, "negative", false);
               o.useMask = argBool(a, "masked", true);
               auto vector = [&](const std::string& key, glm::vec3& v) {
                   std::string s = argString(a, key);
                   if (s.empty()) return;
                   std::replace(s.begin(), s.end(), ',', ' ');
                   std::istringstream in(s);
                   in >> v.x >> v.y >> v.z;
               };
               std::string axis = argString(a, "axis", "y");
               if (axis != "x" && axis != "y" && axis != "z") return {false, "Axis must be x, y or z"};
               o.axis = axis[0] - 'x';
               vector("pivot", o.pivot);
               glm::vec3 rotation(0);
               vector("orient", rotation);
               o.frame = glm::mat3_cast(eulerRotation(rotation));
               for (int i = 0; i < 8; ++i)
                   vector("cage" + std::to_string(i), o.cage[i]);
               for (int i = 0; i < 4; ++i)
                   vector("curve" + std::to_string(i), o.curve[i]);
               o.slices = int(argNumber(a, "slices", 3));
               o.exponent = float(argNumber(a, "exponent", 1));
               o.keepVolume = argBool(a, "keep_volume", false);
               Mesh m;
               std::string error;
               if (!gizmoDeform(d.mesh, op, o, m, error)) return {false, error};
               if (m.positions == d.mesh.positions && m.triangles == d.mesh.triangles &&
                   m.polygroups == d.mesh.polygroups && m.quadPartners == d.mesh.quadPartners)
                   return {true, "Deformer unchanged"};
               d.replaceMeshUndoable(std::move(m), gizmoDeformName(op));
               return {true, gizmoDeformName(op)};
           }});
}
} // namespace sf
