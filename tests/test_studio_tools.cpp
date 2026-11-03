// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"
#include "core/Commands.h"
#include "core/Polygroups.h"
#include "core/Primitives.h"
#include "core/StudioTools.h"
#include "core/Timeline.h"
#include <map>
using namespace sf;
namespace {
bool closed(const Mesh& m) {
    std::map<std::pair<uint32_t, uint32_t>, int> e;
    for (auto t : m.triangles)
        for (int k = 0; k < 3; ++k)
            ++e[{t[k], t[(k + 1) % 3]}];
    for (auto [key, n] : e)
        if (n != 1 || e[{key.second, key.first}] != 1) return false;
    return !e.empty();
}
} // namespace
TEST(studio_morph_target_switch_layers_undo_and_project) {
    auto d = makeDoc(makeSculptQuadSphere(2));
    CommandRegistry c;
    d.mesh.addLayer("Detail");
    d.mesh.layers[0].delta[3] = {.01f, .02f, 0};
    d.mesh.evaluateAll();
    auto original = d.mesh.positions;
    CHECK(c.execute(d, "morph_store").ok);
    CHECK(hasMorphTarget(d.mesh));
    CHECK(c.execute(d, "transform", {{"translate", std::string("0.2,0.1,0")}}).ok);
    auto moved = d.mesh.positions;
    CHECK(c.execute(d, "morph_switch").ok);
    CHECK(d.mesh.positions == original);
    CHECK(d.mesh.morphTarget == moved);
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.mesh.positions == moved);
    CHECK(d.mesh.morphTarget == original);
    auto path = tempPath("sf-studio-morph.sfproj");
    CHECK(c.execute(d, "save_project", {{"path", path}}).ok);
    auto other = makeDoc(makePlane(2));
    CHECK(c.execute(other, "open_project", {{"path", path}}).ok);
    CHECK(hasMorphTarget(other.mesh));
    CHECK(other.mesh.morphTarget == original);
    CHECK(other.mesh.positions == moved);
    CHECK(c.execute(other, "morph_switch").ok);
    CHECK(other.mesh.positions == original);
    CHECK(c.execute(other, "morph_delete").ok);
    CHECK(!hasMorphTarget(other.mesh));
    CHECK(c.execute(other, "undo").ok);
    CHECK(hasMorphTarget(other.mesh));
    std::swap(other.mesh.triangles[0].x, other.mesh.triangles[0].y);
    CHECK(!hasMorphTarget(other.mesh));
    CHECK(!c.execute(other, "morph_switch").ok);
}
TEST(studio_close_holes_preserves_surface_and_is_undoable) {
    Mesh cube = makeCube(4);
    std::vector<uint8_t> keep(cube.triangleCount(), 1);
    for (size_t t = 0; t < keep.size(); ++t) {
        auto tri = cube.triangles[t];
        if (cube.positions[tri.x].y > .99f && cube.positions[tri.y].y > .99f && cube.positions[tri.z].y > .99f)
            keep[t] = 0;
    }
    auto open = extractTriangles(cube, keep);
    CHECK(!closed(open));
    auto d = makeDoc(open);
    CommandRegistry c;
    CHECK(c.execute(d, "close_holes").ok);
    CHECK(closed(d.mesh));
    CHECK(d.mesh.vertexCount() == open.vertexCount());
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.mesh.positions == open.positions);
    CHECK(d.mesh.triangles == open.triangles);
    CHECK(c.execute(d, "redo").ok);
    CHECK(closed(d.mesh));
}
TEST(studio_group_loops_and_panel_shells) {
    auto cube = makeCube(4);
    std::string error;
    Mesh rings = groupLoops(cube, .08f, 3, error);
    CHECK(error.empty());
    CHECK(rings.triangleCount() > cube.triangleCount());
    CHECK(closed(rings));
    CHECK(rings.quadCount() > cube.quadCount());
    error.clear();
    Mesh panels = panelLoops(cube, .02f, .05f, 2, error);
    CHECK(error.empty());
    CHECK(panels.vertexCount() > cube.vertexCount());
    CHECK(closed(panels));
    auto d = makeDoc(cube);
    CommandRegistry c;
    CHECK(c.execute(d, "groups_loops", {{"loops", 2.0}}).ok);
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.mesh.positions == cube.positions);
    CHECK(d.mesh.triangles == cube.triangles);
}
TEST(studio_split_masked_single_undo_restores_both_parts) {
    auto d = makeDoc(makeCube(6));
    CommandRegistry c;
    auto original = d.mesh.positions;
    auto faces = d.mesh.triangles;
    auto& mask = d.mesh.ensureMask();
    for (size_t v = 0; v < mask.size(); ++v)
        mask[v] = d.mesh.positions[v].x < 0 ? 1 : 0;
    CHECK(c.execute(d, "subtool_split_masked").ok);
    CHECK(d.subtoolCount() == 2);
    CHECK(d.mesh.triangleCount() < faces.size());
    CHECK(d.undo.undoCount() == 1);
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.subtoolCount() == 1);
    CHECK(d.mesh.positions == original);
    CHECK(d.mesh.triangles == faces);
    CHECK(c.execute(d, "redo").ok);
    CHECK(d.subtoolCount() == 2);
}
TEST(studio_flip_uvs_and_weld_undo) {
    auto d = makeDoc(makePlane(3));
    CommandRegistry c;
    auto original = d.mesh.triangles;
    auto n = d.mesh.normals;
    CHECK(c.execute(d, "mesh_flip").ok);
    for (size_t v = 0; v < n.size(); ++v)
        CHECK(glm::dot(d.mesh.normals[v], n[v]) < -.99f);
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.mesh.triangles == original);
    CHECK(c.execute(d, "weld_points").ok);
    CHECK(c.execute(d, "undo").ok);
    CHECK(d.mesh.triangles == original);
}
TEST(studio_negative_adjust_last_reverses_displacement) {
    auto d = makeDoc(makeSculptQuadSphere(3));
    auto t = Timeline::attach(d);
    auto original = d.mesh.positions;
    DabParams p;
    p.center = {0, 0, 1};
    p.radius = .4f;
    p.strength = .6f;
    p.viewDir = {0, 0, -1};
    CHECK(d.sculptor.beginStroke(d.mesh, "Standard"));
    d.sculptor.dab(d.mesh, d.bvh, BrushType::Standard, p, {false, false, false});
    d.undo.push(d.sculptor.endStroke());
    t->sync(d);
    auto positive = d.mesh.positions;
    CHECK(t->retune(d, t->cursorNode(), -1));
    for (size_t v = 0; v < original.size(); ++v)
        CHECK(glm::length((d.mesh.positions[v] - original[v]) + (positive[v] - original[v])) < 1e-6f);
}
