// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/StudioTools.h"
#include "../../third_party/earcut/earcut.hpp"
#include "core/Commands.h"
#include "core/Document.h"
#include "core/ExtractMirror.h"
#include "core/Micro.h"
#include "core/Multires.h"
#include "core/Polygroups.h"
#include "core/SubTools.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

namespace sf {
namespace {
uint64_t edgeKey(uint32_t a, uint32_t b) { return (uint64_t(std::min(a, b)) << 32) | std::max(a, b); }
struct Edge {
    uint32_t a = 0, b = 0;
    int count = 0;
};
std::map<uint64_t, Edge> edgeMap(const Mesh& m) {
    std::map<uint64_t, Edge> out;
    for (auto t : m.triangles)
        for (int k = 0; k < 3; ++k) {
            auto a = t[k], b = t[(k + 1) % 3];
            auto& e = out[edgeKey(a, b)];
            e.a = a;
            e.b = b;
            ++e.count;
        }
    return out;
}
void finish(Mesh& m) {
    m.morphTarget.clear();
    m.morphTopologyKey = 0;
    m.multires.reset();
    m.micro.reset();
    m.rebuildTopology();
    m.recomputeAllNormals();
}
void addQuad(Mesh& m, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t group) {
    uint32_t t = uint32_t(m.triangleCount());
    m.triangles.push_back({a, b, c});
    m.triangles.push_back({a, c, d});
    m.polygroups.push_back(group);
    m.polygroups.push_back(group);
    m.pairQuad(t, t + 1);
}
} // namespace
uint64_t meshTopologyKey(const Mesh& m) {
    uint64_t key = 1469598103934665603ull;
    auto add = [&](uint32_t n) {
        for (int k = 0; k < 4; ++k) {
            key ^= uint8_t(n >> (k * 8));
            key *= 1099511628211ull;
        }
    };
    add(uint32_t(m.vertexCount()));
    for (auto t : m.triangles) {
        add(t.x);
        add(t.y);
        add(t.z);
    }
    return key;
}
bool hasMorphTarget(const Mesh& m) {
    return !m.morphTarget.empty() && m.morphTarget.size() == m.vertexCount() &&
           m.morphTopologyKey == meshTopologyKey(m);
}
Mesh closeMeshHoles(const Mesh& source, std::string& error) {
    Mesh out = source;
    auto edges = edgeMap(out);
    std::map<uint32_t, uint32_t> next;
    std::set<uint32_t> incoming;
    for (auto [key, e] : edges) {
        if (e.count > 2) {
            error = "Close Holes requires manifold edges";
            return {};
        }
        if (e.count != 1) continue;
        // Reverse the existing boundary so the cap faces outward.
        if (next.count(e.b) || !incoming.insert(e.a).second) {
            error = "Branching boundary: repair the mesh before Close Holes";
            return {};
        }
        next[e.b] = e.a;
    }
    if (next.empty()) return source;
    const uint32_t group = nextPolygroupId(out);
    out.ensurePolygroups();
    while (!next.empty()) {
        uint32_t start = next.begin()->first, at = start;
        std::vector<uint32_t> ids;
        do {
            ids.push_back(at);
            auto it = next.find(at);
            if (it == next.end()) {
                error = "Open boundary chain";
                return {};
            }
            at = it->second;
            next.erase(it);
        } while (at != start);
        if (ids.size() < 3) {
            error = "Invalid hole boundary";
            return {};
        }
        glm::vec3 n(0);
        for (size_t i = 0; i < ids.size(); ++i)
            n += glm::cross(out.positions[ids[i]], out.positions[ids[(i + 1) % ids.size()]]);
        if (glm::length(n) < 1e-9f) {
            error = "Cannot close a degenerate hole";
            return {};
        }
        n = glm::normalize(n);
        glm::vec3 u = glm::normalize(glm::cross(std::abs(n.x) < .8f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0), n)),
                  v = glm::cross(n, u);
        std::vector<std::vector<std::array<double, 2>>> polygon(1);
        for (auto id : ids) {
            auto p = out.positions[id];
            polygon[0].push_back({glm::dot(p, u), glm::dot(p, v)});
        }
        auto caps = mapbox::earcut<uint32_t>(polygon);
        if (caps.empty()) {
            error = "Could not triangulate the hole";
            return {};
        }
        // Restore any collinear boundary nodes discarded by the triangulator.
        std::vector<glm::uvec3> triangles;
        std::set<uint32_t> used;
        for (size_t i = 0; i < caps.size(); i += 3) {
            glm::uvec3 t(ids[caps[i]], ids[caps[i + 1]], ids[caps[i + 2]]);
            triangles.push_back(t);
            used.insert(t.x);
            used.insert(t.y);
            used.insert(t.z);
        }
        float eps = std::max(glm::length(out.bounds().extent()) * 1e-6f, 1e-8f);
        for (auto q : ids)
            if (!used.count(q)) {
                bool found = false;
                std::map<uint64_t, int> count;
                for (auto t : triangles)
                    for (int k = 0; k < 3; ++k)
                        ++count[edgeKey(t[k], t[(k + 1) % 3])];
                for (size_t i = 0; i < triangles.size() && !found; ++i)
                    for (int k = 0; k < 3; ++k) {
                        auto t = triangles[i];
                        auto a = t[k], b = t[(k + 1) % 3], c = t[(k + 2) % 3];
                        if (count[edgeKey(a, b)] != 1) continue;
                        auto ab = out.positions[b] - out.positions[a], aq = out.positions[q] - out.positions[a];
                        float l2 = glm::dot(ab, ab);
                        if (l2 < eps * eps) continue;
                        float f = glm::dot(aq, ab) / l2;
                        if (f > 0 && f < 1 && glm::length(aq - ab * f) < eps) {
                            triangles[i] = {a, q, c};
                            triangles.push_back({q, b, c});
                            found = true;
                            break;
                        }
                    }
                if (!found) {
                    error = "Could not weld the hole cap";
                    return {};
                }
            }
        for (auto t : triangles) {
            if (glm::dot(glm::cross(out.positions[t.y] - out.positions[t.x], out.positions[t.z] - out.positions[t.x]),
                         n) < 0)
                std::swap(t.y, t.z);
            out.triangles.push_back(t);
            out.polygroups.resize(out.triangleCount(), group);
            if (source.hasHidden()) out.hidden.push_back(0);
            if (source.hasUvs())
                for (int k = 0; k < 3; ++k)
                    out.uvs.push_back({glm::dot(out.positions[t[k]], u), glm::dot(out.positions[t[k]], v)});
        }
    }
    if (out.hasQuads()) out.quadPartners.resize(out.triangleCount(), Mesh::kNoQuad);
    finish(out);
    return out;
}

Mesh groupLoops(const Mesh& source, float inset, int loops, std::string& error) {
    inset = std::clamp(inset, 0.001f, .45f);
    loops = std::clamp(loops, 1, 8);
    std::set<uint32_t> groups;
    for (uint32_t t = 0; t < source.triangleCount(); ++t)
        if (!source.triangleHidden(t)) groups.insert(source.polygroupAt(t));
    std::vector<Mesh> parts;
    for (auto group : groups) {
        std::vector<uint8_t> keep(source.triangleCount());
        for (uint32_t t = 0; t < source.triangleCount(); ++t)
            keep[t] = !source.triangleHidden(t) && source.polygroupAt(t) == group;
        Mesh m = extractTriangles(source, keep);
        m.layers.clear();
        m.base.clear();
        m.paintLayers.clear();
        m.paintBase.clear();
        const bool hadUvs = m.hasUvs();
        auto edges = edgeMap(m);
        std::vector<Edge> boundary;
        std::set<uint32_t> boundaryVerts;
        for (auto [key, e] : edges)
            if (e.count == 1) {
                boundary.push_back(e);
                boundaryVerts.insert(e.a);
                boundaryVerts.insert(e.b);
            }
        if (boundary.empty()) {
            parts.push_back(std::move(m));
            continue;
        }
        glm::vec3 center(0);
        for (auto p : m.positions)
            center += p;
        center /= float(m.vertexCount());
        auto original = m.positions;
        std::map<uint32_t, std::vector<uint32_t>> rings;
        for (auto v : boundaryVerts) {
            auto& ring = rings[v];
            ring.push_back(v);
            for (int j = 1; j <= loops; ++j) {
                uint32_t id = uint32_t(m.vertexCount());
                m.positions.push_back(glm::mix(original[v], center, inset * (1 - float(j) / loops)));
                ring.push_back(id);
                if (source.hasColors()) m.colors.push_back(m.colors[v]);
                if (source.hasMask()) m.mask.push_back(m.mask[v]);
            }
            m.positions[v] = glm::mix(original[v], center, inset);
        }
        // ring[0] is the inset patch; ring.back() lies on the original group boundary.
        for (auto e : boundary)
            for (int j = 0; j < loops; ++j)
                addQuad(m, rings[e.a][j], rings[e.a][j + 1], rings[e.b][j + 1], rings[e.b][j], group);
        // New ring UVs are planar; original patch corner UVs remain intact.
        if (hadUvs) m.uvs.resize(m.triangleCount() * 3, glm::vec2(0));
        m.hidden.clear();
        finish(m);
        parts.push_back(std::move(m));
    }
    if (parts.empty()) {
        error = "No visible PolyGroups";
        return {};
    }
    std::vector<const Mesh*> refs;
    for (auto& m : parts)
        refs.push_back(&m);
    Mesh out = mergeMeshes(refs);
    // Adjacent group borders share exact positions. Weld restores the original connected surface.
    out = weldImported(out);
    finish(out);
    return out;
}
Mesh panelLoops(const Mesh& source, float thickness, float inset, int loops, std::string& error) {
    Mesh borders = groupLoops(source, inset, loops, error);
    if (borders.vertexCount() == 0) return {};
    auto parts = splitByPolygroups(borders);
    if (parts.empty()) parts.push_back(std::move(borders));
    std::vector<Mesh> panels;
    for (auto& part : parts) {
        part.ensureMask(1);
        std::fill(part.mask.begin(), part.mask.end(), 1);
        ExtractOptions options;
        options.thickness = thickness;
        options.smooth = 0;
        options.doubleSided = true;
        Mesh panel = extractShell(part, options, error);
        if (panel.vertexCount() == 0) return {};
        panel.mask.clear();
        panels.push_back(std::move(panel));
    }
    std::vector<const Mesh*> refs;
    for (auto& m : panels)
        refs.push_back(&m);
    Mesh out = mergeMeshes(refs);
    finish(out);
    return out;
}

void registerStudioTools(CommandRegistry& registry) {
    using T = ParamSpec::Type;
    registry.add({"morph_store",
                  "Store a topology-bound Morph Target for this SubTool; saved in projects.",
                  {},
                  [](Document& d, const Args&) -> CommandResult {
                      if (hasMicro(d.mesh)) return {false, "Bake Micro before storing a Morph Target"};
                      Mesh m = d.mesh;
                      m.morphTarget = m.positions;
                      m.morphTopologyKey = meshTopologyKey(m);
                      d.replaceMeshUndoable(std::move(m), "Store Morph Target");
                      return {true, "Morph Target stored"};
                  }});
    registry.add({"morph_delete",
                  "Delete the active SubTool's stored Morph Target.",
                  {},
                  [](Document& d, const Args&) -> CommandResult {
                      Mesh m = d.mesh;
                      m.morphTarget.clear();
                      m.morphTopologyKey = 0;
                      d.replaceMeshUndoable(std::move(m), "Delete Morph Target");
                      return {true, "Morph Target deleted"};
                  }});
    registry.add({"morph_switch",
                  "Swap the current surface and stored Morph Target. Topology must still match.",
                  {},
                  [](Document& d, const Args&) -> CommandResult {
                      if (!hasMorphTarget(d.mesh)) return {false, "Store a Morph Target with matching topology first"};
                      Mesh m = d.mesh;
                      auto target = m.morphTarget;
                      m.morphTarget = m.positions;
                      auto& base = m.layerTarget(-1);
                      for (size_t v = 0; v < m.vertexCount(); ++v)
                          base[v] += target[v] - m.positions[v];
                      if (!m.layers.empty())
                          m.evaluateAll();
                      else
                          m.positions = target;
                      m.recomputeAllNormals();
                      d.replaceMeshUndoable(std::move(m), "Switch Morph Target");
                      return {true, "Morph Target switched"};
                  }});
    auto topology = [&](const char* name, const char* label, auto fn, std::vector<ParamSpec> params = {}) {
        registry.add({name, label, std::move(params), [fn, label](Document& d, const Args& a) -> CommandResult {
                          if (hasMicro(d.mesh) || subdivLevelCount(d.mesh) > 0)
                              return {false, "Bake Micro and remove subdivision levels before this topology operation"};
                          std::string error;
                          Mesh m = fn(d.mesh, a, error);
                          if (m.triangleCount() == 0) return {false, error.empty() ? "No geometry" : error};
                          d.replaceMeshUndoable(std::move(m), label);
                          return {true, label};
                      }});
    };
    topology("mesh_flip", "Flip normals", [](const Mesh& source, const Args&, std::string&) {
        Mesh m = source;
        for (size_t t = 0; t < m.triangleCount(); ++t) {
            std::swap(m.triangles[t].y, m.triangles[t].z);
            if (m.hasUvs()) std::swap(m.uvs[3 * t + 1], m.uvs[3 * t + 2]);
        }
        finish(m);
        return m;
    });
    topology("weld_points", "Weld coincident points", [](const Mesh& source, const Args&, std::string&) {
        auto m = weldImported(source);
        finish(m);
        return m;
    });
    topology("close_holes", "Close Holes",
             [](const Mesh& m, const Args&, std::string& error) { return closeMeshHoles(m, error); });
    const std::vector<ParamSpec> params = {{"inset", T::Number, "Group boundary inset fraction (0.001-0.45)", false},
                                           {"loops", T::Number, "Boundary loops (1-8)", false},
                                           {"thickness", T::Number, "Panel thickness as a share of size", false}};
    topology(
        "groups_loops", "Groups Loops",
        [](const Mesh& m, const Args& a, std::string& error) {
            return groupLoops(m, float(argNumber(a, "inset", .04)), int(argNumber(a, "loops", 2)), error);
        },
        params);
    topology(
        "panel_loops", "Panel Loops",
        [](const Mesh& m, const Args& a, std::string& error) {
            return panelLoops(m, float(argNumber(a, "thickness", .015)), float(argNumber(a, "inset", .04)),
                              int(argNumber(a, "loops", 2)), error);
        },
        params);
    registry.add({"subtool_split_masked",
                  "Split masked faces into a separate SubTool; layers and vertex attributes are kept.",
                  {},
                  [](Document& d, const Args&) -> CommandResult {
                      if (!d.mesh.hasMask()) return {false, "Mask part of the SubTool first"};
                      if (hasMicro(d.mesh) || subdivLevelCount(d.mesh) > 0)
                          return {false, "Bake Micro and remove subdivision levels before splitting"};
                      std::vector<uint8_t> keep(d.mesh.triangleCount()), rest(keep.size());
                      size_t count = 0;
                      for (size_t t = 0; t < keep.size(); ++t) {
                          auto tri = d.mesh.triangles[t];
                          keep[t] = !d.mesh.triangleHidden(t) &&
                                    (d.mesh.mask[tri.x] + d.mesh.mask[tri.y] + d.mesh.mask[tri.z]) / 3 >= .5f;
                          rest[t] = !keep[t];
                          count += keep[t];
                      }
                      if (count == 0 || count == keep.size())
                          return {false, "The mask must select only part of the mesh"};
                      Mesh part = extractTriangles(d.mesh, keep), other = extractTriangles(d.mesh, rest);
                      std::string name = d.activeSubtoolInfo().name;
                      UndoRecord record;
                      record.label = "Split masked points";
                      record.mesh = std::make_unique<Mesh>(d.mesh);
                      d.mesh = std::move(other);
                      d.bvh.build(d.mesh);
                      d.bvhDirty = false;
                      int added = d.addSubtool(std::move(part), name + " Masked", false);
                      record.subtool = std::make_shared<SubtoolStep>();
                      record.subtool->id = d.subtools[size_t(added)].id;
                      record.subtool->at = added;
                      d.undo.push(std::move(record));
                      d.modified = true;
                      return {true, "Masked points split into a SubTool"};
                  }});
}
} // namespace sf
