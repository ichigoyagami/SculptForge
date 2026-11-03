// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"
#include "core/PlaneTools.h"
#include "core/Primitives.h"
#include "core/Commands.h"
#include "core/Masking.h"
#include "core/Polygroups.h"
#include "core/SubTools.h"
#include <map>
#include <cmath>
using namespace sf;
namespace {
bool closed(const Mesh& m){std::map<std::pair<uint32_t,uint32_t>,int> edges;
 for(auto t:m.triangles)for(int j=0;j<3;++j)++edges[{t[j],t[(j+1)%3]}];
 for(auto [e,n]:edges)if(n!=1 || edges[{e.second,e.first}]!=1)return false;return !edges.empty();}
}
TEST(knife_caps_cube_sphere_and_torus){
 for(auto mesh:{makeCube(8),makeSculptQuadSphere(3),makeTorus(32)}){
  for(auto normal:{glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,1),glm::vec3(1,1,.3f)}){
   std::string error;auto out=knifePlane(mesh,normal,.13f,error);
   CHECK(error.empty());CHECK(out.vertexCount()>0);
   if(!closed(out)){std::printf("open knife source=%zu normal=%g,%g,%g source_closed=%d\n",mesh.vertexCount(),normal.x,normal.y,normal.z,closed(mesh));std::map<std::pair<uint32_t,uint32_t>,int> e;for(auto t:out.triangles)for(int k=0;k<3;++k)++e[{t[k],t[(k+1)%3]}];int count=0;for(auto [key,n]:e)if(n!=1 || e[{key.second,key.first}]!=1){if(count++<8){auto a=out.positions[key.first],b=out.positions[key.second];std::printf(" edge %u,%u n%d reverse%d (%g,%g,%g)-(%g,%g,%g)\n",key.first,key.second,n,e[{key.second,key.first}],a.x,a.y,a.z,b.x,b.y,b.z);}}}
   CHECK(closed(out));
   for(auto p:out.positions)CHECK(glm::dot(normal,p)<=.1301f);
  }
 }
}
TEST(knife_and_clip_undo_masks_and_symmetry){
 CommandRegistry commands;auto d=makeDoc(makeCube(5));auto original=d.mesh.positions;auto tris=d.mesh.triangles;
 auto result=commands.execute(d,"knife_plane",{{"normal",std::string("1,0,0")},{"offset",.15}});
 CHECK(result.ok);CHECK(closed(d.mesh));CHECK(d.undo.undoCount()==1);
 CHECK(d.undo.undo(d.mesh,d.bvh));CHECK(d.mesh.positions==original);CHECK(d.mesh.triangles==tris);
 d.mesh.ensureMask()[0]=1;auto protectedPoint=d.mesh.positions[0];
 result=commands.execute(d,"clip_plane",{{"normal",std::string("1,0,0")},{"offset",-.2}});
 CHECK(result.ok);CHECK(d.mesh.triangles==tris);CHECK(d.mesh.positions[0]==protectedPoint);
 CHECK(d.undo.undo(d.mesh,d.bvh));CHECK(d.mesh.positions==original);
 result=commands.execute(d,"knife_plane",{{"normal",std::string("1,0,0")},{"offset",.2}});
 CHECK(!result.ok);CHECK(d.mesh.positions==original);
 maskClear(d.mesh);
 result=commands.execute(d,"knife_plane",{{"normal",std::string("1,0,0")},{"offset",.2},{"symmetry",std::string("x")}});
 CHECK(result.ok);CHECK(closed(d.mesh));for(auto p:d.mesh.positions)CHECK(std::abs(p.x)<=.2001f);
}
TEST(topology_mask_soft_joint_and_polygroup){
 auto m=makeCube(16);uint32_t origin=0,tip=0;
 for(uint32_t v=0;v<m.vertexCount();++v){if(m.positions[v].x<m.positions[origin].x)origin=v;if(m.positions[v].x>m.positions[tip].x)tip=v;}
 CHECK(maskTopology(m,origin,tip,false,false));CHECK(m.mask[origin]>.99f);CHECK(m.mask[tip]<.01f);
 bool soft=false;for(auto w:m.mask)soft|=w>0 && w<1;CHECK(soft);
 auto d=makeDoc(m);CommandRegistry commands;auto old=d.mesh.mask;
 CHECK(commands.execute(d,"mask_except_polygroup",{{"group",double(m.polygroupAt(0))}}).ok);
 CHECK(d.undo.undo(d.mesh,d.bvh));CHECK(d.mesh.mask==old);
}

TEST(slice_keeps_both_sides_welded_and_undoes){
 CommandRegistry commands;
 for(auto mesh:{makeCube(6),makeSculptQuadSphere(3),makeTorus(24)}){
  auto doc=makeDoc(mesh);auto result=commands.execute(doc,"slice_plane",{{"normal",std::string("1,.2,.1")},{"offset",.13}});
  CHECK(result.ok);CHECK(doc.mesh.triangleCount()>mesh.triangleCount());CHECK(closed(doc.mesh));CHECK(visiblePolygroupCount(doc.mesh)>=2);
  for(uint32_t v=0;v<mesh.vertexCount();++v)CHECK(doc.mesh.positions[v]==mesh.positions[v]);
  CHECK(glm::length(doc.mesh.bounds().min-mesh.bounds().min)<1e-6f);CHECK(glm::length(doc.mesh.bounds().max-mesh.bounds().max)<1e-6f);
  auto sliced=doc.mesh;CHECK(doc.undo.undo(doc.mesh,doc.bvh));CHECK(doc.mesh.triangles==mesh.triangles);CHECK(doc.mesh.positions==mesh.positions);
  CHECK(doc.undo.redo(doc.mesh,doc.bvh));CHECK(doc.mesh.triangles==sliced.triangles);
 }
}
TEST(slice_interpolates_attributes_and_leaves_hidden_groups){
 auto mesh=makeCube(6);mesh.clearPolygroups();mesh.ensureMask(.35f);mesh.ensureColors({.2f,.4f,.6f});int layer=mesh.addLayer("Detail");
 mesh.ensureHidden();for(uint32_t t=0;t<mesh.triangleCount();++t)if(t%3==0)mesh.hidden[t]=1;
 std::string error;auto out=slicePlane(mesh,{1,.2f,.1f},.13f,error);
 CHECK(error.empty());CHECK(out.layers.size()==1);CHECK(out.base.size()==out.vertexCount());CHECK(out.layers[layer].delta.size()==out.vertexCount());
 CHECK(out.mask.size()==out.vertexCount());CHECK(out.colors.size()==out.vertexCount());
 bool hidden=false;for(uint32_t t=0;t<out.triangleCount();++t)if(out.triangleHidden(t)){hidden=true;CHECK(out.polygroupAt(t)==0);}CHECK(hidden);
 for(auto m:out.mask)CHECK(std::abs(m-.35f)<1e-6f);
}
TEST(knife_caps_have_quads_and_untouched_parts_remain){
 CommandRegistry commands;auto left=makeCube(5),right=makeCube(3);for(auto& p:right.positions)p.x+=4;
 auto doc=makeDoc(mergeMeshes({&left,&right}));auto result=commands.execute(doc,"knife_plane",{{"normal",std::string("1,0,0")},{"offset",.13}});
 CHECK(result.ok);CHECK(closed(doc.mesh));CHECK(doc.mesh.bounds().max.x>4.5f);uint32_t cap=nextPolygroupId(doc.mesh)-1;size_t quads=0;
 for(uint32_t t=0;t<doc.mesh.triangleCount();++t)if(doc.mesh.polygroupAt(t)==cap && doc.mesh.quadPartnerAt(t)!=Mesh::kNoQuad)++quads;
 CHECK(quads>0);CHECK(doc.undo.undoCount()==1);
}
TEST(clip_profiles_preserve_topology_masks_and_layers){
 CommandRegistry commands;auto mesh=makeCube(6);mesh.ensureMask()[0]=1;mesh.addLayer("Clip layer");auto doc=makeDoc(mesh);doc.activeLayer=0;
 auto result=commands.execute(doc,"clip_profile",{{"points",std::string("-.5 -.5 .5 -.5 .5 .5 -.5 .5")},{"closed",true}});
 CHECK(result.ok);CHECK(doc.mesh.triangles==mesh.triangles);CHECK(doc.mesh.vertexCount()==mesh.vertexCount());CHECK(doc.mesh.positions[0]==mesh.positions[0]);CHECK(doc.mesh.layers.size()==1);
 CHECK(doc.undo.undo(doc.mesh,doc.bvh));CHECK(doc.mesh.positions==mesh.positions);
}
TEST(profile_slice_is_topology_and_knife_rejects_invalid_curves_atomically){
 CommandRegistry commands;auto mesh=makeSculptQuadSphere(3);auto doc=makeDoc(mesh);
 auto result=commands.execute(doc,"slice_profile",{{"points",std::string("-.8 -.2 0 .2 .8 -.2")}});
 CHECK(result.ok);CHECK(closed(doc.mesh));CHECK(doc.mesh.triangleCount()>mesh.triangleCount());CHECK(visiblePolygroupCount(doc.mesh)>=2);
 CHECK(doc.undo.undo(doc.mesh,doc.bvh));CHECK(doc.mesh.positions==mesh.positions);auto before=doc.undo.undoCount();
 result=commands.execute(doc,"knife_profile",{{"points",std::string("-.8 -.2 0 .2 .8 -.2")}});
 CHECK(!result.ok);CHECK(doc.mesh.positions==mesh.positions);CHECK(doc.undo.undoCount()==before);
 result=commands.execute(doc,"knife_profile",{{"points",std::string("-.8 .2 0 -.2 .8 .2")}});CHECK(result.ok);CHECK(closed(doc.mesh));
}
TEST(clip_profile_symmetry_uses_opposite_hemispheres_once){
 CommandRegistry commands;auto mesh=makeSculptQuadSphere(3);auto doc=makeDoc(mesh);
 auto result=commands.execute(doc,"clip_profile",{{"points",std::string(".05 -.5 .85 -.5 .85 .5 .05 .5")},{"closed",true},{"symmetry",std::string("x")}});
 CHECK(result.ok);CHECK(doc.mesh.vertexCount()==mesh.vertexCount());
 for(uint32_t v=0;v<mesh.vertexCount();++v){auto reflected=mesh.positions[v];reflected.x=-reflected.x;uint32_t match=0;
  for(uint32_t u=0;u<mesh.vertexCount();++u)if(glm::length(mesh.positions[u]-reflected)<glm::length(mesh.positions[match]-reflected))match=u;
  auto expected=doc.mesh.positions[match];expected.x=-expected.x;CHECK(glm::length(doc.mesh.positions[v]-expected)<1e-5f);
  if(std::abs(mesh.positions[v].x)<1e-6f)CHECK(std::abs(doc.mesh.positions[v].x)<1e-6f);
 }CHECK(doc.undo.undo(doc.mesh,doc.bvh));CHECK(doc.mesh.positions==mesh.positions);
}
