struct Light {
	vec3 colour;
	vec3 position;
	vec3 falloff;
};

struct Material {
	vec2 baseColorRoughnessSize;
	vec2 normalMetalnessSize;
	vec3 f0;
	float padding;
	int baseColorRoughnessIdx;
	int normalMetalnessIdx;
};

struct Mesh {
	int materialID;
	int indexOffset;
	int indexCount;
	int bvhRoot;
	mat4 trans;
	mat4 invtrans;
	vec3 aabb1;
	vec3 aabb2;
};

struct Vertex {
	vec3 pos; float u;
	vec3 normal; float v;
};

struct BVHNode {
	vec3 aabb1;
	int left;
	vec3 aabb2;
	int right;
};
