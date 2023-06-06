/*
 * The following code is sourced from: http://jcgt.org/published/0003/02/01/paper.pdf
 *
 * (c) 2014 Cigolle, Donow, Evangelakos, Mara, McGuire, Meyer (the Authors).
 * The Authors provide this document (the Work) under the Creative Commons CC BY-ND
 * 3.0 license available online at http://creativecommons.org/licenses/by-nd/3.0/
 */

#ifndef NORMAL_ENCODE_GLSL_INCLUDED
#define NORMAL_ENCODE_GLSL_INCLUDED

vec2 SignNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// Encodes a normalized vector into a vec2 using octahedron mapping
vec2 EncodeNormal(vec3 v) {
    // Project the sphere onto the octahedron, and then onto the xy plane
    vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    // Reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * SignNotZero(p)) : p;
}

// Decodes a normal vector from EncodeNormal
vec3 DecodeNormal(vec2 e) {
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * SignNotZero(v.xy);
    return normalize(v);
}

#endif
