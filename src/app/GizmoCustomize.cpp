// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/App.h"
#include "app/Ui.h"
#include "core/Primitives.h"
#include <IconsLucide.h>
#include <algorithm>
#include <cmath>
#include <sstream>
namespace sf {
namespace {
std::string vecText(const glm::vec3& p) {
    std::ostringstream s;
    s.precision(9);
    s << p.x << ',' << p.y << ',' << p.z;
    return s.str();
}
bool cageOp(GizmoDeformOp op) {
    return op == GizmoDeformOp::Cage || op == GizmoDeformOp::CageSoft || op == GizmoDeformOp::CageHard;
}
} // namespace
void App::beginGizmoDeformer(GizmoDeformOp op) {
    finishGizmoDeformer(true);
    if (doc_.mesh.micro || doc_.mesh.multires || !doc_.mesh.zspheres.empty()) {
        status_ = "Deformers need a mesh without Micro or subdivision levels; bake/delete those first";
        return;
    }
    if (doc_.sculptor.strokeActive() || solverBusy()) {
        status_ = "Finish the stroke before starting a deformer";
        return;
    }
    deformer_.source = std::make_shared<Mesh>(doc_.mesh);
    deformer_.subtool = doc_.activeSubtoolInfo().id;
    deformer_.op = op;
    deformer_.options = {};
    deformer_.options.pivot = gizmo_.pivot;
    deformer_.options.frame = gizmoFrame();
    deformer_.options.useMask = gizmo_.useMask;
    deformer_.changed = false;
    if (op == GizmoDeformOp::Slice || op == GizmoDeformOp::MultiSlice) previewGizmoDeformer();
    status_ = std::string(gizmoDeformName(op)) + ": drag a cone or change the settings, then Accept";
}
void App::previewGizmoDeformer() {
    if (!deformer_.source) return;
    if (doc_.activeSubtoolInfo().id != deformer_.subtool) {
        finishGizmoDeformer(false);
        return;
    }
    Mesh result;
    std::string error;
    if (!gizmoDeform(*deformer_.source, deformer_.op, deformer_.options, result, error)) {
        status_ = error;
        return;
    }
    deformer_.changed = result.positions != deformer_.source->positions;
    doc_.mesh = std::move(result);
    doc_.bvhDirty = true;
}
void App::finishGizmoDeformer(bool accept) {
    if (!deformer_.source) return;
    auto source = std::move(deformer_.source);
    const auto o = deformer_.options;
    const auto op = deformer_.op;
    // Switching Parts via the tray must never put the snapshot into the newly active Part.
    int index = -1;
    for (int i = 0; i < doc_.subtoolCount(); ++i)
        if (doc_.subtools[size_t(i)].id == deformer_.subtool) index = i;
    if (index < 0) return;
    if (index != doc_.activeSubtool) {
        doc_.subtools[size_t(index)].mesh = std::move(*source);
        doc_.subtools[size_t(index)].bvhDirty = true;
        status_ = "Deformer cancelled on SubTool change";
        return;
    }
    doc_.mesh = std::move(*source);
    doc_.bvhDirty = true;
    if (!accept) {
        status_ = "Deformer cancelled";
        return;
    }
    Args a{{"type", gizmoDeformName(op)},
           {"amount", double(o.amount)},
           {"axis", std::string(1, char('x' + o.axis))},
           {"pivot", vecText(o.pivot)},
           {"orient", vecText(eulerDegrees(glm::quat_cast(o.frame)))},
           {"negative", o.negativeSide},
           {"masked", o.useMask},
           {"exponent", double(o.exponent)},
           {"keep_volume", o.keepVolume},
           {"slices", double(o.slices)}};
    for (int i = 0; i < 8; ++i)
        a["cage" + std::to_string(i)] = vecText(o.cage[i]);
    for (int i = 0; i < 4; ++i)
        a["curve" + std::to_string(i)] = vecText(o.curve[i]);
    runCommand("gizmo_deform", a);
}
void App::gizmoToAxis(bool orientation) {
    finishGizmoDeformer(true);
    const bool pivotOnly = gizmo_.unlocked || (inputMods() & SDL_KMOD_ALT);
    if (!pivotOnly) {
        Args a{{"pivot", vecText(gizmo_.pivot)}, {"masked", gizmo_.useMask}};
        if (orientation)
            a["rotate"] = vecText(eulerDegrees(glm::quat_cast(glm::transpose(gizmoFrame()))));
        else
            a["translate"] = vecText(-gizmo_.pivot);
        runCommand("transform", a);
        if (!lastCommandOk_) return;
    }
    if (orientation) {
        gizmo_.orientSurface = false;
        gizmo_.frame = glm::mat3(1);
    } else {
        gizmo_.pivot = glm::vec3(0);
        gizmo_.pivotPlaced = true;
    }
}
void App::drawGizmoCustomize() {
    if (deformer_.source && deformer_.subtool != doc_.activeSubtoolInfo().id) finishGizmoDeformer(false);
    const CanvasRect canvas = canvasRect();
    ImVec2 center;
    if (!gizmoProject(gizmo_.pivot, center)) center = ImVec2(canvas.x + canvas.w * .5f, canvas.y + canvas.h * .5f);
    const float scale = uiScale_;
    const float stripW = 240 * scale;
    ImVec2 strip(std::clamp(center.x - stripW * .5f, canvas.x + 4 * scale,
                            canvas.x + std::max(4 * scale, canvas.w - stripW - 4 * scale)),
                 std::clamp(center.y - 135 * scale, canvas.y + 45 * scale, canvas.y + canvas.h - 38 * scale));
    ImGui::SetNextWindowPos(strip);
    ImGui::SetNextWindowBgAlpha(.78f);
    const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Gizmo Operators", nullptr, flags);
    auto button = [&](const char* icon, const char* id, const char* help, bool active = false) {
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const bool clicked = ImGui::Button((std::string(icon) + "###" + id).c_str(), ImVec2(26 * scale, 26 * scale));
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
        return clicked;
    };
    if (button(ICON_LC_SETTINGS, "customize", "Customize: deformers and primitives", bool(deformer_.source)))
        ImGui::OpenPopup("Transform Type");
    if (deformer_.openMenu) {
        ImGui::OpenPopup("Transform Type");
        deformer_.openMenu = false;
    }
    ImGui::SameLine();
    if (button(ICON_LC_PIN, "sticky", "Sticky: keep pivot at its starting position", gizmo_.sticky))
        gizmo_.sticky = !gizmo_.sticky;
    ImGui::SameLine();
    if (button(ICON_LC_LOCATE_FIXED, "center", "Unmasked Center: center the pivot on the visible, unmasked bounds")) {
        finishGizmoDeformer(true);
        gizmo_.pivotPlaced = false;
        gizmo_.centreKey = ~uint64_t(0);
        updateGizmoPivot();
    }
    ImGui::SameLine();
    if (button(ICON_LC_HOUSE, "axis", "Mesh to Axis: center selection at world origin. Alt/unlock: pivot only"))
        gizmoToAxis(false);
    ImGui::SameLine();
    if (button(ICON_LC_ROTATE_CCW, "orientation", "Reset orientation to world axes. Alt/unlock: axes only"))
        gizmoToAxis(true);
    ImGui::SameLine();
    if (button(gizmo_.unlocked ? ICON_LC_LOCK_OPEN : ICON_LC_LOCK, "lock",
               "Unlock: move/rotate the pivot without changing the mesh", gizmo_.unlocked))
        gizmo_.unlocked = !gizmo_.unlocked;
    ImGui::SameLine();
    if (button(ICON_LC_LAYERS, "multiple", "Transform all visible SubTools (deformers affect the active one)",
               gizmo_.all))
        gizmo_.all = !gizmo_.all;
    if (ImGui::BeginPopup("Transform Type")) {
        ImGui::TextUnformatted("Transform Type");
        ImGui::Separator();
        if (deformer_.source) {
            ImGui::Text("%s", gizmoDeformName(deformer_.op));
            if (ImGui::Button("Accept")) {
                finishGizmoDeformer(true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                deformer_.options.amount = 0;
                deformer_.options.cage = {};
                deformer_.options.curve = {};
                previewGizmoDeformer();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                finishGizmoDeformer(false);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Gizmo 3D")) {
                finishGizmoDeformer(true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
        }
        const float columnWidth = std::max(140 * scale, ImGui::CalcTextSize("Remesh By Quad Remesh").x +
                                                            ImGui::GetStyle().FramePadding.x * 2);
        const char* primNames[] = {"Cone 3D",   "Cylinder 3D", "PolyCube", "PolyCylinder",
                                   "PolyPlane", "PolySphere",  "Ring 3D",  "Sphere 3D"};
        for (int i = 0; i < 8; ++i) {
            if (i % 4) ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::Button(primNames[i], ImVec2(columnWidth, 38 * scale))) {
                finishGizmoDeformer(true);
                // Preserve the sculpt: append a new SubTool. Parametric adjustment lives in Tool > Initialize.
                const char* types[] = {"cone",  "cylinder",   "cube",  "cylinder",
                                       "plane", "quadsphere", "torus", "sphere"};
                runCommand("subtool_append", {{"type", types[i]}});
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Append as a new SubTool; keeps your sculpt. Adjust in Tool > Initialize.");
            ImGui::PopID();
        }
        ImGui::Separator();
        struct Item {
            const char* name;
            int op;
        };
        const Item items[] = {{"Bend Arc", 0},
                              {"Bend Curve", 1},
                              {"Bevel", -1},
                              {"Crease", -1},
                              {"Deformer", 2},
                              {"Deformer Hard", 3},
                              {"Deformer Soft", 4},
                              {"Extender", -1},
                              {"Flatten", 5},
                              {"Inflate", 6},
                              {"Multi Slice", 17},
                              {"Offset", 7},
                              {"Project Primitive", -1},
                              {"Remesh By DynaMesh", -4},
                              {"Remesh By Union", -1},
                              {"Remesh By Quad Remesh", -5},
                              {"Remesh By Decimation", -6},
                              {"Rotate", 8},
                              {"Scale", 9},
                              {"Skew", 10},
                              {"Slice", 16},
                              {"Smooth", 11},
                              {"Smooth All", 12},
                              {"Stretch", 13},
                              {"Subdivide", -7},
                              {"Taper", 14},
                              {"Twist", 15}};
        for (int i = 0; i < int(std::size(items)); ++i) {
            const auto item = items[i];
            if (i % 4) ImGui::SameLine();
            ImGui::BeginDisabled(item.op == -1);
            if (ImGui::Button(item.name, ImVec2(columnWidth, 0))) {
                if (item.op >= 0)
                    beginGizmoDeformer(GizmoDeformOp(item.op));
                else {
                    finishGizmoDeformer(true);
                    if (item.op == -4) startVoxelRemesh();
                    if (item.op == -5) startQRemesh();
                    if (item.op == -6)
                        runCommand("decimate",
                                   {{"triangles", double(std::max(size_t(4), doc_.mesh.triangleCount() / 2))}});
                    if (item.op == -7) runCommand("subdivide", {{"levels", 1.0}});
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (item.op == -1)
                    ImGui::SetTooltip("Not implemented in SculptForge yet; no replacement operation will be applied.");
                else if (item.op == -5)
                    ImGui::SetTooltip(
                        "SculptForge Quad Remesh, using Tool > Quad Remesh settings; independent of ZRemesher.");
                else if (item.op < 0)
                    ImGui::SetTooltip(
                        "Runs the existing topology tool with its current settings; Undo restores the result.");
            }
        }
        ImGui::EndPopup();
    }
    ImGui::End();
    if (!deformer_.source) return;
    ImGui::SetNextWindowPos(ImVec2(canvas.x + 12 * scale, canvas.y + canvas.h - 12 * scale), ImGuiCond_Always,
                            ImVec2(0, 1));
    ImGui::SetNextWindowSize(ImVec2(305 * scale, 0));
    ImGui::Begin("Gizmo Deformer", nullptr, flags);
    ImGui::TextUnformatted(gizmoDeformName(deformer_.op));
    ImGui::TextDisabled("Active SubTool; masks and hidden protected");
    bool changed = false;
    auto& o = deformer_.options;
    changed |= ImGui::Combo("Axis", &o.axis, "X\0Y\0Z\0");
    changed |= ImGui::Checkbox("Negative side", &o.negativeSide);
    if (cageOp(deformer_.op)) {
        ImGui::SliderInt("Corner", &deformer_.corner, 0, 7);
        changed |= ImGui::DragFloat3("Displacement", &o.cage[size_t(deformer_.corner)].x, .005f);
        ImGui::TextWrapped("Drag the cage's corner dots on the model to reshape it.");
    } else if (deformer_.op == GizmoDeformOp::BendCurve) {
        for (int i = 0; i < 4; ++i) {
            ImGui::PushID(i);
            changed |= ImGui::DragFloat3(("Point " + std::to_string(i + 1)).c_str(), &o.curve[i].x, .005f);
            ImGui::PopID();
        }
    } else {
        const bool angle = deformer_.op == GizmoDeformOp::BendArc || deformer_.op == GizmoDeformOp::Rotate ||
                           deformer_.op == GizmoDeformOp::Twist;
        const bool positive = deformer_.op == GizmoDeformOp::Flatten || deformer_.op == GizmoDeformOp::Smooth ||
                              deformer_.op == GizmoDeformOp::SmoothAll;
        const float minimum = positive ? 0.0f : angle ? -180.0f : -99.0f, maximum = angle ? 180.0f : 100.0f;
        changed |= ImGui::InputFloat(angle ? "Angle" : "Amount", &o.amount, 1, 10, "%.1f");
        o.amount = std::clamp(o.amount, minimum, maximum);
        changed |= ImGui::SliderFloat("##amountslider", &o.amount, minimum, maximum, angle ? "%.1f deg" : "%.1f%%");
        if (deformer_.op == GizmoDeformOp::Taper) changed |= ImGui::SliderFloat("Exponent", &o.exponent, .1f, 8);
        if (deformer_.op == GizmoDeformOp::Smooth || deformer_.op == GizmoDeformOp::SmoothAll)
            changed |= ImGui::Checkbox("Keep volume", &o.keepVolume);
    }
    if (deformer_.op == GizmoDeformOp::MultiSlice) changed |= ImGui::SliderInt("Slices", &o.slices, 1, 12);
    if (changed) previewGizmoDeformer();
    if (ImGui::Button("Accept")) {
        finishGizmoDeformer(true);
        ImGui::End();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        o.amount = 0;
        o.cage = {};
        o.curve = {};
        previewGizmoDeformer();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        finishGizmoDeformer(false);
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("Q accepts and returns to Draw. Ctrl+Z cancels.");
    ImGui::End();
    // Oriented cage and working-plane handles. Each drag adjusts the snapshot, never the previous preview.
    AABB box;
    const auto inv = glm::transpose(o.frame);
    for (const auto& p : deformer_.source->positions)
        box.expand(inv * (p - o.pivot));
    ImVec2 corners[8];
    bool shown[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec3 p;
        for (int d = 0; d < 3; ++d)
            p[d] = i & (1 << d) ? box.max[d] : box.min[d];
        if (cageOp(deformer_.op)) p += o.cage[i];
        shown[i] = gizmoProject(o.pivot + o.frame * p, corners[i]);
    }
    auto* dl = ImGui::GetForegroundDrawList();
    for (int i = 0; i < 8; ++i)
        for (int d = 0; d < 3; ++d)
            if (!(i & (1 << d)) && shown[i] && shown[i | (1 << d)])
                dl->AddLine(corners[i], corners[i | (1 << d)], IM_COL32(255, 180, 72, 180), 1.5f * scale);
    const int handles = cageOp(deformer_.op) ? 8 : deformer_.op == GizmoDeformOp::BendCurve ? 4 : 1;
    for (int i = 0; i < handles; ++i) {
        ImVec2 at;
        if (cageOp(deformer_.op)) {
            if (!shown[i]) continue;
            at = corners[i];
        } else if (deformer_.op == GizmoDeformOp::BendCurve) {
            glm::vec3 p = box.center();
            p[o.axis] = glm::mix(box.min[o.axis], box.max[o.axis], i / 3.0f);
            p += o.curve[i];
            if (!gizmoProject(o.pivot + o.frame * p, at)) continue;
        } else {
            if (!shown[7]) continue;
            at = ImVec2(corners[7].x + 20 * scale,
                        std::max(corners[7].y - 20 * scale, canvas.y + timelineTopInset_ + 54 * scale));
        }
        if (!canvas.contains(at.x, at.y)) continue;
        ImGui::SetNextWindowPos(ImVec2(at.x - 9 * scale, at.y - 9 * scale));
        ImGui::SetNextWindowSize(ImVec2(18 * scale, 18 * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(("Deformer handle##" + std::to_string(i)).c_str(), nullptr,
                     flags | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        ImGui::InvisibleButton("Drag", ImVec2(18 * scale, 18 * scale));
        const bool active = ImGui::IsItemActive();
        dl->AddCircleFilled(at, 6 * scale, active ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 165, 45, 255));
        if (active && ImGui::IsMouseDragging(0)) {
            const auto delta = ImGui::GetIO().MouseDelta;
            if (cageOp(deformer_.op) || deformer_.op == GizmoDeformOp::BendCurve) {
                const glm::mat3 cameraWorld = glm::transpose(glm::mat3(camera_.view()));
                const glm::vec3 motion = inv * (cameraWorld[0] * delta.x - cameraWorld[1] * delta.y) *
                                         camera_.worldPerPixel(gizmo_.pivot, canvasHeight());
                if (cageOp(deformer_.op)) {
                    o.cage[i] += motion;
                    deformer_.corner = i;
                } else
                    o.curve[i] += motion;
            } else
                o.amount = std::clamp(o.amount + (delta.x - delta.y) * .4f, -180.0f, 180.0f);
            previewGizmoDeformer();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag to adjust %s", gizmoDeformName(deformer_.op));
        ImGui::End();
        ImGui::PopStyleVar();
    }
}
} // namespace sf
