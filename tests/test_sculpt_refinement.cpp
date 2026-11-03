// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"
#include "core/BrushAlpha.h"
#include "core/Commands.h"
#include "core/Primitives.h"
#include "core/QRemesh.h"
#include "core/Sculpt.h"
#include "io/ObjIO.h"
#include <cmath>
#include <fstream>
#include <sstream>
using namespace sf;
namespace {
constexpr Symmetry noSymmetry{false,false,false};
Mesh bumpyPlane(int n=64) {
    Mesh m=makePlane(n);
    for(size_t i=0;i<m.vertexCount();++i) {
        auto& p=m.positions[i];
        p.y=0.04f*std::sin(p.x*7)*std::cos(p.z*5)+0.018f*(float((i*2654435761u)%11)/10-0.5f);
    }
    m.recomputeAllNormals();return m;
}
Mesh sculpt(Mesh m,BrushType type,DabParams p) {
    Document d=makeDoc(std::move(m));
    d.sculptor.beginStroke(d.mesh,"test");d.sculptor.dab(d.mesh,d.bvh,type,p,noSymmetry);d.sculptor.endStroke();
    return d.mesh;
}
DabParams parameters() {
    DabParams p;p.center={0,0,0};p.radius=0.5f;p.viewDir={0,-1,0};p.alphaUp={0,0,1};return p;
}
float change(const Mesh& a,const Mesh& b) {
    float sum=0;for(size_t i=0;i<a.vertexCount();++i)sum+=glm::distance(a.positions[i],b.positions[i]);return sum;
}
float roughness(const Mesh& m) {
    float sum=0;int count=0;
    for(uint32_t v=0;v<m.vertexCount();++v) {
        auto p=m.positions[v];if(p.x*p.x+p.z*p.z>0.09f)continue;
        auto ring=m.surfaceNeighbors(v);float y=0;for(auto u:ring)y+=m.positions[u].y;
        if(!ring.empty()){sum+=std::abs(p.y-y/ring.size());++count;}
    }
    return sum/std::max(count,1);
}
}
TEST(sculpt_zero_intensity_and_zero_pressure_do_not_change_clay) {
    const Mesh original=bumpyPlane();auto p=parameters();
    for(auto type:{BrushType::Clay,BrushType::ClayBuildup,BrushType::DamStandard,BrushType::Smooth}) {
        p.strength=0;p.pressure=1;CHECK(sculpt(original,type,p).positions==original.positions);
        p.strength=1;p.pressure=0;CHECK(sculpt(original,type,p).positions==original.positions);
    }
}
TEST(sculpt_clay_intensity_is_monotonic_and_low_values_are_gentle) {
    const Mesh m=makePlane(64);auto p=parameters();float moved[3];
    for(auto type:{BrushType::Clay,BrushType::ClayBuildup}) {
        int i=0;for(float strength:{0.1f,0.5f,0.9f}){p.strength=strength;moved[i++]=change(m,sculpt(m,type,p));}
        CHECK(moved[0]>moved[1]*0.19f);CHECK(moved[0]<moved[1]*0.21f);CHECK(moved[1]<moved[2]);
        p.strength=0.5f;p.invert=true;Mesh negative=sculpt(m,type,p);
        float height=0;for(auto v:negative.positions)height+=v.y;CHECK(height<0);p.invert=false;
    }
}
TEST(sculpt_clay_buildup_tip_follows_stroke_and_can_be_locked) {
    const Mesh m=makePlane(64);auto p=parameters();p.strokeDelta={1,0,1};
    p.followStroke=true;const Mesh follows=sculpt(m,BrushType::ClayBuildup,p);
    p.followStroke=false;const Mesh fixed=sculpt(m,BrushType::ClayBuildup,p);
    CHECK(change(follows,fixed)>0.01f);
    p.strokeDelta={-1,0,0};CHECK(sculpt(m,BrushType::ClayBuildup,p).positions==fixed.positions);
    p.followStroke=true;p.strokeDelta={1,0,1};BrushAlpha alpha=BrushAlpha::square();p.alpha=&alpha;
    Mesh clayFollows=sculpt(m,BrushType::Clay,p);p.followStroke=false;
    CHECK(change(clayFollows,sculpt(m,BrushType::Clay,p))>0.01f);
}
TEST(sculpt_clay_settles_within_stroke_but_buildup_keeps_accumulating) {
    auto p=parameters();p.strength=0.5f;
    auto height=[&](BrushType brush) {
        Document d=makeDoc(makePlane(64));d.sculptor.beginStroke(d.mesh,"clay");
        for(int i=0;i<16;++i)d.sculptor.dab(d.mesh,d.bvh,brush,p,noSymmetry);
        d.sculptor.endStroke();return d.mesh.positions[32*65+32].y;
    };
    const float clay=height(BrushType::Clay),buildup=height(BrushType::ClayBuildup);
    CHECK(clay>0.035f && clay<=p.radius*0.081f);
    CHECK(buildup>clay*2);
}
TEST(sculpt_damstandard_cuts_and_raises_narrow_crease_without_lengthwise_pinch) {
    const Mesh m=makePlane(96);auto p=parameters();p.strokeDelta={0.1f,0,0};p.strength=0.5f;
    const Mesh cut=sculpt(m,BrushType::DamStandard,p);
    p.invert=true;const Mesh ridge=sculpt(m,BrushType::DamStandard,p);
    const size_t center=48*97+48,shoulder=48*97+56;
    CHECK(cut.positions[center].y < -0.01f);
    CHECK(ridge.positions[center].y > 0.01f);
    // makePlane indexes z by rows and x by columns: a transverse shoulder lies on the next rows.
    const size_t across=56*97+48;
    CHECK(std::abs(cut.positions[center].y)>std::abs(cut.positions[across].y)*3);
    CHECK(std::abs(cut.positions[shoulder].y)>std::abs(cut.positions[across].y));
    for(size_t i=0;i<m.vertexCount();++i){
        CHECK(std::abs(cut.positions[i].x-m.positions[i].x)<0.000001f);
        CHECK(std::abs(cut.positions[i].y+ridge.positions[i].y)<0.000001f);
    }
    p.invert=false;p.damPinch=0;const Mesh noPinch=sculpt(m,BrushType::DamStandard,p);
    for(size_t i=0;i<m.vertexCount();++i)CHECK(noPinch.positions[i].z==m.positions[i].z);
    CHECK(cut.triangles==m.triangles && cut.quadPartners==m.quadPartners);
}
TEST(sculpt_z_intensity_and_pen_pressure_multiply_without_a_second_curve) {
    const Mesh m=makePlane(64);auto p=parameters();p.strokeDelta={0.1f,0,0};
    for(auto type:{BrushType::Standard,BrushType::Clay,BrushType::ClayBuildup,BrushType::DamStandard}) {
        p.strength=0.25f;p.pressure=1;const Mesh low=sculpt(m,type,p);
        p.strength=0.5f;p.pressure=0.5f;CHECK(sculpt(m,type,p).positions==low.positions);
        p.strength=1;p.pressure=1;const Mesh high=sculpt(m,type,p);
        CHECK(std::abs(change(m,high)/change(m,low)-4)<0.001f);
    }
}
TEST(sculpt_smooth_reduces_dense_quad_noise_and_keeps_topology) {
    Mesh m=bumpyPlane(96);auto p=parameters();p.strength=0.9f;
    Mesh result=sculpt(m,BrushType::Smooth,p);
    CHECK(roughness(result)<roughness(m)*0.65f);
    CHECK(result.triangles==m.triangles);CHECK(result.quadPartners==m.quadPartners);
    p.strength=0.1f;
    CHECK(roughness(sculpt(m,BrushType::Smooth,p))<=roughness(m)+0.000001f);
}
TEST(sculpt_smooth_removes_broad_bumps_at_default_intensity_across_densities) {
    float lowDensity = 0;
    for (int n : {64, 128, 256}) {
        Mesh m = makePlane(n);
        for (auto& v : m.positions) v.y = 0.12f * std::exp(-(v.x*v.x + v.z*v.z) / (2*0.08f*0.08f));
        m.recomputeAllNormals();
        auto p = parameters();p.center.y=0.12f;p.strength=0.5f;
        const size_t center = size_t(n/2)*(n+1)+n/2;
        Mesh standard = sculpt(m, BrushType::Smooth, p);
        p.smoothMode = SmoothMode::Stronger;
        Mesh stronger = sculpt(m, BrushType::Smooth, p);
        const float retained = standard.positions[center].y / m.positions[center].y;
        std::printf("  grid=%d standard peak=%.4f stronger peak=%.4f\n", n, retained,
                    stronger.positions[center].y / m.positions[center].y);
        CHECK(retained < 0.95f); // visible removal of broad form, not just single-edge noise
        CHECK(stronger.positions[center].y < standard.positions[center].y * 0.85f);
        CHECK(stronger.positions[center].y >= 0);
        CHECK(standard.triangles == m.triangles && stronger.quadPartners == m.quadPartners);
        if(n==64) lowDensity=retained;else CHECK(std::abs(retained-lowDensity)<0.15f);
    }
}
TEST(sculpt_smooth_intensity_and_pressure_give_progressive_results) {
    const Mesh m=bumpyPlane(96);auto p=parameters();float before=roughness(m),previous=before;
    for(float intensity:{0.1f,0.5f,1.0f}){
        p.strength=intensity;float after=roughness(sculpt(m,BrushType::Smooth,p));
        CHECK(after<previous);previous=after;
    }
    p.strength=0.5f;p.pressure=0.5f;Mesh pressure=sculpt(m,BrushType::Smooth,p);
    p.strength=0.25f;p.pressure=1;
    CHECK(pressure.positions==sculpt(m,BrushType::Smooth,p).positions);
}
TEST(sculpt_smooth_modes_respect_mask_and_brush_boundary) {
    Mesh m=bumpyPlane(64);
    m.mask.resize(m.vertexCount());
    for(size_t i=0;i<m.vertexCount();++i)m.mask[i]=m.positions[i].x<0?1.0f:0.0f;
    auto p=parameters();p.strength=1;
    for(auto mode:{SmoothMode::Standard,SmoothMode::Stronger,SmoothMode::Volume}){
        p.smoothMode=mode;Mesh result=sculpt(m,BrushType::Smooth,p);
        CHECK(change(m,result)>0.01f);
        for(size_t i=0;i<m.vertexCount();++i){
            if(m.mask[i]==1 || glm::distance(m.positions[i],p.center)>=p.radius)
                CHECK(m.positions[i]==result.positions[i]);
            CHECK(std::isfinite(result.positions[i].y));
        }
        CHECK(result.triangles==m.triangles && result.quadPartners==m.quadPartners);
    }
}
TEST(sculpt_smooth_volume_alternative_reduces_sphere_shrinkage) {
    Mesh m=makeSculptQuadSphere(5);auto p=parameters();p.center={0,0,1};p.viewDir={0,0,-1};p.strength=0.5f;
    auto volume=[](const Mesh& mesh){
        double sum=0;for(auto t:mesh.triangles)sum+=glm::dot(mesh.positions[t.x],glm::cross(mesh.positions[t.y],mesh.positions[t.z]));
        return std::abs(sum/6);
    };
    const double original=volume(m);
    const Mesh standard=sculpt(m,BrushType::Smooth,p);
    p.smoothMode=SmoothMode::Volume;const Mesh alternate=sculpt(m,BrushType::Smooth,p);
    CHECK(volume(standard)<original);
    CHECK(std::abs(volume(alternate)-original)<std::abs(volume(standard)-original)*0.5);
    CHECK(change(m,alternate)>0);
}
TEST(sculpt_quad_smoothing_and_normals_ignore_render_diagonal) {
    Mesh a=bumpyPlane(24),b=a;
    for(uint32_t t=0;t<b.triangleCount();t+=2){
        glm::uvec4 f;CHECK(a.exportFace(t,f)==4);
        b.triangles[t]={f[0],f[1],f[3]};b.triangles[t+1]={f[1],f[2],f[3]};
    }
    b.rebuildTopology();b.recomputeAllNormals();
    for(size_t i=0;i<a.vertexCount();++i)CHECK(glm::distance(a.normals[i],b.normals[i])<0.00001f);
    auto p=parameters();p.strength=0.8f;
    Mesh sa=sculpt(a,BrushType::Smooth,p),sb=sculpt(b,BrushType::Smooth,p);
    for(size_t i=0;i<a.vertexCount();++i)CHECK(glm::distance(sa.positions[i],sb.positions[i])<0.00001f);
    auto ring=a.surfaceNeighbors(12*25+12);CHECK(ring.size()==4);
    a.positions[12*25+12].y+=0.1f;uint32_t v=12*25+12;a.updateNormals(std::span<const uint32_t>(&v,1));
    Mesh full=a;full.recomputeAllNormals();
    for(size_t i=0;i<a.vertexCount();++i)CHECK(glm::distance(a.normals[i],full.normals[i])<0.00001f);
}
TEST(sculpt_quad_sphere_and_dynamesh_export_real_quads_and_undo) {
    Mesh sphere=makeSculptQuadSphere(3);auto topology=topologyReport(sphere);
    CHECK(topology.quadFraction()==1);CHECK(topology.manifold());CHECK(topology.boundaryEdges==0);
    for(auto p:sphere.positions)CHECK(std::abs(glm::length(p)-1)<0.00001f);
    CommandRegistry commands;Document d=makeDoc(makeIcosphere(2));auto original=d.mesh.positions;
    CHECK(commands.execute(d,"dynamesh",{{"resolution",32.0},{"keep_texture",false}}).ok);
    topology=topologyReport(d.mesh);CHECK(topology.quadFraction()==1);CHECK(topology.manifold());
    std::string error;const auto path=tempPath("sculpt_refinement_quads.obj");CHECK(saveObj(path,d.mesh,error));
    std::ifstream file(path);std::string line;int faces=0;
    while(std::getline(file,line))if(line.starts_with("f ")){std::istringstream row(line);std::string word;int count=-1;while(row>>word)++count;CHECK(count==4);++faces;}
    CHECK(faces>0);CHECK(commands.execute(d,"undo").ok);CHECK(d.mesh.positions==original);
}
