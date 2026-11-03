// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"
#include "core/ReferenceImage.h"
#include "core/Project.h"
#include "render/Camera.h"
#include <cmath>

using namespace sf;

TEST(reference_embedded_image_and_settings_roundtrip) {
    Image image; image.width = 3; image.height = 2;
    image.rgba = {255,0,0,255, 0,255,0,128, 0,0,255,255, 12,34,56,255, 72,83,94,255, 200,190,180,0};
    ReferenceImage a; std::string error;
    CHECK(a.setImage(image, "a-file-that-will-not-exist.png", error));
    a.opacity = 0.3f; a.scale = 1.7f; a.rotation = -37; a.offset = {0.2f, -0.1f};
    a.inFront = true; a.flipX = true; a.visible = false;
    Json parsed; CHECK(Json::parse(a.toJson().dump(), parsed, error));
    ReferenceImage b; CHECK(b.fromJson(parsed, error));
    CHECK(b.image.rgba == image.rgba);
    CHECK(b.image.width == 3 && b.image.height == 2);
    CHECK(b.opacity == a.opacity && b.scale == a.scale && b.rotation == a.rotation);
    CHECK(b.offset == a.offset && b.inFront && b.flipX && !b.visible);
    CHECK(b.source == a.source);
    const auto size = b.fittedSize(600, 600);
    CHECK(std::abs(size.x / size.y - 1.5f) < 0.0001f);
}

TEST(reference_invalid_embedded_data_is_rejected_without_replacing_image) {
    ReferenceImage a; Image image; image.width=1; image.height=1; image.rgba={1,2,3,255}; std::string error;
    CHECK(a.setImage(image, "original", error));
    Json j=a.toJson(); j.set("png_hex", "xyz");
    CHECK(!a.fromJson(j,error)); CHECK(a.source=="original"); CHECK(a.image.rgba==image.rgba);
    j.set("png_hex", "gg"); CHECK(!a.fromJson(j,error));
    CHECK(a.fromJson(Json::object(),error)); CHECK(a.image.empty());
}

TEST(reference_settings_clamp_and_reset) {
    ReferenceImage a; Image image; image.width=1; image.height=1; image.rgba={255,255,255,255}; std::string error;
    CHECK(a.setImage(image,"source",error));
    Json j=a.toJson(); j.set("opacity", 6); j.set("scale", -2); j.set("offset_x", 100); j.set("rotation", 1000);
    CHECK(a.fromJson(j,error)); CHECK(a.opacity==1); CHECK(a.scale==0.05f); CHECK(a.offset.x==2); CHECK(a.rotation==180);
    a.flipX=true; a.resetPlacement(); CHECK(a.scale==1 && a.rotation==0 && a.offset==glm::vec2(0) && !a.flipX);
    CHECK(a.fittedSize(0,0)==glm::vec2(0));
}

TEST(camera_six_named_views_preserve_frame_and_clear_roll) {
    Camera c; c.target={2,3,4}; c.distance=7; c.roll=1;
    const Camera::AxisView views[] = {Camera::AxisView::Front,Camera::AxisView::Back,Camera::AxisView::Left,
                                     Camera::AxisView::Right,Camera::AxisView::Top,Camera::AxisView::Bottom};
    const glm::vec3 back[]={{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    const char* labels[]={"Front","Back","Left","Right","Top","Bottom"};
    for(int i=0;i<6;++i) {
        c.setAxisView(views[i]);
        CHECK(glm::length((c.position()-c.target)/c.distance-back[i]) < 0.00001f);
        CHECK(std::string(c.nearestAxisView())==labels[i]);
        CHECK(c.target==glm::vec3(2,3,4) && c.distance==7 && c.roll==0);
        glm::vec3 origin,dir; c.screenRay(300,300,600,600,origin,dir);
        CHECK(glm::dot(dir,c.forward())>0.9999f);
    }
}
