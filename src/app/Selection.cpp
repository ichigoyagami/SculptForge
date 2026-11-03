// SculptForge - open source digital sculpting
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Screen selections: the rectangle and lasso drags shared by visibility
// (Ctrl+Shift, see PanelPolygroups.cpp) and masking (Ctrl with MaskLasso or
// MaskRect, see PanelMasking.cpp). This file tracks and draws the shape; the
// panels decide what a click or a finished drag does. The selection itself
// runs as hide_rect / hide_lasso / mask_rect / mask_lasso commands with the
// shape in normalised device coordinates and the current view.
#include "app/App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace sf {

namespace {

constexpr float kClickSlopPx = 4.0f;    // movement that turns a click into a drag
constexpr float kLassoSpacingPx = 3.0f; // lasso points closer than this to the last one are skipped

std::string matrixString(const glm::mat4& m) {
    std::string s;
    char buf[32];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(buf, sizeof buf, "%s%.9g", i ? " " : "", double(m[i / 4][i % 4]));
        s += buf;
    }
    return s;
}

// Fills a polygon with the even-odd rule, the inside test the selection uses, one pixel row at a
// time (ImGui's concave fill expects a simple polygon; a hand-drawn lasso often crosses itself).
void fillEvenOdd(ImDrawList* dl, const std::vector<ImVec2>& poly, ImU32 color) {
    if (poly.size() < 3) return;
    float top = poly[0].y, bottom = poly[0].y;
    for (const ImVec2& p : poly) {
        top = std::fmin(top, p.y);
        bottom = std::fmax(bottom, p.y);
    }
    std::vector<float> xs;
    for (float y = std::floor(top) + 0.5f; y < bottom; y += 1.0f) {
        xs.clear();
        for (size_t i = 0; i < poly.size(); ++i) {
            const ImVec2 a = poly[i], b = poly[(i + 1) % poly.size()];
            if ((a.y > y) != (b.y > y)) xs.push_back(a.x + (y - a.y) / (b.y - a.y) * (b.x - a.x));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2)
            dl->AddRectFilled(ImVec2(xs[i], y - 0.5f), ImVec2(xs[i + 1], y + 0.5f), color);
    }
}

} // namespace

void App::beginSelection(float x, float y, SelectTarget target, SelectShape shape) {
    selectTarget_ = target;
    selectShape_ = shape;
    selectPath_.assign(1, ImVec2(x, y));
    selectMoved_=false;selectEnd_=selectPointer_={x,y};selectMoving_=false;selectSharp_={1};selectAltAt_=0;
    drag_ = Drag::Select;
}

void App::selectionMove(float x, float y) {
    const ImVec2 delta(x-selectPointer_.x,y-selectPointer_.y);selectPointer_={x,y};
    if(selectMoving_){for(auto& p:selectPath_){p.x+=delta.x;p.y+=delta.y;}selectEnd_.x+=delta.x;selectEnd_.y+=delta.y;return;}
    selectEnd_={x,y};
    const ImVec2 start = selectPath_.front();
    if (!selectMoved_ && std::abs(x - start.x) < kClickSlopPx && std::abs(y - start.y) < kClickSlopPx) return;
    selectMoved_ = true;
    if (selectShape_ != SelectShape::Lasso) return; // a rectangle only needs the cursor
    const ImVec2 last = selectPath_.back();
    if (std::hypot(x - last.x, y - last.y) >= kLassoSpacingPx) selectPath_.push_back(ImVec2(x, y));
}

void App::selectionMouseUp() {
    if(selectTarget_==SelectTarget::Knife || selectTarget_==SelectTarget::Clip || selectTarget_==SelectTarget::Slice) planeSelectMouseUp();
    else if(selectTarget_==SelectTarget::TopologyMask || selectTarget_==SelectTarget::TransposeLine) topologySelectMouseUp();
    else if (selectTarget_ == SelectTarget::Hide)
        polygroupMouseUp();
    else
        maskSelectMouseUp();
    selectPath_.clear();selectMoving_=false;
}

bool App::runSelection(const char* rectCommand, const char* lassoCommand, Args args) {
    const CanvasRect c = canvasRect();
    if (c.w <= 0.0f || c.h <= 0.0f || selectPath_.empty()) return false;
    // Window pixels -> normalised device coordinates (y up).
    auto ndc = [&](ImVec2 p) { return glm::vec2((p.x - c.x) / c.w * 2.0f - 1.0f, 1.0f - (p.y - c.y) / c.h * 2.0f); };
    args["view_proj"] = matrixString(camera_.projection() * camera_.view());
    if (selectTarget_ == SelectTarget::Mask) {
        std::string axes;
        if (symmetry_.x) axes += 'x';
        if (symmetry_.y) axes += 'y';
        if (symmetry_.z) axes += 'z';
        args["symmetry"] = axes;
    }
    if (selectShape_ == SelectShape::Lasso) {
        std::vector<ImVec2> path = selectPath_;
        path.push_back(ImVec2(mouseX_, mouseY_));
        if (path.size() < 3) return false;
        std::string points;
        char buf[48];
        for (const ImVec2& p : path) {
            const glm::vec2 q = ndc(p);
            std::snprintf(buf, sizeof buf, "%s%.5f %.5f", points.empty() ? "" : " ", double(q.x), double(q.y));
            points += buf;
        }
        args["points"] = points;
        runCommand(lassoCommand, args);
        return true;
    }
    const glm::vec2 a = ndc(selectPath_.front()), b = ndc(ImVec2(mouseX_, mouseY_));
    args["x0"] = double(a.x);
    args["y0"] = double(a.y);
    args["x1"] = double(b.x);
    args["y1"] = double(b.y);
    runCommand(rectCommand, args);
    return true;
}

void App::drawSelectionOverlay() {
    if (drag_ != Drag::Select || !selectMoved_ || selectPath_.empty()) return;
    // Visibility: green keeps the inside, red hides it (Alt). Mask: a dark veil masks, blue unmasks (Alt).
    const bool alt = (inputMods() & SDL_KMOD_ALT) != 0;
    ImU32 line, fill;
    if (selectTarget_ == SelectTarget::Hide) {
        line = alt ? IM_COL32(255, 110, 110, 230) : IM_COL32(120, 230, 140, 230);
        fill = alt ? IM_COL32(255, 110, 110, 40) : IM_COL32(120, 230, 140, 30);
    } else {
        line = alt ? IM_COL32(120, 200, 255, 230) : IM_COL32(230, 230, 230, 230);
        fill = alt ? IM_COL32(120, 200, 255, 35) : IM_COL32(0, 0, 0, 70);
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const CanvasRect c = canvasRect();
    dl->PushClipRect(ImVec2(c.x, c.y), ImVec2(c.x + c.w, c.y + c.h), true);
    const ImVec2 start = selectPath_.front(), mouse(mouseX_, mouseY_);
    if(selectTarget_==SelectTarget::Knife || selectTarget_==SelectTarget::Clip || selectTarget_==SelectTarget::Slice){
        auto profile=selectionProfile();bool closed=selectShape_==SelectShape::ClipRect || selectShape_==SelectShape::ClipCircle || selectShape_==SelectShape::SliceRect || selectShape_==SelectShape::SliceCircle;
        if(closed){fillEvenOdd(dl,profile,IM_COL32(255,120,80,35));dl->AddPolyline(profile.data(),int(profile.size()),line,ImDrawFlags_Closed,2);}
        else if(profile.size()>1){dl->AddPolyline(profile.data(),int(profile.size()),line,0,2);
            for(size_t i=0;i+1<profile.size();++i){glm::vec2 d(profile[i+1].x-profile[i].x,profile[i+1].y-profile[i].y);float len=glm::length(d);if(len<1e-3f)continue;d/=len;
                auto n=glm::vec2(-d.y,d.x)*(alt?-1.f:1.f)*12.f;ImVec2 q[]={profile[i],profile[i+1],{profile[i+1].x+n.x,profile[i+1].y+n.y},{profile[i].x+n.x,profile[i].y+n.y}};dl->AddConvexPolyFilled(q,4,IM_COL32(255,100,80,65));}}
        for(size_t i=1;i<selectPath_.size();++i)dl->AddCircleFilled(selectPath_[i],3,selectSharp_[i]?IM_COL32(255,200,80,255):IM_COL32(120,200,255,255));
    } else if(selectTarget_==SelectTarget::TopologyMask || selectTarget_==SelectTarget::TransposeLine){
        dl->AddLine(start,mouse,IM_COL32(255,210,70,255),2);dl->AddCircle(start,6,line);dl->AddCircle(mouse,6,line);
    } else if (selectShape_ == SelectShape::Lasso) {
        std::vector<ImVec2> path = selectPath_;
        path.push_back(mouse);
        fillEvenOdd(dl, path, fill);
        dl->AddPolyline(path.data(), int(path.size()), line, ImDrawFlags_Closed, 1.5f);
    } else {
        const ImVec2 p0(std::fmin(start.x, mouse.x), std::fmin(start.y, mouse.y));
        const ImVec2 p1(std::fmax(start.x, mouse.x), std::fmax(start.y, mouse.y));
        dl->AddRectFilled(p0, p1, fill);
        dl->AddRect(p0, p1, line, 0.0f, 0, 1.5f);
    }
    dl->PopClipRect();
}

bool App::selectionKey(SDL_Keycode key){
    if(key==SDLK_ESCAPE){selectPath_.clear();selectMoving_=false;drag_=Drag::None;status_="Selection cancelled";return true;}
    if(key==SDLK_SPACE){selectMoving_=true;return true;}
    bool curve=selectShape_==SelectShape::KnifeCurve || selectShape_==SelectShape::ClipCurve || selectShape_==SelectShape::SliceCurve;
    if(curve && (key==SDLK_LALT || key==SDLK_RALT)){
        auto now=SDL_GetTicks();if(selectAltAt_ && now-selectAltAt_<350 && selectPath_.size()>1 && std::hypot(selectEnd_.x-selectPath_.back().x,selectEnd_.y-selectPath_.back().y)<6)selectSharp_.back()=1;
        else if(std::hypot(selectEnd_.x-selectPath_.back().x,selectEnd_.y-selectPath_.back().y)>3){selectPath_.push_back(selectEnd_);selectSharp_.push_back(0);}
        selectAltAt_=now;return true;
    }return false;
}
std::vector<ImVec2> App::selectionProfile() const {
    if(selectPath_.empty())return {};auto start=selectPath_.front(),end=selectEnd_;
    bool rect=selectShape_==SelectShape::ClipRect || selectShape_==SelectShape::SliceRect,circle=selectShape_==SelectShape::ClipCircle || selectShape_==SelectShape::SliceCircle;
    if(rect || circle){float dx=end.x-start.x,dy=end.y-start.y;if(profileSquare_){float d=std::max(std::abs(dx),std::abs(dy));dx=std::copysign(d,dx);dy=std::copysign(d,dy);}
        ImVec2 a=profileCenter_?ImVec2(start.x-dx,start.y-dy):start,b(start.x+dx,start.y+dy);if(rect)return {a,{b.x,a.y},b,{a.x,b.y}};
        std::vector<ImVec2> p;for(int i=0;i<48;++i){float angle=float(i)*6.283185307f/48;p.push_back({(a.x+b.x)*.5f+std::abs(b.x-a.x)*.5f*std::cos(angle),(a.y+b.y)*.5f+std::abs(b.y-a.y)*.5f*std::sin(angle)});}return p;}
    auto controls=selectPath_;controls.push_back(end);std::vector<ImVec2> out{controls.front()};
    for(size_t i=1;i+1<controls.size();++i){if(i<selectSharp_.size() && selectSharp_[i]){out.push_back(controls[i]);continue;}
        auto a=controls[i-1],b=controls[i],c=controls[i+1];ImVec2 from((a.x+b.x)*.5f,(a.y+b.y)*.5f),to((b.x+c.x)*.5f,(b.y+c.y)*.5f);out.push_back(from);
        for(int j=1;j<=6;++j){float t=float(j)/6,u=1-t;out.push_back({u*u*from.x+2*u*t*b.x+t*t*to.x,u*u*from.y+2*u*t*b.y+t*t*to.y});}}
    out.push_back(end);return out;
}
void App::planeSelectMouseUp(){
    if(!selectMoved_ || selectPath_.empty())return;
    bool line=selectShape_==SelectShape::KnifeLine || selectShape_==SelectShape::ClipLine || selectShape_==SelectShape::SliceLine;auto profile=selectionProfile();auto c=canvasRect();
    auto ndc=[&](ImVec2 p){return glm::vec2((p.x-c.x)/c.w*2-1,1-(p.y-c.y)/c.h*2);};
    std::string axes;if(symmetry_.x)axes+='x';if(symmetry_.y)axes+='y';if(symmetry_.z)axes+='z';bool alt=(inputMods() & SDL_KMOD_ALT)!=0;
    if(line){auto a=ndc(profile.front()),b=ndc(profile.back()),d=b-a;auto plane=glm::transpose(camera_.projection()*camera_.view())*glm::vec4(d.y,-d.x,0,d.x*a.y-d.y*a.x);
        float len=glm::length(glm::vec3(plane));if(len<1e-8f)return;plane/=len;if(alt)plane=-plane;
        auto normal=std::to_string(plane.x)+","+std::to_string(plane.y)+","+std::to_string(plane.z);
        runCommand(selectTarget_==SelectTarget::Knife?"knife_plane":selectTarget_==SelectTarget::Slice?"slice_plane":"clip_plane",{{"normal",normal},{"offset",double(-plane.w)},{"symmetry",axes}});
    }else{std::string points;char buf[64];for(auto p:profile){auto q=ndc(p);std::snprintf(buf,sizeof buf,"%.9g %.9g ",double(q.x),double(q.y));points+=buf;}
        bool closed=selectShape_==SelectShape::ClipRect || selectShape_==SelectShape::ClipCircle || selectShape_==SelectShape::SliceRect || selectShape_==SelectShape::SliceCircle;
        runCommand(selectTarget_==SelectTarget::Knife?"knife_profile":selectTarget_==SelectTarget::Slice?"slice_profile":"clip_profile",{{"points",points},{"view_proj",matrixString(camera_.projection()*camera_.view())},{"closed",closed},{"invert",alt},{"symmetry",axes}});
    }if(lastCommandOk_ && selectTarget_==SelectTarget::Slice)renderSettings_.showPolygroups=true;
}

void App::placeTransposeLine(const glm::vec3& a,const glm::vec3& b){
    if(glm::length(b-a)<1e-7f)return;
    glm::vec3 x=glm::normalize(b-a),z=-camera_.forward();
    glm::vec3 y=glm::cross(z,x);if(glm::length(y)<1e-6f)return;y=glm::normalize(y);z=glm::normalize(glm::cross(x,y));
    gizmo_.pivot=a;gizmo_.pivotPlaced=true;gizmo_.orientSurface=true;gizmo_.frame=glm::mat3(x,y,z);gizmo_.lineLength=glm::length(b-a);
}

void App::topologySelectMouseUp(){
    doc_.syncBvh();
    if(!selectMoved_){if(selectTarget_==SelectTarget::TopologyMask)runCommand("mask_except_polygroup",{{"group",double(doc_.mesh.polygroupAt(selectOrigin_.triangle))}});return;}
    RayHit tip;if(!pick(mouseX_,mouseY_,tip)){status_="End the topology/Transpose line on the model surface";return;}
    auto closest=[&](const RayHit& h){auto t=doc_.mesh.triangles[h.triangle];uint32_t best=t.x;
        for(int i=1;i<3;++i)if(glm::length(doc_.mesh.positions[t[i]]-h.position)<glm::length(doc_.mesh.positions[best]-h.position))best=t[i];return best;};
    std::string axes;
    if(symmetry_.x)axes+='x';if(symmetry_.y)axes+='y';if(symmetry_.z)axes+='z';
    if(selectTarget_==SelectTarget::TopologyMask)runCommand("mask_topology",withSymmetryArgs({{"origin",double(closest(selectOrigin_))},{"tip",double(closest(tip))},
        {"add",bool(inputMods() & SDL_KMOD_SHIFT)},{"unmask",bool(inputMods() & SDL_KMOD_ALT)},{"symmetry",axes}}));
    placeTransposeLine(selectOrigin_.position,tip.position);
}

} // namespace sf
