#ifndef SPATIAL_UTIL_GLSL_INCLUDED
#define SPATIAL_UTIL_GLSL_INCLUDED

const float shadowBiasDistance = 0.04;

// Projects v onto a normalized vector u.
vec3 ProjectVec3(vec3 v, vec3 u) {
    return u * dot(v, u);
}

// Returns a ray from the camera to the far plane.
vec3 FarPlaneRay(vec2 viewSpacePosition, float tanHalfFov, float aspectRatio) {
    return vec3(viewSpacePosition.x * tanHalfFov * aspectRatio, viewSpacePosition.y * tanHalfFov, 1.0);
}

// Returns the homogeneous view-space position of clip-space position.
vec4 ClipPosToHViewPos(vec3 clipPos, mat4 invProj) {
    return invProj * vec4(clipPos, 1.0);
}

// Returns the view-space position of clip-space position.
vec3 ClipPosToViewPos(vec3 clipPos, mat4 invProj) {
    vec4 hpos = ClipPosToHViewPos(clipPos, invProj);
    return hpos.xyz / hpos.w; // Inverse perspective divide.
}

// Returns the homogenous view-space position of a screen-space texcoord and depth.
vec4 ScreenPosToHViewPos(vec2 texCoord, float depth, mat4 invProj) {
    vec3 clip = vec3(texCoord, depth) * 2.0 - 1.0;
    return ClipPosToHViewPos(clip, invProj);
}

// Returns the view-space position of a screen-space texcoord and depth.
vec3 ScreenPosToViewPos(vec2 texCoord, float depth, mat4 invProj) {
    vec3 clip = vec3(texCoord, depth) * 2.0 - 1.0;
    return ClipPosToViewPos(clip, invProj);
}

// Returns the screen-space texcoord of a view-space position.
vec3 ViewPosToScreenPos(vec3 viewPos, mat4 projMat) {
    vec4 clip = projMat * vec4(viewPos, 1.0);
    return clip.xyz / clip.w * vec3(0.5) + vec3(0.5);
}

// Gradient noise in [0, 1] as in [Jimenez 2014]
float InterleavedGradientNoise(vec2 position) {
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position, magic.xy)));
}

// Returns the maximum size of a voxel along an axis
float AxisVoxelWidth(vec3 axis) {
    vec3 absAxis = abs(axis);
    return 1.0 / max(absAxis.x, max(absAxis.y, absAxis.z));
}

// Some nice sample coordinates around a spiral.
const vec2[8] SpiralOffsets = vec2[](vec2(-0.7071, 0.7071),
    vec2(-0.0000, -0.8750),
    vec2(0.5303, 0.5303),
    vec2(-0.6250, -0.0000),
    vec2(0.3536, -0.3536),
    vec2(-0.0000, 0.3750),
    vec2(-0.1768, -0.1768),
    vec2(0.1250, 0.0000));

// const int DiskKernelRadius = 2;
// const float[5][5] DiskKernel = float[][](
//     float[]( 0.0   , 0.5/17, 1.0/17, 0.5/17, 0.0    ),
//     float[]( 0.5/17, 1.0/17, 1.0/17, 1.0/17, 0.5/17 ),
//     float[]( 1.0/17, 1.0/17, 1.0/17, 1.0/17, 1.0/17 ),
//     float[]( 0.5/17, 1.0/17, 1.0/17, 1.0/17, 0.5/17 ),
//     float[]( 0.0   , 0.5/17, 1.0/17, 0.5/17, 0.0    )
// );

const int DiskKernelRadius = 1;
const float[3][3] DiskKernel = float[][](float[](0.5 / 7, 1.0 / 7, 0.5 / 7),
    float[](1.0 / 7, 1.0 / 7, 1.0 / 7),
    float[](0.5 / 7, 1.0 / 7, 0.5 / 7));

// Returns vector with angles phi, tht in the hemisphere defined by the input normal.
vec3 OrientByNormal(float phi, float tht, vec3 normal) {
    float sintht = sin(tht);
    float xs = sintht * cos(phi);
    float ys = cos(tht);
    float zs = sintht * sin(phi);

    vec3 up = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tangent1 = normalize(up - normal * dot(up, normal));
    vec3 tangent2 = normalize(cross(tangent1, normal));
    return normalize(xs * tangent1 + ys * normal + zs * tangent2);
}

// Returns a unit vector that is orthogonal to the input.
vec3 OrthogonalVec3(vec3 normal) {
    vec3 up = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    return normalize(cross(normal, up));
}

// Produce linear depth in (0, 1) using view space coordinates
float LinearDepth(vec3 viewPos, vec2 clip) {
    return (length(viewPos) - clip.x) / (clip.y - clip.x);
}

// Produce linear depth in (0, 1) using view space coordinates
float LinearDepthBias(vec3 viewPos, mat4 projMat, float bias) {
    return ViewPosToScreenPos(viewPos + vec3(0, 0, bias), projMat).z;
}

vec2 SignNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// Encodes a normalized vector into a vec2 using octahedron mapping
// http://jcgt.org/published/0003/02/01/paper.pdf
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
