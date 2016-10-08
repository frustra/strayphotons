const uint maxLights = 16;

uniform int lightCount;
uniform vec3[maxLights] lightPosition;
uniform vec3[maxLights] lightTint;
uniform vec3[maxLights] lightDirection;
uniform float[maxLights] lightSpotAngleCos;
uniform mat4[maxLights] lightProj;
uniform mat4[maxLights] lightView;
uniform vec2[maxLights] lightClip;
uniform vec4[maxLights] lightMapOffset;

uniform float[maxLights] lightIntensity;
uniform float[maxLights] lightIlluminance;
