// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/Mesh.h"
#include <array>
#include <string>
namespace sf {
class CommandRegistry;
enum class GizmoDeformOp {
    BendArc,
    BendCurve,
    Cage,
    CageHard,
    CageSoft,
    Flatten,
    Inflate,
    Offset,
    Rotate,
    Scale,
    Skew,
    Smooth,
    SmoothAll,
    Stretch,
    Taper,
    Twist,
    Slice,
    MultiSlice
};
const char* gizmoDeformName(GizmoDeformOp op);
struct GizmoDeformOptions {
    int axis = 1;
    int slices = 3;
    float amount = 0; // degrees for Bend/Rotate/Twist; percent of bounds for the rest
    float exponent = 1;
    bool negativeSide = false, useMask = true, keepVolume = false;
    glm::vec3 pivot{0};
    glm::mat3 frame{1};
    std::array<glm::vec3, 8> cage{};  // local displacement of each corner, bit 0 X, 1 Y, 2 Z
    std::array<glm::vec3, 4> curve{}; // cubic Bezier displacement along chosen axis
};
// Pure preview from the original snapshot. Hidden vertices stay; masks blend once. No topology changes.
bool gizmoDeform(const Mesh& source, GizmoDeformOp op, const GizmoDeformOptions& options, Mesh& out,
                 std::string& error);
void registerGizmoDeformCommands(CommandRegistry& registry);
} // namespace sf
