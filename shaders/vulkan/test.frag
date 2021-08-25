#version 450

layout(location = 0) in vec3 viewPos;
layout(location = 1) in vec3 viewNormal;
layout(location = 2) in vec3 color;
layout(location = 0) out vec4 outColor;

const vec3 diffuseColor = vec3(1.0, 0.7, 0.7);
const vec3 specColor = vec3(1.0, 1.0, 1.0);

void main() {
    vec3 normal = normalize(viewNormal);

    vec3 lightDir = -viewPos; // light is at view origin
    float distance = length(lightDir);
    distance = distance * distance;
    lightDir = normalize(lightDir);

    float diffusePower = max(dot(lightDir, normal), 0.0);
    vec3 viewDir = normalize(-viewPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float specAngle = max(dot(halfDir, normal), 0.0);
    float specPower = pow(specAngle, 16);

    vec3 colorLinear = diffuseColor * diffusePower / distance + specColor * specPower / distance;
    outColor = vec4(colorLinear * 10, 1.0);
}
