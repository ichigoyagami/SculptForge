// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/App.h"
#include <imgui_internal.h>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace sf {

void App::drawHistoryBar() {
    const CanvasRect c = canvasRect();
    if (!layout_.shelvesVisible || c.w < 240*uiScale_) return;
    const float s=uiScale_;
    ImGui::SetNextWindowPos(ImVec2(c.x+8*s,c.y+timelineTopInset_+8*s));
    ImGui::SetNextWindowSize(ImVec2(c.w-(showViewCube_?216:16)*s,0));
    ImGui::SetNextWindowBgAlpha(0.87f);
    const auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
                     ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing;
    if(ImGui::Begin("SubTool History",nullptr,flags)) {
        ImGui::PushFont(fontSmall_);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4*s,3*s));
        const std::string name=doc_.activeSubtoolInfo().name.substr(0,20);
        ImGui::Text("%s",name.c_str());ImGui::SameLine();
        ImGui::BeginDisabled(doc_.sculptor.strokeActive() || !doc_.undo.canUndo());
        if(ImGui::Button("Undo")) runCommand("undo");
        ImGui::EndDisabled();ImGui::SameLine();
        ImGui::BeginDisabled(doc_.sculptor.strokeActive() || !doc_.undo.canRedo());
        if(ImGui::Button("Redo")) runCommand("redo");
        ImGui::EndDisabled();ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const int total=int(doc_.undo.historyCount());int position=int(doc_.undo.undoCount());
        const std::string format="History %d / "+std::to_string(total);
        ImGui::BeginDisabled(doc_.sculptor.strokeActive() || total==0);
        if(ImGui::SliderInt("##history_position",&position,0,total,format.c_str())) seekHistory(size_t(position));
        ImGui::EndDisabled();
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s\nEach SubTool has its own history. Drag to undo or redo.\nUse Sculpt Timeline for branches and optional Save History beside the project.",
                              doc_.undo.historyLabel(doc_.undo.undoCount()).c_str());
        }
        ImGui::PopStyleVar();ImGui::PopFont();
    }
    ImGui::End();
}

void App::registerViewportActions() {
    commands_.add({"reference_load", "Load an image into the Spotlight reference overlay.",
                   {{"path", ParamSpec::Type::String, "Image filename", true}},
                   [this](Document&, const Args& a) { bool ok = loadSpotlight(argString(a, "path")); return CommandResult{ok, status_}; }});
    commands_.add({"reference_settings", "Adjust reference and mesh opacity and screen placement.",
                   {{"opacity", ParamSpec::Type::Number, "Reference opacity, 0 to 1"},
                    {"mesh_opacity", ParamSpec::Type::Number, "Mesh opacity, 0 to 1"},
                    {"scale", ParamSpec::Type::Number, "Reference scale"},
                    {"rotation", ParamSpec::Type::Number, "Clockwise degrees"},
                    {"x", ParamSpec::Type::Number, "Horizontal canvas offset"},
                    {"y", ParamSpec::Type::Number, "Vertical canvas offset"},
                    {"front", ParamSpec::Type::Bool, "Overlay in front of mesh"},
                    {"visible", ParamSpec::Type::Bool, "Show reference"},
                    {"edit", ParamSpec::Type::Bool, "Edit placement instead of sculpting"}},
                   [this](Document&, const Args& a) {
                       auto value = [&](const char* key, float current, float lo, float hi) {
                           double v = argNumber(a, key, current);
                           return std::isfinite(v) ? std::clamp(float(v), lo, hi) : current;
                       };
                       spotlight_.opacity = value("opacity", spotlight_.opacity, 0, 1);
                       renderSettings_.meshOpacity = value("mesh_opacity", renderSettings_.meshOpacity, 0, 1);
                       spotlight_.scale = value("scale", spotlight_.scale, 0.05f, 4);
                       spotlight_.rotation = value("rotation", spotlight_.rotation, -180, 180);
                       spotlight_.offset = {value("x", spotlight_.offset.x, -2, 2), value("y", spotlight_.offset.y, -2, 2)};
                       spotlight_.inFront = argBool(a, "front", spotlight_.inFront);
                       spotlight_.visible = argBool(a, "visible", spotlight_.visible);
                       editSpotlight_ = argBool(a, "edit", editSpotlight_);
                       doc_.modified = true; syncReferenceView();
                       return CommandResult{true, "Reference settings updated"};
                   }});
    actions_.add({"reference.panel", "Spotlight reference...", nullptr,
                  "Reference image placement, image opacity and mesh opacity.", "Render", "", "",
                  [this] { showSpotlight_ = !showSpotlight_; }, [this] { return showSpotlight_; }, nullptr});
    actions_.add({"reference.add", "Add reference image...", nullptr,
                  "Place a PNG or JPEG reference on the canvas without generating a mesh.", "File", "", "",
                  [this] { chooseSpotlightImage(); }, nullptr, nullptr});
    actions_.add({"reference.visible", "Show reference image", nullptr,
                  "Show or hide the Spotlight reference without removing it.", "Render", "Shift+Z", "",
                  [this] { spotlight_.visible = !spotlight_.visible; doc_.modified = true; },
                  [this] { return spotlight_.visible && !spotlight_.image.empty(); }, nullptr});
}

void App::chooseSpotlightImage() {
    showSpotlight_ = true;
    static const SDL_DialogFileFilter filters[] = {{"Reference images", "png;jpg;jpeg;bmp;tga;psd;gif"}, {"All files", "*"}};
    SDL_ShowOpenFileDialog([](void* user, const char* const* files, int) {
        if (files && files[0]) static_cast<App*>(user)->queueFile(FileAction::SpotlightImage, files[0]);
    }, this, window_, filters, 2, nullptr, false);
}

bool App::loadSpotlight(const std::string& path) {
    std::string error;
    if (!spotlight_.load(path, error)) { status_ = error; return false; }
    uploadSpotlight();
    showSpotlight_ = true;
    editSpotlight_ = false;
    doc_.modified = true;
    status_ = "Reference image loaded. Use Edit placement to move it; turn it off to sculpt.";
    return spotlightTexture_ != 0;
}

void App::uploadSpotlight() {
    if (spotlightTexture_) gl::DeleteTextures(1, &spotlightTexture_);
    spotlightTexture_ = 0;
    if (spotlight_.image.empty()) { syncReferenceView(); return; }
    gl::GenTextures(1, &spotlightTexture_);
    gl::BindTexture(GL_TEXTURE_2D, spotlightTexture_);
    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GLint(GL_RGBA8), spotlight_.image.width, spotlight_.image.height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, spotlight_.image.rgba.data());
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint(GL_LINEAR));
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint(GL_LINEAR));
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint(GL_CLAMP_TO_EDGE));
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint(GL_CLAMP_TO_EDGE));
    gl::BindTexture(GL_TEXTURE_2D, 0);
    syncReferenceView();
}

void App::syncReferenceView() {
    const CanvasRect c = canvasRect();
    const glm::vec2 size = spotlight_.fittedSize(c.w, c.h);
    renderSettings_.referenceTexture = spotlight_.visible ? spotlightTexture_ : 0;
    renderSettings_.referenceCenter = glm::vec2(0.5f) + spotlight_.offset;
    renderSettings_.referenceSize = glm::vec2(size.x / std::max(c.w, 1.0f), size.y / std::max(c.h, 1.0f));
    renderSettings_.referenceOpacity = spotlight_.opacity;
    renderSettings_.referenceRotation = glm::radians(spotlight_.rotation);
    renderSettings_.referenceInFront = spotlight_.inFront;
    renderSettings_.referenceFlipX = spotlight_.flipX;
}

void App::drawSpotlight() {
    const CanvasRect c = canvasRect();
    ImGui::SetNextWindowPos(ImVec2(c.x + 14 * uiScale_, c.y + 24 * uiScale_), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(320 * uiScale_, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(260 * uiScale_, 180 * uiScale_),
                                       ImVec2(460 * uiScale_, std::max(260.0f * uiScale_, c.h)));
    if (ImGui::Begin("Spotlight Reference", &showSpotlight_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::Button("Add / Replace image...", ImVec2(-1, 0))) chooseSpotlightImage();
        ImGui::TextWrapped("Or drop an image onto the canvas. Images are saved inside your project.");
        bool changed = false;
        ImGui::SeparatorText("Reference image");
        ImGui::BeginDisabled(spotlight_.image.empty());
        changed |= ImGui::Checkbox("Show reference", &spotlight_.visible);
        ImGui::Checkbox("Edit placement", &editSpotlight_);
        if (editSpotlight_ && spotlight_.visible)
            ImGui::TextWrapped("Drag the canvas to move the image. Wheel: scale. Shift+drag: rotate. Turn Edit placement off to sculpt.");
        else ImGui::TextWrapped("Placement locked. You can sculpt and orbit through the reference.");
        changed |= ImGui::SliderFloat("Image opacity", &spotlight_.opacity, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::Checkbox("In front of mesh", &spotlight_.inFront);
        changed |= ImGui::SliderFloat("Image scale", &spotlight_.scale, 0.05f, 4.0f, "%.2fx");
        changed |= ImGui::SliderFloat("Image rotation", &spotlight_.rotation, -180.0f, 180.0f, "%.1f deg");
        changed |= ImGui::SliderFloat("Horizontal position", &spotlight_.offset.x, -1.5f, 1.5f, "%.2f");
        changed |= ImGui::SliderFloat("Vertical position", &spotlight_.offset.y, -1.5f, 1.5f, "%.2f");
        changed |= ImGui::Checkbox("Flip image horizontally", &spotlight_.flipX);
        if (ImGui::Button("Reset image placement")) { spotlight_.resetPlacement(); changed = true; }
        if (ImGui::Button("Remove reference")) {
            spotlight_ = ReferenceImage{}; editSpotlight_ = false; uploadSpotlight(); changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SeparatorText("Mesh");
        changed |= ImGui::SliderFloat("Mesh opacity", &renderSettings_.meshOpacity, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Restore solid mesh")) { renderSettings_.meshOpacity = 1.0f; changed = true; }
        ImGui::TextWrapped("Place the image behind the mesh and lower Mesh opacity to compare the silhouette. In front mode overlays the image using Image opacity.");
        if (changed) doc_.modified = true;
        syncReferenceView();
    }
    ImGui::End();
}

void App::drawReferenceInteraction() {
    if (!editSpotlight_ || !spotlight_.visible || spotlight_.image.empty()) return;
    const CanvasRect c = canvasRect();
    if (c.w <= 0 || c.h <= 0) return;
    ImGui::SetNextWindowPos(ImVec2(c.x, c.y));
    ImGui::SetNextWindowSize(ImVec2(c.w, c.h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##SpotlightPlacement", nullptr, flags)) {
        ImGui::InvisibleButton("Reference placement canvas", ImVec2(c.w, c.h));
        const ImGuiIO& io = ImGui::GetIO();
        bool changed = false;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (inputMods() & SDL_KMOD_SHIFT) spotlight_.rotation = std::clamp(spotlight_.rotation + io.MouseDelta.x * 0.5f, -180.0f, 180.0f);
            else spotlight_.offset = glm::clamp(spotlight_.offset + glm::vec2(io.MouseDelta.x / c.w, io.MouseDelta.y / c.h),
                                               glm::vec2(-2), glm::vec2(2));
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (io.MouseWheel != 0) {
                spotlight_.scale = std::clamp(spotlight_.scale * std::pow(1.1f, io.MouseWheel), 0.05f, 4.0f);
                changed = true;
            }
        }
        if (changed) { doc_.modified = true; syncReferenceView(); }
    }
    ImGui::End(); ImGui::PopStyleVar();
}

void App::drawViewCube() {
    if (!showViewCube_) return;
    const CanvasRect c = canvasRect();
    if (c.w < 180 * uiScale_ || c.h < 220 * uiScale_) return;
    const float s = uiScale_, width = 184 * s;
    ImGui::SetNextWindowPos(ImVec2(c.x + c.w - width - 10 * s, c.y + timelineTopInset_ + 10 * s));
    ImGui::SetNextWindowSize(ImVec2(width, 0));
    ImGui::SetNextWindowBgAlpha(0.87f);
    const auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("View Cube", nullptr, flags)) {
        ImGui::PushFont(fontSmall_);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4 * s, 3 * s));
        ImGui::Text("View: %s", camera_.nearestAxisView());
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x, h = 100 * s;
        const ImVec2 centre(start.x + w / 2, start.y + h / 2);
        ImGui::InvisibleButton("Orientation cube", ImVec2(w, h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const glm::quat inverse = glm::conjugate(camera_.orientation());
        struct Face { glm::vec3 n, u, v; const char* label; Camera::AxisView view; ImU32 color; };
        const Face faces[] = {
            {{0,0,1},{1,0,0},{0,1,0},"FRONT",Camera::AxisView::Front,IM_COL32(91,111,136,255)},
            {{0,0,-1},{-1,0,0},{0,1,0},"BACK",Camera::AxisView::Back,IM_COL32(70,85,108,255)},
            {{-1,0,0},{0,0,1},{0,1,0},"LEFT",Camera::AxisView::Left,IM_COL32(137,87,84,255)},
            {{1,0,0},{0,0,-1},{0,1,0},"RIGHT",Camera::AxisView::Right,IM_COL32(157,104,94,255)},
            {{0,1,0},{1,0,0},{0,0,-1},"TOP",Camera::AxisView::Top,IM_COL32(91,131,107,255)},
            {{0,-1,0},{1,0,0},{0,0,1},"BOTTOM",Camera::AxisView::Bottom,IM_COL32(71,103,85,255)}
        };
        for (const Face& f : faces) {
            const glm::vec3 normal = inverse * f.n;
            if (normal.z <= 0.001f) continue;
            ImVec2 p[4];
            const float signs[4][2] = {{-1,-1},{1,-1},{1,1},{-1,1}};
            for (int i = 0; i < 4; ++i) {
                const glm::vec3 q = inverse * (f.n + f.u * signs[i][0] + f.v * signs[i][1]);
                p[i] = ImVec2(centre.x + q.x * 28 * s, centre.y - q.y * 28 * s);
            }
            // Clockwise winding for ImDrawList's anti-aliased convex fill.
            std::swap(p[0], p[3]); std::swap(p[1], p[2]);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool over = ImGui::IsItemHovered() && (ImTriangleContainsPoint(p[0],p[1],p[2],mouse) ||
                                                        ImTriangleContainsPoint(p[0],p[2],p[3],mouse));
            dl->AddConvexPolyFilled(p, 4, over ? IM_COL32(192,136,76,255) : f.color);
            dl->AddPolyline(p, 4, IM_COL32(210,215,220,180), ImDrawFlags_Closed, 1.2f * s);
            const glm::vec3 n = inverse * f.n;
            if (normal.z > 0.38f) {
                const ImVec2 text = ImGui::CalcTextSize(f.label);
                dl->AddText(ImVec2(centre.x + n.x * 28*s - text.x/2, centre.y - n.y * 28*s - text.y/2),
                            IM_COL32(255,255,255,255), f.label);
            }
            if (over && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) camera_.setAxisView(f.view);
        }
        const char* labels[] = {"Front", "Back", "Top", "Left", "Right", "Bottom"};
        const Camera::AxisView views[] = {Camera::AxisView::Front, Camera::AxisView::Back, Camera::AxisView::Top,
                                         Camera::AxisView::Left, Camera::AxisView::Right, Camera::AxisView::Bottom};
        const float buttonW = (w - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
        for (int i = 0; i < 6; ++i) {
            if (i % 3) ImGui::SameLine();
            if (ImGui::Button(labels[i], ImVec2(buttonW, 0))) camera_.setAxisView(views[i]);
        }
        if (ImGui::Button("Reference...", ImVec2(w, 0))) showSpotlight_ = !showSpotlight_;
        ImGui::PopStyleVar();
        ImGui::PopFont();
    }
    ImGui::End();
}
} // namespace sf
