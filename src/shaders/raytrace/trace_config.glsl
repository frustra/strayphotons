#define USE_BVH 0
#define NORMAL_MAP 0

//const int SAMPLES = 2; // M paths per pixel per invocation
//const int PATH_LENGTH = 2; // K secondary rays per path
const int SAMPLES = 8; // M paths per pixel per invocation
const int PATH_LENGTH = 5; // K secondary rays per path

const int MAX_LIGHTS = 32;
const int MAX_MATERIALS = 32;
const int MAX_MESHES = 64;

const vec3 SKY_LIGHT = vec3(000.0); // illuminance in lux
