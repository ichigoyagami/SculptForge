// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/App.h"
#include "app/Shell.h"
#include "app/Theme.h"
#include "app/Ui.h"
#include "core/Micro.h"
#include "core/StudioTools.h"
#include "core/SubTools.h"
#include "core/Timeline.h"
#include <IconsLucide.h>
#include <algorithm>
#include <cmath>

namespace sf {
void App::registerScreenshotActions() {
    auto action = [&](const char* id, const char* label, const char* palette, const char* help,
                      std::function<void()> run, std::function<bool()> checked = nullptr,
                      std::function<bool()> enabled = nullptr) {
        const std::string key(id);
        const char* icon = key == "studio.persp"    ? ICON_LC_BOX
                           : key == "studio.floor"  ? ICON_LC_GRID_3X3
                           : key == "studio.transp" ? ICON_LC_LAYERS_2
                           : key == "studio.ghost"  ? ICON_LC_GHOST
                                                    : nullptr;
        actions_.add({id, label, icon, help, palette, "", "", std::move(run), std::move(checked), std::move(enabled)});
    };
    auto command = [&](const char* id, const char* label, const char* palette, const char* name, const char* help) {
        action(id, label, palette, help, [this, name] { runCommand(name); });
    };
    action(
        "workspace.reference", "Screenshot layout", "Preferences > Interface",
        "Top brush controls, bottom geometry controls and side navigation, based on your reference.",
        [this] {
            layout_.referenceShelves = true;
            noteLayoutChanged();
        },
        [this] { return layout_.referenceShelves; });
    action(
        "workspace.classic", "Classic shelves", "Preferences > Interface",
        "Return to the previous customizable shelves and bottom brush strip.",
        [this] {
            layout_.referenceShelves = false;
            noteLayoutChanged();
        },
        [this] { return !layout_.referenceShelves; });
    commands_.add({"workspace_reference",
                   "Choose the screenshot-based shelves.",
                   {{"enabled", ParamSpec::Type::Bool, "Enable the reference shelves", false}},
                   [this](Document&, const Args& a) -> CommandResult {
                       layout_.referenceShelves = argBool(a, "enabled", true);
                       return {true, "Workspace changed"};
                   }});
    action(
        "studio.tools", "Sculpt tools", "Tool", "Open the added mesh, stroke and posing controls.",
        [this] { showStudioTools_ = !showStudioTools_; }, [this] { return showStudioTools_; });
    command("subtool.duplicate", "Duplicate", "Tool > SubTool", "subtool_duplicate", "Duplicate the active SubTool.");
    command("subtool.split_groups", "Groups Split", "Tool > SubTool", "subtool_split_groups",
            "Split the mesh into one SubTool per PolyGroup.");
    command("subtool.show_all", "All Visible", "Tool > SubTool", "subtool_show_all", "Show all SubTools.");
    command("studio.flip", "Flip", "Tool > Display Properties", "mesh_flip",
            "Reverse polygon normals and UV winding. Undo restores the mesh.");
    command("studio.weld", "WeldPoints", "Tool > Geometry > Modify Topology", "weld_points",
            "Weld coincident vertices. Removes duplicate seam points while preserving attributes.");
    command("studio.close_holes", "Close Holes", "Tool > Geometry > Modify Topology", "close_holes",
            "Cap manifold boundary loops. New caps can contain triangles.");
    command("studio.store_mt", "StoreMT", "Tool > Morph Target", "morph_store",
            "Store the current surface for this SubTool and subdivision layout. Saved with the project.");
    command("studio.switch_mt", "Switch MT", "Tool > Morph Target", "morph_switch",
            "Swap current and stored geometry when vertex order and topology match.");
    command("studio.delete_mt", "DelMT", "Tool > Morph Target", "morph_delete",
            "Delete the stored Morph Target. Undo restores it.");
    command("studio.split_masked", "Split Masked Points", "Tool > SubTool > Split", "subtool_split_masked",
            "Split masked faces into a new SubTool. One undo step restores both parts.");
    command("studio.auto_groups", "Auto Groups", "Tool > Polygroups", "polygroup_by_connectivity",
            "Assign a PolyGroup to each connected mesh part.");
    command("studio.uv_groups", "UV Groups", "Tool > Polygroups", "polygroup_uv",
            "Assign a PolyGroup to each UV island.");
    command("studio.del_hidden", "Del Hidden", "Tool > Geometry > Modify Topology", "delete_hidden",
            "Permanently remove hidden geometry; Undo restores it.");
    command("studio.fill", "FillObject", "Color", "paint_fill",
            "Fill the selected paint target with the current color.");
    action(
        "studio.groups_loops", "GroupsLoops", "Tool > Geometry > EdgeLoop",
        "Inset each visible PolyGroup and add boundary edge loops. Uses a centroid inset, so check concave shapes.",
        [this] { runCommand("groups_loops", {{"inset", double(groupInset_)}, {"loops", double(groupLoopCount_)}}); });
    action("studio.panel_loops", "Panel Loops", "Tool > Geometry > EdgeLoop",
           "Create separate thick panels from visible PolyGroups, with inset borders and quad side walls.", [this] {
               runCommand("panel_loops", {{"inset", double(groupInset_)},
                                          {"loops", double(groupLoopCount_)},
                                          {"thickness", double(panelThickness_)}});
           });
    action(
        "studio.double", "Double", "Tool > Display Properties",
        "Shade both sides of a polygon. Turning it off culls back-facing triangles.",
        [this] { renderSettings_.doubleSided = !renderSettings_.doubleSided; },
        [this] { return renderSettings_.doubleSided; });
    action(
        "studio.persp", "Persp", "Draw > Camera", "Switch perspective and orthographic viewing.",
        [this] {
            camera_.ortho = !camera_.ortho;
            autoOrtho_ = false;
        },
        [this] { return !camera_.ortho; });
    action(
        "studio.floor", "Floor", "Draw > Camera", "Show the world XZ floor grid, with depth occlusion.",
        [this] { renderSettings_.showFloor = !renderSettings_.showFloor; },
        [this] { return renderSettings_.showFloor; });
    action(
        "studio.transp", "Transp", "Tool > Display Properties",
        "Toggle model opacity between solid and 35 percent. The independent opacity slider remains available.",
        [this] { renderSettings_.meshOpacity = renderSettings_.meshOpacity < .99f ? 1 : .35f; },
        [this] { return renderSettings_.meshOpacity < .99f; });
    action(
        "studio.ghost", "Ghost", "Tool > Display Properties",
        "Dim inactive SubTools so the active sculpt reads clearly.",
        [this] { renderSettings_.dimInactive = !renderSettings_.dimInactive; },
        [this] { return renderSettings_.dimInactive; });
    action(
        "studio.xyz", "XYZ", "Transform", "Enable all three mirror axes, or disable them together.",
        [this] {
            bool on = !(symmetry_.x && symmetry_.y && symmetry_.z);
            symmetry_ = {on, on, on};
        },
        [this] { return symmetry_.x && symmetry_.y && symmetry_.z; });
    action(
        "studio.lazy", "LazyMouse", "Stroke > Lazy Mouse",
        "Stabilize the brush path. LazyRadius controls the screen distance the brush trails behind the cursor.",
        [this] { strokeType_ = strokeType_ == StrokeType::LazyMouse ? StrokeType::Dots : StrokeType::LazyMouse; },
        [this] { return strokeType_ == StrokeType::LazyMouse; });
    action("studio.replay_relative", "ReplayLastRel", "Stroke",
           "Replay at the cursor. Shift+1 starts at the live canvas position. A shelf click uses the last canvas "
           "position.",
           [this] {
               if (lastStrokePath_.empty() || !lastReplayCommand_.empty()) { status_ = "Relative replay requires a recorded brush stroke"; return; }
               auto c = canvasRect();
               glm::vec2 at = canvasRect().contains(mouseX_, mouseY_) ? glm::vec2(mouseX_, mouseY_) : lastCanvasPoint_;
               replayOffset_ = at - glm::vec2(c.x + lastStrokePath_[0].uv.x * c.w, c.y + lastStrokePath_[0].uv.y * c.h);
               replayLastStroke();
               replayOffset_ = {0, 0};
           });
    actions_.setDefaultShortcut("studio.replay_relative", "Shift+1");
    action(
        "studio.interpolate", "Interpolate", "Stroke",
        "Replay evenly spaced in-between paths between your two last sculpt strokes, using the current brush settings. "
        "Each inserted path is undoable.",
        [this] { interpolateStrokes(); }, nullptr,
        [this] { return !lastStrokePath_.empty() && !previousStrokePath_.empty(); });
    action("studio.half", "Half", "Tool > Geometry > QRemesh",
           "Set the QRemesh target to half the current polygon count.",
           [this] { retopo_.target = std::max(100, int(doc_.mesh.triangleCount() / 4)); });
    action("studio.same", "Same", "Tool > Geometry > QRemesh", "Set the QRemesh target to the current polygon count.",
           [this] { retopo_.target = std::max(100, int(doc_.mesh.triangleCount() / 2)); });
    action("studio.double_target", "Double target", "Tool > Geometry > QRemesh",
           "Set the QRemesh target to twice the current polygon count.",
           [this] { retopo_.target = std::max(100, int(doc_.mesh.triangleCount())); });
    action(
        "studio.qremesh", "QRemesh", "Tool > Geometry > QRemesh",
        "Rebuild quad topology using the target, adaptation, symmetry and Keep Groups settings.",
        [this] { startQRemesh(); }, nullptr, [this] { return !qremesh_; });
    action(
        "studio.keep_groups", "KeepGroups", "Tool > Geometry > QRemesh",
        "Preserve PolyGroup boundaries during QRemesh.", [this] { retopo_.keepGroups = !retopo_.keepGroups; },
        [this] { return retopo_.keepGroups; });
    action(
        "studio.adapt", "Adapt", "Tool > Geometry > QRemesh",
        "Toggle adaptive quad sizes (0 or 0.5). The full slider is in Geometry.",
        [this] { retopo_.adaptive = retopo_.adaptive > 0 ? 0 : .5f; }, [this] { return retopo_.adaptive > 0; });
    action("studio.decimate", "Decimate Current", "Tool > Geometry",
           "Reduce triangle count using the Geometry palette's Keep ratio.",
           [this] { runCommand("decimate", {{"ratio", double(decimateRatio_)}}); });
    action("studio.copy", "Copy Tool", "Tool", "Copy the current SubTool mesh into an in-app clipboard.", [this] {
        if (doc_.sculptor.strokeActive()) endStroke();
        toolClipboard_ = std::make_unique<Mesh>(doc_.mesh);
        status_ = "SubTool copied";
    });
    action(
        "studio.paste", "Paste Tool", "Tool", "Append a copy of the in-app mesh clipboard as a new SubTool.",
        [this] {
            if (doc_.sculptor.strokeActive()) endStroke();
            if (toolClipboard_) {
                doc_.addSubtool(*toolClipboard_, "Pasted Tool");
                status_ = "SubTool pasted";
            }
        },
        nullptr, [this] { return bool(toolClipboard_); });
    action(
        "studio.tpose_mesh", "TPoseMesh", "Transform > Transpose Master",
        "Create a combined temporary mesh from visible SubTools for posing. Keep its topology unchanged, then use "
        "TPose SubT.",
        [this] { transposeMaster(false); }, nullptr, [this] { return poseParts_.empty(); });
    action(
        "studio.tpose_subtools", "TPose SubT", "Transform > Transpose Master",
        "Transfer the temporary mesh's vertex offsets back to the original SubTools. Each original SubTool gets an "
        "undo step.",
        [this] { transposeMaster(true); }, nullptr, [this] { return !poseParts_.empty(); });
    action(
        "studio.cancel_pose", "Cancel Pose", "Transform > Transpose Master",
        "Discard the temporary pose and restore original SubTool visibility.",
        [this] {
            if (poseParts_.empty()) return;
            int first = -1;
            for (auto& part : poseParts_) {
                int i = doc_.subtoolIndex(part.id);
                if (i >= 0) {
                    doc_.subtools[size_t(i)].visible = part.visible;
                    if (first < 0) first = i;
                }
            }
            if (first >= 0) doc_.selectSubtool(first);
            int temp = doc_.subtoolIndex(poseSubtool_);
            if (temp >= 0 && temp != doc_.activeSubtool) doc_.removeSubtool(temp);
            poseParts_.clear();
            poseSubtool_ = 0;
            status_ = "Temporary pose cancelled";
        },
        nullptr, [this] { return !poseParts_.empty(); });
}

void App::applyAdjustLast(float value) {
    if (doc_.sculptor.strokeActive()) endStroke();
    Timeline* t = Timeline::of(doc_.undo);
    if (!t || t->cursor() == 0) {
        status_ = "Sculpt a stroke before Adjust Last";
        return;
    }
    t->sync(doc_);
    auto id = t->cursorNode();
    std::string error;
    if (!t->retune(doc_, id, value, &error)) {
        status_ = error;
        return;
    }
    adjustLastValue_ = value;
    adjustLastNode_ = id;
    doc_.modified = true;
    status_ = "Adjusted last stroke";
}
void App::interpolateStrokes() {
    if (lastStrokePath_.empty() || previousStrokePath_.empty() || drag_ != Drag::None) return;
    auto a = previousStrokePath_, b = lastStrokePath_;
    size_t count = std::min<size_t>(4096, std::max(a.size(), b.size()));
    auto sample = [](const auto& path, float at) {
        float f = at * float(path.size() - 1);
        size_t i = size_t(f), j = std::min(i + 1, path.size() - 1);
        ReplaySample p;
        p.uv = glm::mix(path[i].uv, path[j].uv, f - float(i));
        p.pressure = glm::mix(path[i].pressure, path[j].pressure, f - float(i));
        return p;
    };
    for (int k = 1; k <= std::clamp(interpolateCount_, 1, 32); ++k) {
        float f = float(k) / float(interpolateCount_ + 1);
        lastStrokePath_.clear();
        for (size_t i = 0; i < count; ++i) {
            float u = count > 1 ? float(i) / float(count - 1) : 0;
            auto p = sample(a, u), q = sample(b, u);
            p.uv = glm::mix(p.uv, q.uv, f);
            p.pressure = glm::mix(p.pressure, q.pressure, f);
            lastStrokePath_.push_back(p);
        }
        replayLastStroke(false);
    }
    previousStrokePath_ = std::move(a);
    lastStrokePath_ = std::move(b);
    status_ = "Interpolated stroke paths";
}
void App::transposeMaster(bool apply) {
    if (doc_.sculptor.strokeActive()) endStroke();
    if (!apply) {
        std::vector<const Mesh*> refs;
        poseParts_.clear();
        for (int i = 0; i < doc_.subtoolCount(); ++i)
            if (doc_.subtools[size_t(i)].visible) {
                auto& m = doc_.subtoolMesh(i);
                if (hasMicro(m)) {
                    status_ = "Bake Micro before TPoseMesh";
                    poseParts_.clear();
                    return;
                }
                poseParts_.push_back({doc_.subtools[size_t(i)].id, m.positions, true, meshTopologyKey(m)});
                refs.push_back(&m);
            }
        if (refs.empty()) {
            status_ = "No visible SubTools to pose";
            return;
        }
        Mesh merged = mergeMeshes(refs);
        uint64_t key = meshTopologyKey(merged);
        for (auto& p : poseParts_)
            doc_.subtools[size_t(doc_.subtoolIndex(p.id))].visible = false;
        int added = doc_.addSubtool(std::move(merged), "TPose Mesh");
        poseSubtool_ = doc_.subtools[size_t(added)].id;
        poseTopology_ = key;
        status_ = "Pose the combined mesh, then press TPose SubT. Keep its topology unchanged.";
        return;
    }
    int pose = doc_.subtoolIndex(poseSubtool_);
    if (pose < 0) {
        status_ = "The TPose mesh was deleted";
        return;
    }
    const Mesh& posed = doc_.subtoolMesh(pose);
    size_t count = 0;
    for (auto& part : poseParts_) {
        int i = doc_.subtoolIndex(part.id);
        if (i < 0 || doc_.subtoolMesh(i).vertexCount() != part.positions.size() ||
            meshTopologyKey(doc_.subtoolMesh(i)) != part.topologyKey) {
            status_ = "An original SubTool changed topology; TPose transfer stopped";
            return;
        }
        count += part.positions.size();
    }
    if (count != posed.vertexCount() || poseTopology_ != meshTopologyKey(posed)) {
        status_ = "TPose mesh topology changed; undo those edits before transfer";
        return;
    }
    const auto result = posed.positions;
    size_t offset = 0;
    for (auto& part : poseParts_) {
        int i = doc_.subtoolIndex(part.id);
        Mesh& m = doc_.subtoolMesh(i);
        UndoRecord record;
        record.label = "Transpose Master pose";
        record.mesh = std::make_unique<Mesh>(m);
        auto& target = m.layerTarget(-1);
        for (size_t v = 0; v < part.positions.size(); ++v)
            target[v] += result[offset + v] - part.positions[v];
        offset += part.positions.size();
        if (!m.layers.empty()) m.evaluateAll();
        m.recomputeAllNormals();
        (i == doc_.activeSubtool ? doc_.undo : doc_.subtools[size_t(i)].undo).push(std::move(record));
        if (i == doc_.activeSubtool)
            doc_.bvhDirty = true;
        else
            doc_.subtools[size_t(i)].bvhDirty = true;
        doc_.subtools[size_t(i)].visible = part.visible;
    }
    int first = doc_.subtoolIndex(poseParts_.front().id);
    doc_.selectSubtool(first);
    doc_.removeSubtool(doc_.subtoolIndex(poseSubtool_));
    poseParts_.clear();
    poseSubtool_ = 0;
    doc_.modified = true;
    status_ = "Pose transferred to original SubTools";
}

void App::drawStudioControls(int section) {
    auto row = [&](std::initializer_list<const char*> ids) {
        bool first = true;
        for (auto id : ids) {
            if (!first) ImGui::SameLine();
            first = false;
            ui::ActionButton(*this, id);
        }
    };
    if (section == 1) {
        row({"studio.replay_relative", "studio.lazy"});
        ImGui::SliderFloat("LazyRadius", &lazyMouse_.radius, 0, 200, "%.0f");
        if (auto* t = Timeline::of(doc_.undo); t && t->cursor() > 0) {
            auto id = t->cursorNode();
            if (id != adjustLastNode_) {
                adjustLastNode_ = id;
                adjustLastValue_ = t->node(id).strength;
            }
        }
        float value = adjustLastValue_;
        if (ImGui::SliderFloat("AdjustLast", &value, -1, 2, "%.2f")) applyAdjustLast(value);
        ImGui::SliderInt("Strokes Count", &interpolateCount_, 1, 32);
        row({"studio.interpolate"});
    } else if (section == 2) {
        row({"studio.persp", "studio.floor"});
        float value = cropFactor_;
        if (ImGui::SliderFloat("Crop factor", &value, .5f, 3, "%.2f")) {
            float mm = 12 / (cropFactor_ * std::tan(camera_.fovY * .5f));
            cropFactor_ = value;
            camera_.fovY = 2 * std::atan(12 / (cropFactor_ * mm));
        }
    } else if (section == 3) {
        row({"studio.weld", "studio.close_holes"});
        row({"studio.groups_loops", "studio.panel_loops"});
        ImGui::SliderInt("Loops", &groupLoopCount_, 1, 8);
        ImGui::SliderFloat("Group inset", &groupInset_, .001f, .3f, "%.3f");
        ImGui::SliderFloat("Panel thickness", &panelThickness_, .001f, .1f, "%.3f");
    } else if (section == 4) {
        row({"studio.store_mt", "studio.switch_mt", "studio.delete_mt"});
        ImGui::TextDisabled(!doc_.mesh.morphTarget.empty() ? "Stored target; topology checked on Switch"
                                                      : "No Morph Target stored");
    } else if (section == 5) {
        row({"studio.split_masked"});
        row({"studio.copy", "studio.paste"});
    } else if (section == 6) {
        row({"studio.flip", "studio.double"});
        row({"studio.transp", "studio.ghost"});
    } else if (section == 7) {
        row({"studio.tpose_mesh", "studio.tpose_subtools"});
        row({"studio.cancel_pose"});
    }
}

void App::drawScreenshotShelf(int shelf, const CanvasRect& rect) {
    if (rect.w <= 0 || rect.h <= 0) return;
    const float s = uiScale_;
    const char* names[] = {"##ReferenceTop", "##ReferenceLeft", "##ReferenceRight", "##ReferenceBottom"};
    ImGui::SetNextWindowPos({rect.x, rect.y});
    ImGui::SetNextWindowSize({rect.w, rect.h});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::shellBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4 * s, 3 * s});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {3 * s, 3 * s});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {5 * s, 3 * s});
    ImGui::PushFont(fontSmall_);
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoNav |
                 (shelf == kShelfTop || shelf == kShelfBottom ? ImGuiWindowFlags_HorizontalScrollbar : 0);
    ImGui::Begin(names[shelf], nullptr, flags);
    bool first = true;
    auto next = [&] {
        if (!first) ImGui::SameLine();
        first = false;
    };
    auto newline = [&] { first = true; };
    auto button = [&](const char* id, const char* alias = nullptr, float width = 0) {
        const Action* a = actions_.find(id);
        if (!a) return;
        next();
        if (alias) {
            bool enabled = !a->enabled || a->enabled();
            ImGui::BeginDisabled(!enabled);
            bool on = a->checked && a->checked();
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
            if (ImGui::Button((std::string(alias) + "##ref." + id).c_str(), {width * s, 22 * s}) && a->run) a->run();
            if (on) ImGui::PopStyleColor();
            ImGui::EndDisabled();
            ui::HelpAction(*this, id);
        } else
            ui::ActionButton(*this, id, {width * s, 22 * s}, ui::Look::Label);
    };
    auto slider = [&](const char* id, float* value, float lo, float hi, const char* format, float width) {
        next();
        ImGui::SetNextItemWidth(width * s);
        return ui::SliderInside(id, value, lo, hi, format);
    };
    if (shelf == kShelfTop) {
        button("ui.lightbox");
        button("tool.draw", "Draw");
        button("tool.move", "Move");
        button("tool.scale", "Scale");
        button("tool.rotate", "Rotate");
        button("draw.sculpt");
        button("draw.paint");
        button("draw.rgb");
        button("draw.zadd", "Zadd");
        button("draw.zsub", "Zsub");
        next();
        ImGui::SetNextItemWidth(130 * s);
        drawZIntensitySlider("##ref.intensity");
        slider("##ref.drawsize", &radiusPx_, 2, 800, "Draw Size %.0f", 125);
        float focal = currentFocalShift() * 100;
        if (slider("##ref.focal", &focal, -100, 100, "Focal Shift %.0f", 125)) currentFocalShift() = focal / 100;
        next();
        size_t total = 0;
        for (int i = 0; i < doc_.subtoolCount(); ++i)
            total += doc_.subtoolMesh(i).vertexCount();
        ImGui::Text("Active Points %zu   Total Points %zu", doc_.mesh.vertexCount(), total);
        newline();
        button("stroke.replay_last", "ReplayLast");
        button("studio.replay_relative");
        if (auto* t = Timeline::of(doc_.undo); t && t->cursor() > 0 && adjustLastNode_ != t->cursorNode()) {
            adjustLastNode_ = t->cursorNode();
            adjustLastValue_ = t->node(adjustLastNode_).strength;
        }
        float adjust = adjustLastValue_;
        if (slider("##ref.adjustlast", &adjust, -1, 2, "AdjustLast %.2f", 120)) applyAdjustLast(adjust);
        button("studio.lazy");
        slider("##ref.lazy_radius", &lazyMouse_.radius, 0, 200, "LazyRadius %.0f", 105);
        button("studio.flip");
        button("studio.double");
        button("draw.front_faces", "BackfaceMask");
        button("studio.store_mt");
        button("studio.delete_mt");
        button("studio.interpolate");
        next();
        ImGui::SetNextItemWidth(100 * s);
        ImGui::SliderInt("##ref.interpolate_count", &interpolateCount_, 1, 32, "Strokes %d");
        newline();
        button("draw.dynamic", "Sculptris Pro");
        button("transform.symmetry_x");
        button("transform.symmetry_y");
        button("transform.symmetry_z");
        button("studio.xyz");
        button("draw.pressure_strength");
        button("draw.pressure_size");
        button("ui.brush_lab");
        float smooth = smoothStrength_ * 100;
        if (slider("##smooth_strength_shelf", &smooth, 0, 100, "Smooth Intensity %.0f", 140))
            smoothStrength_ = smooth / 100;
        next();
        ImGui::SetNextItemWidth(108 * s);
        int mode = int(smoothMode_);
        if (ImGui::Combo("##smooth_mode_shelf", &mode, "Standard\0Stronger\0Volume\0")) smoothMode_ = SmoothMode(mode);
        next();
        ImGui::SetNextItemWidth(140 * s);
        ImGui::SliderInt("##smooth_passes_shelf", &smoothPasses_, 0, 64,
                         smoothPasses_ ? "Smooth passes %d" : "Smooth passes: Auto");
        next();
        ImGui::Checkbox("Clay follows stroke", &clayFollowStroke_);
        newline();
        button("workspace.classic");
        button("reference.panel", "Spotlight");
        button("view.timeline", "Timeline");
        button("geometry.remesh");
        next();
        ImGui::TextDisabled("Ctrl: Mask   Ctrl+Shift: Select / Knife / Clip   1: Replay   Shift+1: Relative replay");
    } else if (shelf == kShelfBottom) {
        float mm = 12 / (cropFactor_ * std::tan(camera_.fovY * .5f));
        if (slider("##ref.focal_length", &mm, 15, 300, "Focal length (mm) %.0f", 165))
            camera_.fovY = 2 * std::atan(12 / (cropFactor_ * mm));
        float crop = cropFactor_;
        if (slider("##ref.crop_factor", &crop, .5f, 3, "Crop factor %.2f", 125)) {
            cropFactor_ = crop;
            camera_.fovY = 2 * std::atan(12 / (cropFactor_ * mm));
        }
        button("deformation.mirror", "Mirror");
        button("studio.qremesh");
        button("studio.half");
        button("studio.same");
        button("studio.double_target", "Double");
        button("studio.adapt");
        button("studio.keep_groups");
        next();
        ImGui::SetNextItemWidth(140 * s);
        ImGui::SliderInt("##ref.qtarget", &retopo_.target, 100, 50000, "Target %d");
        newline();
        button("studio.auto_groups");
        button("studio.del_hidden");
        button("studio.decimate");
        button("geometry.mirror_weld");
        button("studio.weld");
        button("studio.close_holes");
        button("subtool.duplicate", "Duplicate");
        button("subtool.split_groups", "Groups Split");
        button("studio.split_masked");
        newline();
        button("geometry.lower_res", "Low Res");
        button("geometry.higher_res", "High Res");
        button("studio.uv_groups");
        button("reference.panel", "Spotlight");
        button("reference.visible", "Spotlight visible");
        button("file.export_glb", "Export");
        button("studio.flip");
        button("subtool.show_all", "All Visible");
        button("file.save_as");
        next();
        ImGui::SetNextItemWidth(145 * s);
        ImGui::SliderInt("##dynamesh_resolution_shelf", &voxelResolution_, 32, 400, "DynaMesh Res %d");
    } else if (shelf == kShelfLeft) {
        auto tile = [&](const char* which) {
            if (which[0] == 'b')
                drawBrushTile("##ref.brush", {70 * s, 80 * s});
            else if (which[0] == 's')
                drawStrokeTile("##ref.stroke", {70 * s, 80 * s});
            else if (which[0] == 'a')
                drawAlphaTile("##ref.alpha", {70 * s, 80 * s});
            else
                drawMaterialTile("##ref.material", {70 * s, 80 * s});
        };
        tile("brush");
        tile("stroke");
        tile("alpha");
        tile("material");
        drawColorWidget(70 * s);
        auto verticalButton = [&](const char* id, const char* alias = nullptr) {
            newline();
            button(id, alias, 72);
        };
        verticalButton("color.switch", "SwitchColor");
        verticalButton("studio.fill");
        verticalButton("studio.tpose_mesh");
        verticalButton("studio.tpose_subtools");
        verticalButton("geometry.remesh");
        verticalButton("studio.groups_loops");
        verticalButton("studio.panel_loops");
        verticalButton("reference.panel", "Reference");
    } else {
        auto verticalButton = [&](const char* id, const char* alias = nullptr) {
            newline();
            button(id, alias, 42);
        };
        verticalButton("studio.persp");
        verticalButton("studio.floor");
        verticalButton("studio.double");
        verticalButton("studio.xyz");
        verticalButton("view.frame", "Frame");
        canvasNavButton("ref.move", "", "Move", 0);
        canvasNavButton("ref.zoom", "", "Zoom", 1);
        canvasNavButton("ref.rotate", "", "Rotate", 2);
        verticalButton("view.polyf", "PolyF");
        verticalButton("studio.transp");
        verticalButton("studio.ghost");
        verticalButton("subtool.solo", "Solo");
        verticalButton("view.polygroups", "Groups");
        verticalButton("view.mask", "Mask");
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}
} // namespace sf
