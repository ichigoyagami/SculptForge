// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/PlaneTools.h"
#include "core/Commands.h"
#include "core/Document.h"
#include "core/Micro.h"
#include "core/Multires.h"
#include "core/ScreenSelect.h"
#include "core/Sculpt.h"
#include "../../third_party/earcut/earcut.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace sf {
namespace {
uint64_t edge(uint32_t a,uint32_t b){return (uint64_t(std::min(a,b))<<32)|std::max(a,b);}
struct Corner { uint32_t v;glm::vec2 uv; };
bool vectorArg(const Args& a,const char* key,glm::vec3& v){
    std::string s=argString(a,key,"");std::replace(s.begin(),s.end(),',',' ');std::istringstream in(s);
    return bool(in>>v.x>>v.y>>v.z) && std::isfinite(v.x+v.y+v.z);
}
}

Mesh knifePlane(const Mesh& src,glm::vec3 normal,float offset,std::string& error,bool touchedPartsOnly){
    Mesh out;
    const float length=glm::length(normal);
    if(length<1e-8f || !std::isfinite(length+offset)){error="Invalid cutting plane";return out;}
    normal/=length;offset/=length;
    const float scale=std::max(glm::length(src.bounds().extent()),0.001f),eps=scale*1e-6f;
    const size_t nv=src.vertexCount();
    std::vector<float> distance(nv);
    bool positive=false,negative=false;
    for(size_t i=0;i<nv;++i){float d=glm::dot(normal,src.positions[i])-offset;
        distance[i]=std::abs(d)<eps?0:d;positive|=distance[i]>0;negative|=distance[i]<=0;}
    if(touchedPartsOnly){
        std::vector<uint8_t> seen(nv,0);
        for(uint32_t start=0;start<nv;++start)if(!seen[start]){
            std::vector<uint32_t> part{start};seen[start]=1;bool above=false,below=false;
            for(size_t j=0;j<part.size();++j){auto v=part[j];above|=distance[v]>0;below|=distance[v]<0;
                for(auto u:src.neighbors(v))if(!seen[u]){seen[u]=1;part.push_back(u);}}
            if(!above || !below)for(auto v:part)distance[v]=-eps;
        }
        positive=false;negative=false;for(float d:distance){positive|=d>0;negative|=d<=0;}
    }
    if(!positive)return src;
    if(!negative){error="Knife would remove the entire SubTool";return out;}
    std::vector<uint32_t> kept(nv,Mesh::kNoQuad),triMap(src.triangleCount(),Mesh::kNoQuad);
    std::unordered_map<uint64_t,uint32_t> cuts;
    auto vertex=[&](uint32_t a,uint32_t b,float t){
        uint32_t id=uint32_t(out.vertexCount());out.positions.push_back(glm::mix(src.positions[a],src.positions[b],t));
        if(src.hasMask())out.mask.push_back(glm::mix(src.mask[a],src.mask[b],t));
        if(src.hasColors())out.colors.push_back(glm::mix(src.colors[a],src.colors[b],t));
        return id;
    };
    auto original=[&](uint32_t a){if(kept[a]==Mesh::kNoQuad)kept[a]=vertex(a,a,0);return kept[a];};
    auto intersection=[&](uint32_t a,uint32_t b){
        if(distance[a]==0)return original(a);
        if(distance[b]==0)return original(b);
        const auto key=edge(a,b);auto it=cuts.find(key);if(it!=cuts.end())return it->second;
        const float t=distance[a]/(distance[a]-distance[b]);uint32_t v=vertex(a,b,t);
        out.positions[v]-=normal*(glm::dot(normal,out.positions[v])-offset);cuts.emplace(key,v);return v;
    };
    auto triangle=[&](Corner a,Corner b,Corner c,uint32_t group,uint8_t hidden){
        if(a.v==b.v || b.v==c.v || c.v==a.v)return false;
        if(glm::length(glm::cross(out.positions[b.v]-out.positions[a.v],out.positions[c.v]-out.positions[a.v]))<eps*eps)return false;
        out.triangles.push_back({a.v,b.v,c.v});out.polygroups.push_back(group);out.hidden.push_back(hidden);
        if(src.hasUvs()){out.uvs.push_back(a.uv);out.uvs.push_back(b.uv);out.uvs.push_back(c.uv);}return true;
    };
    uint32_t capGroup=1;
    for(uint32_t ti=0;ti<src.triangleCount();++ti){
        capGroup=std::max(capGroup,src.polygroupAt(ti)+1);
        auto face=src.triangles[ti];std::vector<Corner> polygon;
        for(int j=0;j<3;++j){uint32_t a=face[j],b=face[(j+1)%3];
            glm::vec2 ua=src.hasUvs()?src.uvs[3*ti+j]:glm::vec2(0),ub=src.hasUvs()?src.uvs[3*ti+(j+1)%3]:glm::vec2(0);
            bool ia=distance[a]<=0,ib=distance[b]<=0;
            if(ia)polygon.push_back({original(a),ua});
            if(ia!=ib){float t=distance[a]/(distance[a]-distance[b]);polygon.push_back({intersection(a,b),glm::mix(ua,ub,t)});}
        }
        if(polygon.size()<3)continue;
        uint32_t first=uint32_t(out.triangleCount());
        for(size_t j=1;j+1<polygon.size();++j)triangle(polygon[0],polygon[j],polygon[j+1],src.polygroupAt(ti),src.triangleHidden(ti));
        if(polygon.size()==4 && out.triangleCount()==first+2)out.pairQuad(first,first+1);
        if(distance[face.x]<=0 && distance[face.y]<=0 && distance[face.z]<=0)triMap[ti]=first;
    }
    for(uint32_t t=0;t<src.triangleCount();++t){uint32_t u=src.quadPartnerAt(t);
        if(u!=Mesh::kNoQuad && u>t && triMap[t]!=Mesh::kNoQuad && triMap[u]!=Mesh::kNoQuad)out.pairQuad(triMap[t],triMap[u]);}
    struct Edge {uint32_t a,b;int count=0;};std::map<uint64_t,Edge> edges;
    for(auto t:out.triangles)for(int j=0;j<3;++j){uint32_t a=t[j],b=t[(j+1)%3];auto& e=edges[edge(a,b)];e.a=a;e.b=b;++e.count;}
    std::map<uint32_t,std::vector<uint32_t>> adjacency;
    std::set<uint64_t> unused;
    for(auto [key,e]:edges)if(e.count==1 && std::abs(glm::dot(normal,out.positions[e.a])-offset)<eps*2 &&
                                               std::abs(glm::dot(normal,out.positions[e.b])-offset)<eps*2){
        adjacency[e.a].push_back(e.b);adjacency[e.b].push_back(e.a);unused.insert(key);}
    for(auto& [v,ring]:adjacency)if(ring.size()!=2){error="Knife needs closed, manifold cut contours; repair the mesh first";return {};}
    std::vector<std::vector<uint32_t>> loops;
    while(!unused.empty()){
        uint32_t start=uint32_t(*unused.begin()>>32),at=start,previous=Mesh::kNoQuad;std::vector<uint32_t> loop;
        do{loop.push_back(at);const auto& ring=adjacency.at(at);uint32_t next=ring[0]==previous?ring[1]:ring[0];
            if(!unused.erase(edge(at,next))){error="Knife contour is self-intersecting";return {};}
            previous=at;at=next;
        }while(at!=start && loop.size()<=adjacency.size());
        if(at!=start || loop.size()<3){error="Knife cut contour is open";return {};}
        loops.push_back(std::move(loop));
    }
    glm::vec3 u=glm::normalize(glm::cross(std::abs(normal.x)<0.8f?glm::vec3(1,0,0):glm::vec3(0,1,0),normal));
    glm::vec3 v=glm::cross(normal,u);
    std::vector<std::vector<glm::vec2>> projected(loops.size());std::vector<double> areas(loops.size());
    for(size_t i=0;i<loops.size();++i){for(auto p:loops[i])projected[i].push_back({glm::dot(out.positions[p],u),glm::dot(out.positions[p],v)});
        for(size_t j=0;j<loops[i].size();++j){auto a=projected[i][j],b=projected[i][(j+1)%loops[i].size()];areas[i]+=double(a.x)*b.y-double(a.y)*b.x;}areas[i]=std::abs(areas[i]);}
    std::vector<int> parent(loops.size(),-1),depth(loops.size(),0);
    for(size_t i=0;i<loops.size();++i)for(size_t j=0;j<loops.size();++j)
        if(areas[j]>areas[i] && ScreenPolygon(projected[j]).contains(projected[i][0]) && (parent[i]<0 || areas[j]<areas[parent[i]]))parent[i]=int(j);
    for(size_t i=0;i<loops.size();++i)for(int p=parent[i];p>=0;p=parent[size_t(p)])++depth[i];
    std::vector<uint32_t> boundary;for(auto& [id,ring]:adjacency)boundary.push_back(id);
    for(size_t i=0;i<loops.size();++i)if(depth[i]%2==0){
        std::vector<std::vector<std::array<double,2>>> polygon;std::vector<uint32_t> ids;
        auto ring=[&](size_t k){
            auto clean=loops[k];bool changed=true;
            while(changed && clean.size()>3){changed=false;
                for(size_t j=0;j<clean.size();++j){auto a=out.positions[clean[(j+clean.size()-1)%clean.size()]],b=out.positions[clean[j]],c=out.positions[clean[(j+1)%clean.size()]];
                    auto ac=c-a;float l=glm::length(ac);
                    if(l>eps && glm::length(glm::cross(b-a,ac))/l<eps*.1f && glm::dot(b-a,b-c)<=0){clean.erase(clean.begin()+j);changed=true;break;}}
            }
            polygon.emplace_back();for(auto id:clean){auto p=out.positions[id];polygon.back().push_back({glm::dot(p,u),glm::dot(p,v)});ids.push_back(id);}
        };
        ring(i);for(size_t j=0;j<loops.size();++j)if(parent[j]==int(i) && depth[j]%2)ring(j);
        std::vector<uint32_t> capBoundary=loops[i];
        for(size_t j=0;j<loops.size();++j)if(parent[j]==int(i) && depth[j]%2)capBoundary.insert(capBoundary.end(),loops[j].begin(),loops[j].end());
        auto caps=mapbox::earcut<uint32_t>(polygon);if(caps.empty()){error="Could not triangulate Knife cap";return {};}
        std::vector<glm::uvec3> pending;for(size_t j=0;j<caps.size();j+=3)pending.push_back({ids[caps[j]],ids[caps[j+1]],ids[caps[j+2]]});
        // Only split cap boundary edges, and only at vertices absent from the
        // triangulation. Splitting interior diagonals can overlap neighboring faces.
        std::set<uint32_t> used;for(auto t:pending)for(int e=0;e<3;++e)used.insert(t[e]);
        for(auto q:capBoundary)if(!used.count(q)){
            std::map<uint64_t,int> count;for(auto t:pending)for(int e=0;e<3;++e)++count[edge(t[e],t[(e+1)%3])];
            bool found=false;
            for(size_t j=0;j<pending.size() && !found;++j)for(int e=0;e<3;++e){auto t=pending[j];auto a=t[e],b=t[(e+1)%3],c=t[(e+2)%3];
                if(count[edge(a,b)]!=1)continue;
                auto ab=out.positions[b]-out.positions[a],aq=out.positions[q]-out.positions[a];float l2=glm::dot(ab,ab);
                if(l2<eps*eps)continue;
                float f=glm::dot(aq,ab)/l2;
                if(f>0 && f<1 && glm::length(aq-ab*f)<eps*.25f){pending[j]={a,q,c};pending.push_back({q,b,c});used.insert(q);found=true;break;}
            }
            if(!found){error="Could not weld Knife cap boundary";return {};}
        }
        for(auto t:pending){
            if(glm::dot(glm::cross(out.positions[t.y]-out.positions[t.x],out.positions[t.z]-out.positions[t.x]),normal)<0)std::swap(t.y,t.z);
            auto corner=[&](uint32_t q){return Corner{q,{glm::dot(out.positions[q],u)/scale,glm::dot(out.positions[q],v)/scale}};};
            triangle(corner(t.x),corner(t.y),corner(t.z),capGroup,0);
        }
    }
    std::map<uint64_t,uint32_t> capEdges;
    for(uint32_t t=0;t<out.triangleCount();++t)if(out.polygroupAt(t)==capGroup){
        auto face=out.triangles[t];
        for(int e=0;e<3;++e){auto key=edge(face[e],face[(e+1)%3]);auto it=capEdges.find(key);
            if(it==capEdges.end()){capEdges[key]=t;continue;}auto other=it->second;
            if(out.quadPartnerAt(t)!=Mesh::kNoQuad || out.quadPartnerAt(other)!=Mesh::kNoQuad)continue;
            int diag=out.quadDiagonal(t,other);if(diag<0)continue;
            auto a=out.triangles[t],b=out.triangles[other];uint32_t far=b.x;
            for(int k=0;k<3;++k)if(b[k]!=a[diag] && b[k]!=a[(diag+1)%3])far=b[k];
            glm::uvec4 q(a[(diag+1)%3],a[(diag+2)%3],a[diag],far);bool convex=true;
            for(int k=0;k<4;++k)convex &= glm::dot(glm::cross(out.positions[q[(k+1)%4]]-out.positions[q[k]],out.positions[q[(k+2)%4]]-out.positions[q[(k+1)%4]]),normal)>eps*eps;
            if(convex)out.pairQuad(t,other);
        }
    }
    out.texture=src.texture;out.paintToTexture=src.paintToTexture;
    if(out.hasQuads())out.quadPartners.resize(out.triangleCount(),Mesh::kNoQuad);
    if (std::none_of(out.hidden.begin(),out.hidden.end(),[](uint8_t h){return h!=0;})) out.hidden.clear();
    out.rebuildTopology();out.recomputeAllNormals();return out;
}

void registerPlaneTools(CommandRegistry& r){
    using T=ParamSpec::Type;
    auto add=[&](const char* name,bool knife){r.add({name,knife?"Knife: remove one side of a plane and cap the cut.":"Clip: push vertices onto a plane without changing topology; masks apply.",
        {{"normal",T::String,"Plane normal x,y,z",true},{"offset",T::Number,"Plane offset dot(normal,p)",false},{"symmetry",T::String,"Optional mirrored plane axes xyz",false}},
        [knife](Document& d,const Args& a)->CommandResult{
            glm::vec3 n;if(!vectorArg(a,"normal",n) || glm::length(n)<1e-8f)return {false,"Invalid cutting normal"};
            float off=float(argNumber(a,"offset",0)),len=glm::length(n);n/=len;off/=len;
            if(hasMicro(d.mesh) || subdivLevelCount(d.mesh)>0)return {false,"Bake Micro or remove subdivision levels before using the plane tools"};
            std::string axes=argString(a,"symmetry","");Symmetry s{axes.find('x')!=std::string::npos,axes.find('y')!=std::string::npos,axes.find('z')!=std::string::npos};
            std::vector<glm::vec3> normals;for(auto m:Sculptor::mirrors(s)){auto q=n*m;bool duplicate=false;for(auto p:normals)duplicate|=glm::length(p-q)<1e-6f;if(!duplicate)normals.push_back(q);}
            if(knife){
                if(d.mesh.hasHidden())return {false,"Show all faces before Knife"};
                if(std::any_of(d.mesh.mask.begin(),d.mesh.mask.end(),[](float v){return v>0;}))return {false,"Clear the mask before Knife; Clip supports masked cuts"};
                Mesh out=d.mesh;std::string error;
                for(auto normal:normals){out=knifePlane(out,normal,off,error,true);if(out.triangleCount()==0)return {false,error};}
                if(out.positions==d.mesh.positions && out.triangles==d.mesh.triangles)return {true,"Knife: no intersected component"};
                d.replaceMeshUndoable(std::move(out),"Knife cut");return {true,"Knife cut: geometry removed and cut capped"};
            }
            int layer=d.activeLayer;if(layer>=int(d.mesh.layers.size()))layer=-1;float w=d.mesh.layerTargetWeight(layer);
            if(std::abs(w)<1e-3f)return {false,"Raise the active layer weight before Clip"};
            UndoRecord record;record.label="Clip cut";record.layer=layer;auto& target=d.mesh.layerTarget(layer);
            for(uint32_t v=0;v<d.mesh.vertexCount();++v){glm::vec3 p=d.mesh.positions[v],q=p;
                for(auto normal:normals){float dist=glm::dot(normal,q)-off;if(dist>0)q-=normal*dist;}
                glm::vec3 delta=(q-p)*vertexInfluence(d.mesh,v);if(glm::dot(delta,delta)<1e-20f)continue;
                record.positions.add(v,target[v]);target[v]+=delta/w;
            }
            if(record.empty())return {true,"Clip: nothing on the shaded side"};
            d.mesh.evaluate(record.positions.indices);d.mesh.updateNormals(record.positions.indices);d.bvh.refit(d.mesh,record.positions.indices);
            d.undo.push(std::move(record));d.modified=true;return {true,"Clip: vertices pushed to the cutting plane"};
        }});};add("knife_plane",true);add("clip_plane",false);registerProfileTools(r);
}
}
