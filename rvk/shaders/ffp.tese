#version 450
// Phong tessellation (Boubekeur & Alexa 2008): a point of the triangle is projected onto the tangent plane of each
// corner (its averaged normal), the projections blended by the same barycentric weights - the surface bulges like the
// smooth normals say, rounding low-polygon silhouettes. D.tess.y: how much (0 = flat); each corner's pull also
// weighted by how smooth the model is there (t_vSmoothN's length: 0 at hard edges, which stay sharp). Then what the vertex shader
// computes from the world position: the clip position (with the TAA jitter), the motion vector inputs, fog distance.
// The native ground (D.terrain.y set): only its steep parts rounded, the shape fading out towards D.tess.z (no step
// where the splitting ends), and its cliffs' rock stands out of them by up to D.tess.w units (terrain_rock.glsl). Both of its passes run this alike:
// the lighting pass's equal depth test meets the base pass's.
#extension GL_EXT_nonuniform_qualifier : require
#include "constants.glsl"
#include "ffp_varyings.glsl"
#include "terrain_rock.glsl"
#include "rock_shape.glsl"

invariant gl_Position;

// Lower-left domain origin (pipeline): counter-clockwise here keeps the patch's winding. Fractional spacing: as a level
// grows, new points slide out of their neighbours instead of every point jumping at each whole step - the ground's
// levels (by distance) and the characters' (by closeness) change as the camera moves, and whole steps showed as the
// shape popping (user, 2026-10-19).
layout(triangles, fractional_odd_spacing, ccw) in;

#define IN(type, name, loc) layout(location = loc) in type t_##name[];
#define OUT(type, name, loc) layout(location = loc) out type name;
#define LERP(type, name, loc) name = gl_TessCoord.x * t_##name[0] + gl_TessCoord.y * t_##name[1] + gl_TessCoord.z * t_##name[2];
FFP_VARYINGS(IN)
FFP_VARYINGS(OUT)
// The draw's record index: one patch value from the control stage, passed flat to the fragment shader.
layout(location = 15) patch in uint tRecord;
layout(location = 16) patch in uint tRockEdges;
layout(location = 15) flat out uint vRecord;

void main()
{
    gRecord = tRecord;
    vRecord = tRecord;
    FFP_VARYINGS(LERP)
    vec3 b = gl_TessCoord;
    vec3 p = vPosW, pull = vec3(0.0);
    for (int i = 0; i < 3; ++i) {
        float weight = length(t_vSmoothN[i]);
        if (weight <= 0.0) continue;
        vec3 n = t_vSmoothN[i] / weight;
        pull += b[i] * weight * (-dot(p - t_vPosW[i], n) * n);   // towards corner i's tangent plane
    }
    vec3 posW;
    vec3 previousDelta=vec3(0.0);
    if((D.mat.z&MAT_ROCK_SHAPE)!=0u){
        vec3 a=t_vPosW[0],c=t_vPosW[2],q=t_vPosW[1];
        vec3 na=t_vNormalW[0].xyz,nb=t_vNormalW[1].xyz,nc=t_vNormalW[2].xyz;
        posW=RockPoint(b,a,q,c,tRockEdges,C.eyePos.xyz,na,nb,nc);
        // Finite differences use centimetres in world space: fixed tiny
        // barycentric steps become noisy at AO's kilometre-scale coordinates.
        float epsilon=clamp(.025/max(max(length(q-a),length(c-a)),.001),.0001,.05);
        vec3 u=RockPoint(b+vec3(epsilon,-epsilon,0.0),a,q,c,tRockEdges,C.eyePos.xyz,na,nb,nc)-posW;
        vec3 v=RockPoint(b+vec3(epsilon,0.0,-epsilon),a,q,c,tRockEdges,C.eyePos.xyz,na,nb,nc)-posW;
        vec3 previousEye=-(transpose(mat3(FL.prevView))*FL.prevView[3].xyz);
        previousDelta=RockPoint(b,a,q,c,tRockEdges,previousEye,na,nb,nc)-p;
        vec3 faceNormal=cross(q-a,c-a),bent=cross(u,v);
        if(dot(faceNormal,faceNormal)>1e-14 && dot(bent,bent)>1e-20){
            faceNormal=normalize(faceNormal);bent=normalize(bent);
            vec3 reference=dot(vNormalW.xyz,vNormalW.xyz)>1e-8?normalize(vNormalW.xyz):faceNormal;
            if(dot(faceNormal,reference)<0.0){faceNormal=-faceNormal;bent=-bent;}
            vNormalW=vec4(normalize(reference+bent-faceNormal),1.0);
        }
    }else if (floatBitsToUint(D.terrain.y) != 0u) {
        const float fade = 1.0 - smoothstep(0.5 * D.tess.z, D.tess.z, distance(p, C.eyePos.xyz));
        // A crease (a cliff's foot or rim: its ends' normals far apart) isn't rounded - Phong's shape overshoots there
        // into lips and spikes; gentle ground is. Per edge, from its ends alone (both triangles beside it agree), blended
        // inside the triangle: on an edge only that edge's weight counts.
        vec3 n0 = normalize(t_vSmoothN[0]), n1 = normalize(t_vSmoothN[1]), n2 = normalize(t_vSmoothN[2]);
        const vec3 e = vec3(b.y * b.z, b.z * b.x, b.x * b.y);   // (edges opposite corners 0, 1, 2)
        const vec3 we = smoothstep(vec3(0.6), vec3(0.9), vec3(dot(n1, n2), dot(n2, n0), dot(n0, n1)));
        const float crease = dot(e, we) / max(e.x + e.y + e.z, 1e-12);
        // Only steep ground is rounded, each corner's pull by its own slope (from ~37 degrees, all of it by ~53): where
        // one can stand the ground stays the game's flat triangles - characters stand on those, and rounded, a
        // hillside sank under their feet. (Per corner: an edge's shape still comes from its two ends alone.)
        vec3 steep = smoothstep(vec3(0.8), vec3(0.6), abs(vec3(n0.y, n1.y, n2.y)));
        vec3 pullSteep = vec3(0.0);
        for (int i = 0; i < 3; ++i) {
            const float weight = length(t_vSmoothN[i]);
            if (weight <= 0.0) continue;
            const vec3 n = t_vSmoothN[i] / weight;
            pullSteep += b[i] * weight * steep[i] * (-dot(p - t_vPosW[i], n) * n);
        }
        posW = p + (D.tess.y * fade * crease) * pullSteep;
        if (fade > 0.0 && D.tess.w > 0.0) posW += TerrainRockRelief(p, vSmoothN, D.tess.w * fade);
    } else {
        posW = p + D.tess.y * pull;
    }
    vec3 delta = posW - p;
    if((D.mat.z&MAT_ROCK_SHAPE)==0u)previousDelta=delta;
    // vPosW (lighting, shadow lookups) stays on the flat triangles: the shadow maps hold the untessellated mesh, and a
    // rounded point pulled inside it would find itself shadowed by its own body. The light barely differs.
    vPosW = p;
    vec4 pv = C.view * vec4(posW, 1.0);
    gl_Position = C.proj * pv;
    vClip = gl_Position;                         // motion vectors: without the jitter
    if ((C.flags.x & F_HDR) != 0u)
        gl_Position.xy += FL.taa.xy * gl_Position.w;
    vPrevClip += FL.prevProj * (FL.prevView * vec4(previousDelta, 0.0));
    vFogDist = (C.flags.x & F_RANGEFOG) != 0u ? length(pv.xyz) : abs(pv.z);
}
