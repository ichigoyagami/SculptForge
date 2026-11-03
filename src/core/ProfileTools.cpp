// SculptForge - open source digital sculpting
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/PlaneTools.h"
#include "core/Commands.h"
#include "core/Micro.h"
#include "core/Multires.h"
#include "core/Polygroups.h"
#include "core/ScreenSelect.h"
#include "core/Sculpt.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <unordered_map>
namespace sf {
namespace {
struct Plane{glm::vec3 n;float offset;};
uint64_t edgeKey(uint32_t a,uint32_t b){return (uint64_t(std::min(a,b))<<32)|std::max(a,b);}
// Intersections are shared by index, retaining a connected surface across Slice.
Mesh bisect(const Mesh& src,const Plane& plane){
 Mesh out=src;out.triangles.clear();out.uvs.clear();out.uvFlags.clear();out.polygroups.clear();out.hidden.clear();out.clearQuads();out.morphTarget.clear();out.morphTopologyKey=0;
 const float eps=std::max(glm::length(src.bounds().extent()),.001f)*1e-6f;
 std::vector<float> dist(src.vertexCount());for(uint32_t v=0;v<src.vertexCount();++v){float d=glm::dot(plane.n,src.positions[v])-plane.offset;dist[v]=std::abs(d)<eps?0:d;}
 std::unordered_map<uint64_t,uint32_t> cuts;
 auto cut=[&](uint32_t a,uint32_t b){
  if(dist[a]==0)return a;if(dist[b]==0)return b;
  auto key=edgeKey(a,b);auto found=cuts.find(key);if(found!=cuts.end())return found->second;
  float t=dist[a]/(dist[a]-dist[b]);uint32_t v=uint32_t(out.vertexCount());out.positions.push_back(glm::mix(src.positions[a],src.positions[b],t));
  if(src.hasMask())out.mask.push_back(glm::mix(src.mask[a],src.mask[b],t));
  if(src.hasColors())out.colors.push_back(glm::mix(src.colors[a],src.colors[b],t));
  if(!src.base.empty())out.base.push_back(glm::mix(src.base[a],src.base[b],t));
  for(size_t i=0;i<out.layers.size();++i)out.layers[i].delta.push_back(glm::mix(src.layers[i].delta[a],src.layers[i].delta[b],t));
  if(!src.paintBase.empty())out.paintBase.push_back(glm::mix(src.paintBase[a],src.paintBase[b],t));
  for(size_t i=0;i<out.paintLayers.size();++i){out.paintLayers[i].color.push_back(glm::mix(src.paintLayers[i].color[a],src.paintLayers[i].color[b],t));out.paintLayers[i].alpha.push_back(glm::mix(src.paintLayers[i].alpha[a],src.paintLayers[i].alpha[b],t));}
  cuts[key]=v;return v;
 };
 struct Corner{uint32_t v;glm::vec2 uv;};std::vector<uint32_t> remap(src.triangleCount(),Mesh::kNoQuad);
 for(uint32_t ti=0;ti<src.triangleCount();++ti){
  auto tri=src.triangles[ti];bool above=false,below=false;for(int j=0;j<3;++j){above|=dist[tri[j]]>0;below|=dist[tri[j]]<0;}
  auto emit=[&](const std::vector<Corner>& poly){uint32_t first=uint32_t(out.triangleCount());
   for(size_t j=1;j+1<poly.size();++j){if(poly[0].v==poly[j].v || poly[0].v==poly[j+1].v || poly[j].v==poly[j+1].v)continue;
    out.triangles.push_back({poly[0].v,poly[j].v,poly[j+1].v});out.polygroups.push_back(src.polygroupAt(ti));out.hidden.push_back(src.triangleHidden(ti));
    if(src.hasUvs()){out.uvs.push_back(poly[0].uv);out.uvs.push_back(poly[j].uv);out.uvs.push_back(poly[j+1].uv);}}
   if(poly.size()==4 && out.triangleCount()==first+2)out.pairQuad(first,first+1);
  };
  if(!above || !below){remap[ti]=uint32_t(out.triangleCount());std::vector<Corner> poly;for(int j=0;j<3;++j)poly.push_back({tri[j],src.hasUvs()?src.uvs[3*ti+j]:glm::vec2(0)});emit(poly);continue;}
  for(int side:{-1,1}){std::vector<Corner> poly;
   for(int j=0;j<3;++j){auto a=tri[j],b=tri[(j+1)%3];auto ua=src.hasUvs()?src.uvs[3*ti+j]:glm::vec2(0),ub=src.hasUvs()?src.uvs[3*ti+(j+1)%3]:glm::vec2(0);
    if(dist[a]*side>=0)poly.push_back({a,ua});if(dist[a]*dist[b]<0)poly.push_back({cut(a,b),glm::mix(ua,ub,dist[a]/(dist[a]-dist[b]))});}emit(poly);}
 }
 for(uint32_t t=0;t<src.triangleCount();++t){auto u=src.quadPartnerAt(t);if(u!=Mesh::kNoQuad && u>t && remap[t]!=Mesh::kNoQuad && remap[u]!=Mesh::kNoQuad)out.pairQuad(remap[t],remap[u]);}
 if(out.hasQuads())out.quadPartners.resize(out.triangleCount(),Mesh::kNoQuad);if(!src.hasHidden())out.hidden.clear();return out;
}
void groupsForRegion(Mesh& out,const std::function<bool(glm::vec3)>& inside){
 uint32_t next=nextPolygroupId(out);std::map<uint32_t,uint32_t> groups;out.ensurePolygroups();
 for(uint32_t t=0;t<out.triangleCount();++t){if(out.triangleHidden(t))continue;auto tri=out.triangles[t];auto center=(out.positions[tri.x]+out.positions[tri.y]+out.positions[tri.z])/3.f;
  if(inside(center)){auto old=out.polygroups[t];if(!groups.count(old))groups[old]=next++;out.polygroups[t]=groups[old];}}
 out.rebuildTopology();out.recomputeAllNormals();
}
float cross(glm::vec2 a,glm::vec2 b){return a.x*b.y-a.y*b.x;}
std::vector<Plane> profilePlanes(const glm::mat4& vp,const std::vector<glm::vec2>& path,bool closed){
 std::vector<Plane> result;for(size_t i=0;i+1<path.size() || (closed && i<path.size());++i){auto a=path[i],b=path[(i+1)%path.size()],d=b-a;
  glm::vec4 p=glm::transpose(vp)*glm::vec4(d.y,-d.x,0,d.x*a.y-d.y*a.x);float length=glm::length(glm::vec3(p));if(length<1e-8f)continue;result.push_back({glm::vec3(p)/length,-p.w/length});}return result;
}
bool pathArg(const Args& args,std::vector<glm::vec2>& points,std::string& error){
 std::istringstream in(argString(args,"points"));glm::vec2 p;while(in>>p.x>>p.y){if(!std::isfinite(p.x+p.y)){error="Invalid profile point";return false;}if(points.empty() || glm::length(p-points.back())>1e-6f)points.push_back(p);}
 if(!in.eof() || points.size()<2 || points.size()>96){error="Profile needs 2 to 96 distinct x,y points";return false;}return true;
}
bool shaded(glm::vec2 q,const std::vector<glm::vec2>& path,glm::vec2* nearest=nullptr){
 float best=1e30f,side=0;glm::vec2 point{};for(size_t i=0;i+1<path.size();++i){auto a=path[i],d=path[i+1]-a;float l2=glm::dot(d,d);if(l2<1e-12f)continue;
  float t=std::clamp(glm::dot(q-a,d)/l2,i==0?-1e6f:0.f,i+2==path.size()?1e6f:1.f);auto p=a+d*t;float distance=glm::dot(q-p,q-p);
  if(distance<best){best=distance;side=cross(d,q-a);point=p;}}
 if(nearest)*nearest=point;return side<0;
}
}
Mesh slicePlane(const Mesh& src,glm::vec3 normal,float offset,std::string& error){
 float length=glm::length(normal);if(length<1e-8f || !std::isfinite(length+offset)){error="Invalid slice plane";return {};}
 normal/=length;offset/=length;
 const float eps=std::max(glm::length(src.bounds().extent()),.001f)*1e-6f;bool above=false,below=false;
 for(uint32_t v=0;v<src.vertexCount();++v)if(!src.vertexHidden(v)){const float d=glm::dot(normal,src.positions[v])-offset;above|=d>eps;below|=d< -eps;}
 if(!above || !below)return src;
 auto out=bisect(src,{normal,offset});if(out.triangles==src.triangles)out=src;
 groupsForRegion(out,[&](glm::vec3 p){return glm::dot(normal,p)>offset;});return out;
}
void registerProfileTools(CommandRegistry& registry){using T=ParamSpec::Type;
 registry.add({"slice_plane","Slice geometry into connected PolyGroups, keeping both sides; standard symmetry is intentionally ignored.",{{"normal",T::String,"Plane normal x,y,z",true},{"offset",T::Number,"Plane offset",false}},
 [](Document& doc,const Args& args)->CommandResult{
  if(hasMicro(doc.mesh) || subdivLevelCount(doc.mesh)>0)return {false,"Bake Micro or remove subdivision levels before Slice"};
  std::string raw=argString(args,"normal");std::replace(raw.begin(),raw.end(),',',' ');std::istringstream in(raw);glm::vec3 n;if(!(in>>n.x>>n.y>>n.z))return {false,"Invalid slice normal"};
  std::string error;auto out=slicePlane(doc.mesh,n,float(argNumber(args,"offset",0)),error);if(!error.empty())return {false,error};
  if(out.triangles==doc.mesh.triangles && out.polygroups==doc.mesh.polygroups)return {true,"Slice: nothing crossed"};
  doc.replaceMeshUndoable(std::move(out),"Slice");return {true,"Slice: both sides retained with new PolyGroups"};}});
 for(const std::string operation:{"clip_profile","knife_profile","slice_profile"})registry.add({operation,"Screen profile: Clip moves points, Knife cuts and caps, Slice retains both sides as PolyGroups.",
 {{"points",T::String,"2 to 96 NDC x,y pairs",true},{"view_proj",T::String,"View-projection matrix",false},{"closed",T::Bool,"Closed polygon",false},{"invert",T::Bool,"Reverse the side",false},{"symmetry",T::String,"Mirror axes xyz (Clip/Knife only)",false}},
 [operation](Document& doc,const Args& args)->CommandResult{
  bool clip=operation=="clip_profile",knife=operation=="knife_profile";if(doc.mesh.triangleCount()==0)return {false,"The mesh is empty"};
  if(hasMicro(doc.mesh) || subdivLevelCount(doc.mesh)>0)return {false,"Bake Micro or remove subdivision levels before profile tools"};
  std::vector<glm::vec2> path;std::string error;glm::mat4 vp;if(!pathArg(args,path,error) || !viewProjArg(doc.mesh,args,vp,error))return {false,error};
  bool closed=argBool(args,"closed",false),invert=argBool(args,"invert",false);if(closed && path.size()<3)return {false,"Closed profile needs at least 3 points"};
  auto planes=profilePlanes(vp,path,closed);if(planes.empty())return {false,"Profile is too small"};ScreenPolygon polygon(path);if(closed && !polygon.valid())return {false,"Closed profile must enclose an area"};
  auto inside=[&](glm::vec3 p){glm::vec2 q;if(!projectToNdc(vp,p,q))return false;return closed?polygon.contains(q):shaded(q,path);};
  if(!clip && !knife){Mesh out=doc.mesh;for(auto& plane:planes){out=bisect(out,plane);if(out.triangleCount()>std::max<size_t>(2000000,doc.mesh.triangleCount()*12))return {false,"Slice profile is too complex; use fewer curve points"};}
   if(out.triangleCount()==doc.mesh.triangleCount())return {true,"Slice: nothing crossed"};groupsForRegion(out,inside);doc.replaceMeshUndoable(std::move(out),"Slice profile");return {true,"Slice: topology added; both sides retained as PolyGroups (no standard symmetry)"};}
  std::string axes=argString(args,"symmetry");auto mirrors=Sculptor::mirrors({axes.find('x')!=std::string::npos,axes.find('y')!=std::string::npos,axes.find('z')!=std::string::npos});
  if(knife){if(closed)return {false,"Knife profile currently supports open convex curves; use Slice for closed shapes"};
   for(size_t j=0;j+1<path.size();++j)for(auto point:path)if(cross(path[j+1]-path[j],point-path[j])*(invert?-1.f:1.f)<-1e-5f)return {false,"KnifeCurve requires a convex kept profile; reverse the stroke or use SliceCurve"};
   if(doc.mesh.hasHidden() || std::any_of(doc.mesh.mask.begin(),doc.mesh.mask.end(),[](float x){return x>0;}))return {false,"Show all geometry and clear the mask before Knife"};
   Mesh out=doc.mesh;for(auto mirror:mirrors)for(auto plane:planes){plane.n*=mirror;if(invert){plane.n=-plane.n;plane.offset=-plane.offset;}out=knifePlane(out,plane.n,plane.offset,error,true);if(!error.empty() || out.triangleCount()==0)return {false,error};}
   doc.replaceMeshUndoable(std::move(out),"Knife profile");return {true,"Knife: profile cut and closed; new cap PolyGroups"};}
  glm::mat4 inverse=glm::inverse(vp);if(!std::isfinite(inverse[0][0]))return {false,"Invalid profile view"};
  glm::vec2 average(0);for(auto q:path)average+=q;average/=float(path.size());auto center=vp*glm::vec4(doc.mesh.bounds().center(),1);auto reference=inverse*glm::vec4(average,center.z/center.w,1);reference/=reference.w;
  float seam=std::max(glm::length(doc.mesh.bounds().extent()),.001f)*1e-6f;bool activeAxes[]={axes.find('x')!=std::string::npos,axes.find('y')!=std::string::npos,axes.find('z')!=std::string::npos};
  int layer=doc.activeLayer;if(layer>=int(doc.mesh.layers.size()))layer=-1;float weight=doc.mesh.layerTargetWeight(layer);if(std::abs(weight)<1e-3f)return {false,"Raise the active layer weight before Clip"};
  UndoRecord record;record.label="Clip profile";record.layer=layer;auto& target=doc.mesh.layerTarget(layer);
  for(uint32_t v=0;v<doc.mesh.vertexCount();++v){auto original=doc.mesh.positions[v],p=original;
   for(auto mirror:mirrors){bool hemisphere=true;for(int k=0;k<3;++k)if(activeAxes[k] && std::abs(reference[k])>seam && original[k]*mirror[k]*reference[k]<-seam)hemisphere=false;if(!hemisphere)continue;
    glm::vec4 h=vp*glm::vec4(p*mirror,1);if(h.w<=1e-8f)continue;glm::vec2 q=glm::vec2(h)/h.w,nearest{};bool selected;
    if(closed){selected=polygon.contains(q);if(selected!=invert)continue;float best=1e30f;
     for(size_t j=0;j<path.size();++j){auto a=path[j],d=path[(j+1)%path.size()]-a;float l2=glm::dot(d,d);if(l2<1e-12f)continue;auto point=a+d*std::clamp(glm::dot(q-a,d)/l2,0.f,1.f);auto delta=q-point;float distance=glm::dot(delta,delta);if(distance<best){best=distance;nearest=point;}}}
    else{selected=shaded(q,path,&nearest);if(selected==invert)continue;}
    auto result=inverse*glm::vec4(nearest,h.z/h.w,1);if(std::abs(result.w)>1e-8f)p=glm::vec3(result)/result.w*mirror;
   }
   for(int k=0;k<3;++k)if(activeAxes[k] && std::abs(original[k])<seam)p[k]=original[k];auto delta=(p-original)*vertexInfluence(doc.mesh,v);if(glm::dot(delta,delta)<1e-20f)continue;record.positions.add(v,target[v]);target[v]+=delta/weight;
  }
  if(record.empty())return {true,"Clip: no points on the selected side"};doc.mesh.evaluate(record.positions.indices);doc.mesh.updateNormals(record.positions.indices);doc.bvh.refit(doc.mesh,record.positions.indices);doc.undo.push(std::move(record));doc.modified=true;return {true,"Clip: existing points pushed to the profile; topology unchanged"};
 }});
}
}
