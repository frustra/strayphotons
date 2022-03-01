#include "../lib/types_common.glsl"
#include "../lib/util.glsl"

layout(binding = 0) uniform sampler2DArray gBuffer0;
layout(binding = 1) uniform sampler2DArray gBuffer1;
layout(binding = 2) uniform sampler2DArray gBuffer2;
layout(binding = 3) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform sampler2D lightingGels[MAX_LIGHT_GELS];

INCLUDE_LAYOUT(binding = 11)
#include "lib/light_data_uniform.glsl"

INCLUDE_LAYOUT(binding = 10)
#include "lib/view_states_uniform.glsl"

INCLUDE_LAYOUT(binding = 9)
#include "lib/exposure_state.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outFragColor;

#include "../lib/shading.glsl"

layout(constant_id = 0) const uint MODE = 1;

void main() {
    ViewState view = views[gl_ViewID_OVR];

    vec4 gb0 = texture(gBuffer0, vec3(inTexCoord, gl_ViewID_OVR));
    vec4 gb1 = texture(gBuffer1, vec3(inTexCoord, gl_ViewID_OVR));
    vec4 gb2 = texture(gBuffer2, vec3(inTexCoord, gl_ViewID_OVR));

    vec3 baseColor = gb0.rgb;
    float roughness = gb0.a;
    vec3 viewNormal = DecodeNormal(gb1.rg);
    vec3 flatViewNormal = viewNormal; // DecodeNormal(gb1.ba);
    vec3 emissive = vec3(0); // gb2.a * baseColor;
    float metalness = gb2.a;

    // Determine coordinates of fragment.
    vec3 fragPosition = ScreenPosToViewPos(inTexCoord, 0, view.invProjMat);
    vec3 viewPosition = gb2.rgb;
    vec3 worldPosition = (view.invViewMat * vec4(viewPosition, 1.0)).xyz;
    vec3 worldFragPosition = (view.invViewMat * vec4(fragPosition, 1.0)).xyz;

    vec3 worldNormal = mat3(view.invViewMat) * viewNormal;
    vec3 flatWorldNormal = mat3(view.invViewMat) * flatViewNormal;

    vec3 rayDir = normalize(worldPosition - worldFragPosition);
    vec3 rayReflectDir = reflect(rayDir, worldNormal);

    vec3 indirectSpecular = vec3(0);
    vec3 indirectDiffuse = vec3(0);

    vec3 directDiffuseColor = baseColor - baseColor * metalness;
    vec3 directLight =
        DirectShading(worldPosition, -rayDir, baseColor, worldNormal, flatWorldNormal, roughness, metalness);

    vec3 indirectLight = indirectDiffuse * directDiffuseColor + indirectSpecular;
    vec3 totalLight = emissive + directLight + indirectLight;

    if (MODE == 0) { // Direct only
        outFragColor = vec4(directLight, 1.0);
    } else if (MODE == 2) { // Indirect lighting
        outFragColor = vec4(indirectLight, 1.0);
    } else if (MODE == 3) { // diffuse
        outFragColor = vec4(indirectDiffuse, 1.0);
    } else if (MODE == 4) { // specular
        outFragColor = vec4(indirectSpecular, 1.0);
    } else { // Full lighting
        outFragColor = vec4(totalLight, 1.0);
    }

    outFragColor.rgb *= exposure;
}
