// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"
#include "core/Commands.h"
#include "core/Document.h"
#include "core/GizmoDeform.h"
#include "core/MeshTransform.h"
#include "core/Primitives.h"
#include <algorithm>
#include <cmath>
using namespace sf;
TEST(gizmo_deform_identity_and_repeat_preview) {
    const Mesh source = makeCube(5, 2);
    std::string err;
    Mesh out;
    GizmoDeformOptions o;
    for (int op = 0; op < 16; ++op) {
        CHECK(gizmoDeform(source, GizmoDeformOp(op), o, out, err));
        for (size_t i = 0; i < source.vertexCount(); ++i)
            CHECK(glm::distance(out.positions[i], source.positions[i]) < 1e-5f);
        CHECK(out.triangles == source.triangles);
        CHECK(out.quadPartners == source.quadPartners);
    }
    o.amount = 67;
    Mesh a, b;
    CHECK(gizmoDeform(source, GizmoDeformOp::Twist, o, a, err));
    CHECK(gizmoDeform(source, GizmoDeformOp::Twist, o, b, err));
    CHECK(a.positions == b.positions);
    CHECK(a.positions != source.positions);
}
TEST(gizmo_deform_mask_hidden_layers_attributes) {
    Mesh source = makeCube(5, 2);
    source.mask.assign(source.vertexCount(), .5f);
    source.mask[0] = 1;
    source.base = source.positions;
    std::string err;
    Mesh out;
    GizmoDeformOptions o;
    o.amount = 50;
    CHECK(gizmoDeform(source, GizmoDeformOp::Offset, o, out, err));
    CHECK(out.positions[0] == source.positions[0]);
    for (size_t i = 1; i < source.vertexCount(); ++i) {
        CHECK(std::abs(out.positions[i].y - source.positions[i].y - .5f) < 1e-5f);
        CHECK(glm::distance(out.base[i], out.positions[i]) < 1e-5f);
    }
    source.hidden.assign(source.triangleCount(), 1);
    source.rebuildTopology();
    CHECK(!gizmoDeform(source, GizmoDeformOp::Offset, o, out, err));
}
TEST(gizmo_deform_axes_anchor_cage_and_curve) {
    Mesh source = makeCube(4, 2);
    std::string err;
    Mesh out;
    GizmoDeformOptions o;
    o.axis = 0;
    o.amount = 100;
    CHECK(gizmoDeform(source, GizmoDeformOp::Stretch, o, out, err));
    for (size_t i = 0; i < source.vertexCount(); ++i) {
        CHECK(std::abs(out.positions[i].x - (2 * source.positions[i].x + 1)) < 1e-5f);
        CHECK(out.positions[i].y == source.positions[i].y);
    }
    o = {};
    o.cage[7] = glm::vec3(.5f, .4f, .3f);
    CHECK(gizmoDeform(source, GizmoDeformOp::Cage, o, out, err));
    for (size_t i = 0; i < source.vertexCount(); ++i)
        if (source.positions[i] == glm::vec3(1))
            CHECK(glm::distance(out.positions[i], glm::vec3(1.5f, 1.4f, 1.3f)) < 1e-5f);
    o = {};
    o.curve[3] = glm::vec3(1, 0, 0);
    CHECK(gizmoDeform(source, GizmoDeformOp::BendCurve, o, out, err));
    for (size_t i = 0; i < source.vertexCount(); ++i) {
        if (source.positions[i].y == -1) CHECK(out.positions[i] == source.positions[i]);
        if (source.positions[i].y == 1) CHECK(std::abs(out.positions[i].x - source.positions[i].x - 1) < 1e-5f);
    }
}
TEST(gizmo_deform_undo_and_slice) {
    Document d;
    d.setMesh(makeCube(4, 2));
    CommandRegistry r;
    const auto original = d.mesh.positions;
    auto result = r.execute(d, "gizmo_deform", {{"type", "Twist"}, {"amount", 45.0}});
    CHECK(result.ok);
    CHECK(d.mesh.positions != original);
    CHECK(d.undo.undoCount() == 1);
    CHECK(r.execute(d, "undo", {}).ok);
    CHECK(d.mesh.positions == original);
    CHECK(r.execute(d, "redo", {}).ok);
    CHECK(d.mesh.positions != original);
    Mesh source = makeCube(4, 2), out;
    GizmoDeformOptions o;
    o.amount = 13;
    std::string error;
    CHECK(gizmoDeform(source, GizmoDeformOp::MultiSlice, o, out, error));
    CHECK(out.triangleCount() > source.triangleCount());
    for (const auto& p : source.positions)
        CHECK(std::find(out.positions.begin(), out.positions.end(), p) != out.positions.end());
    o.axis = 5;
    CHECK(!gizmoDeform(source, GizmoDeformOp::Twist, o, out, error));
}

TEST(gizmo_deform_slice_on_existing_edges_keeps_groups) {
    Document d;
    d.setMesh(makeCube(4, 2));
    CommandRegistry registry;
    const auto groups = d.mesh.polygroups;
    CHECK(registry.execute(d, "gizmo_deform", {{"type", "Slice"}, {"amount", 0.0}}).ok);
    CHECK(d.mesh.polygroups != groups);
    CHECK(d.undo.undoCount() == 1);
    CHECK(registry.execute(d, "undo", {}).ok);
    CHECK(d.mesh.polygroups == groups);
}
