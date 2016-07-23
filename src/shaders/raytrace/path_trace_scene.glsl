#version 430

#define M_PI 3.14159265358979323846
#define saturate(x) clamp(x, 0.0, 1.0)

##import raytrace/trace_config

layout(location = 0) uniform float tanHalfFoV;
layout(location = 1) uniform vec3 eyePos;
layout(location = 2) uniform mat3 eyeRot;
layout(location = 3) uniform float seed;
layout(location = 4) uniform float focalDist;
layout(location = 5) uniform float aperture;
layout(location = 6) uniform uvec2 invocationOffset;

##import raytrace/types
##import raytrace/intersection
##import raytrace/util
##import raytrace/lighting

layout(binding = 0, rgba32f) uniform image2D outTex;
layout(binding = 1) writeonly uniform image2D outHighPassTex;

layout(binding = 0, std140) uniform MaterialData {
	int nMaterials;
	Material materials[MAX_MATERIALS];
} materialData;

layout(binding = 1, std140) uniform LightData {
	int nLights;
	Light lights[MAX_LIGHTS];
} lightData;

layout(binding = 2, std140) uniform SceneData {
	int nMeshes;
	Mesh meshes[MAX_MESHES];
} sceneData;

layout(binding = 0, std140) buffer VertexData {
	Vertex vertices[];
} vtxData;

// 430 tightly packs scalars
layout(binding = 1, std430) buffer FaceData {
	uint indexes[];
} faceData;

layout(binding = 2, std140) buffer BVHData {
	BVHNode nodes[];
} bvhData;

layout(binding = 0) uniform sampler2DArray baseColorRoughnessAtlas;
layout(binding = 1) uniform sampler2DArray normalMetalnessAtlas;

layout(local_size_x = 16, local_size_y = 16) in;

void iterateFaces(int offset, int count, vec3 P, vec3 R, inout float dist, inout vec3 uvt, inout mat3 tbn, inout bool collision)
{
	vec4 uvtw;

	for (int fi = offset; fi < offset + count; fi += 3) {
		Vertex v0 = vtxData.vertices[faceData.indexes[fi + 0]];
		Vertex v1 = vtxData.vertices[faceData.indexes[fi + 1]];
		Vertex v2 = vtxData.vertices[faceData.indexes[fi + 2]];

		triIntersect(P, R, v0.pos, v1.pos, v2.pos, uvtw);

		if (all(greaterThanEqual(uvtw, vec4(0.0))) && uvtw.z < dist) {
			vec3 normal = uvtw.x * v1.normal + uvtw.y * v2.normal + uvtw.w * v0.normal;

			if (dot(normal, R) <= 0) {
				dist = uvtw.z;
				uvt.x = uvtw.x * v1.u + uvtw.y * v2.u + uvtw.w * v0.u;
				uvt.y = uvtw.x * v1.v + uvtw.y * v2.v + uvtw.w * v0.v;
				uvt.z = dist;
				collision = true;

				#if NORMAL_MAP
				// calculate tangent and bitangent
				vec3 v01 = v1.pos - v0.pos;
				vec3 v02 = v2.pos - v0.pos;

				vec2 uv0 = vec2(v0.u, v0.v);
				vec2 duv1 = vec2(v1.u, v1.v) - uv0;
				vec2 duv2 = vec2(v2.u, v2.v) - uv0;

				float f = 1.0 / (duv1.x * duv2.y - duv2.x * duv1.y);

				vec3 v01duv2y = duv2.y * v01;
				vec3 v02duv1y = duv1.y * v02;
				vec3 v01duv2x = duv2.x * v01;
				vec3 v02duv1x = duv1.x * v02;

				vec3 tangent = normalize(f * (v01duv2y - v02duv1y));
				vec3 bitangent = normalize(f * (v02duv1x - v01duv2x));

				tbn = mat3(tangent, bitangent, normal);
				#else
				tbn = mat3(vec3(0.0), vec3(0.0), normal);
				#endif
			}
		}
	}
}

bool meshIntersect(vec3 P, vec3 R, Mesh mesh, out vec3 uvt, out mat3 tbn)
{
	float aabbtmin, aabbtmax;
	vec3 invR = 1.0 / R;

	aabbIntersectFast(P, R, invR, mesh.aabb1, mesh.aabb2, aabbtmin, aabbtmax);
	if (aabbtmin > aabbtmax || aabbtmax < 0)
		return false;

	bool collision = false;
	float dist = 1.0/0.0;

#if !USE_BVH
	iterateFaces(mesh.indexOffset, mesh.indexCount, P, R, dist, uvt, tbn, collision);
#else
	int stack[24];
	int stackIdx = 0;
	stack[0] = mesh.bvhRoot;

	while (stackIdx >= 0) {
		int curr = stack[stackIdx--];
		BVHNode node = bvhData.nodes[curr];

		aabbIntersectFast(P, R, invR, node.aabb1, node.aabb2, aabbtmin, aabbtmax);
		if (aabbtmin > aabbtmax || aabbtmax < 0)
			continue;

		if (node.left >= 0) {
			stack[++stackIdx] = node.left;
			stack[++stackIdx] = node.right;
			continue;
		}

		// debug BVH leaf AABBs
		/*dist = aabbtmin;
		uvt = vec3(1.0, 1.0, dist);
		tbn = mat3(1.0);
		return true;*/

		// left = -count, right = offset
		iterateFaces(node.right, -node.left, P, R, dist, uvt, tbn, collision);
	}
#endif

	return collision;
}

void worldIntersect(vec3 P, vec3 R, out float dist, out int matID, out mat3 tbn, out vec3 intersection, out vec3 modelUvt)
{
	float closest = -1.0;
	mat3 modelTbn;
	mat4 finalInvTrans;

	for (int i = 0; i < sceneData.nMeshes; i++) {
		Mesh mesh = sceneData.meshes[i];
		mat4 invtrans = mesh.invtrans;
		vec3 Pt = (invtrans * vec4(P, 1)).xyz;
		vec3 Rt = normalize(mat3(invtrans) * R);

		vec3 uvt;
		mat3 currTbn;

		if (meshIntersect(Pt, Rt, mesh, uvt, currTbn) && uvt.z > 0.001) {
			vec3 modelInt = Pt + uvt.z * Rt;
			vec3 worldInt = (mesh.trans * vec4(modelInt, 1.0)).xyz;
			uvt.z = length(worldInt - P);
			if (closest < 0.0 || uvt.z < closest) {
				closest = uvt.z;
				intersection = worldInt;
				modelTbn = currTbn;
				modelUvt = uvt;
				finalInvTrans = invtrans;
				matID = mesh.materialID;
			}
		}
	}

	mat3 rotMat = mat3(transpose(finalInvTrans));

	#if NORMAL_MAP
	tbn = mat3(normalize(rotMat * modelTbn[0]), normalize(rotMat * modelTbn[1]), normalize(rotMat * modelTbn[2]));
	#else
	tbn[2] = normalize(rotMat * modelTbn[2]);
	#endif
	dist = closest;
}

/*vec3 applyLight(vec3 P, vec3 N, vec3 V, vec2 noise, Material mat)
{
	vec3 accum = vec3(0.0);

	for (int i = 0; i < lightData.nLights; i++) {
		Light light = lightData.lights[i];
		vec3 L = normalize(light.position - P);
		float pdf;
		L = normalize(L + cosineDirection(L, noise, pdf) * 0.04);

		float dist; int matID;
		worldIntersect(P, L, dist, matID);

		if (dist < 0.0) {
			vec3 H = normalize(V + L);
			float LdotN = dot(L, N);
			vec3 R = normalize(2.0 * LdotN * N - L);
			float RdotV = dot(R, V);
			float VdotH = min(max(dot(V, H), 0.0), 1.0);

			//vec3 diffuse = vec3(mat.kd) * max(LdotN, 0) * light.colour;
			//vec3 specular = vec3(mat.ks) * pow(max(RdotV, 0), mat.pwr + 0.1) * light.colour;

			//accum += diffuse + specular;
		}
	}

	return accum;
}*/

vec3 background(vec3 R, vec2 noise)
{
	//return SKY_LIGHT;// * max(dot(R, vec3(0, 1, 0)), 0.0);

	vec3 result = vec3(1.0);

	// This makes a very strange pattern
	float xoff = fract(R.x * 100.0) * 0.2 + noise.x * 0.1;

	// Light background
	//float xoff = noise.x * 0.3 - 0.1;
	float up = max(R.y + xoff, 0.0), down = max(-R.y - xoff, 0.0);
	up = pow(up, 0.5);
	//vec3 result = vec3(up * 0.5, up * 0.75, up * 0.9) + vec3(down * 0.85, down * 0.56, down * 0.4);

	// Dark background
	//float xoff = noise.x * 0.3 - 0.1;
	/*float up = max(R.y + xoff, 0.0), down = max(-R.y - xoff, 0.0);
	up = up * up;
	return vec3(up * 0.48, up * 0.33, up * 0.40);*/

	return SKY_LIGHT * result;
}

vec3 calculateColour(vec3 P, vec3 R, vec4 rng)
{
	vec3 accum = vec3(0.0);
	vec3 accumBRDF = vec3(1.0);
	vec2 noise = vec2(rand2(rng), rand2(rng));

	float dist;
	int matID;
	mat3 tbn;
	vec3 intersection;
	vec3 uvt;

	worldIntersect(P, R, dist, matID, tbn, intersection, uvt);

	if (dist < 0.0) {
		return background(R, noise);
	}

	vec3 normal = tbn[2];
	vec2 uv = uvt.xy;
	vec3 viewDir;
	float pdf;

	//return normal * 0.5 + 0.5;
	//return vec3(dist * 0.1);

	for (int i = 0; i < PATH_LENGTH; i++) {
		noise = vec2(rand2(rng), rand2(rng));

		vec3 baseColor = vec3(0.8), mappedNormal = vec3(0.0, 0.0, 1.0), f0 = vec3(0.04);
		float roughness = 0.8, metalness = 0.0;

		if (matID != -1) {
			Material mat = materialData.materials[matID];
			vec3 bcuvt = vec3(fract(uv) * mat.baseColorRoughnessSize, mat.baseColorRoughnessIdx);
			vec4 bcr = texture(baseColorRoughnessAtlas, bcuvt);
			baseColor = bcr.rgb;
			roughness = bcr.a;
			f0 = mat.f0;

			if (mat.normalMetalnessIdx >= 0) {
				vec3 nmuvt = vec3(fract(uv) * mat.normalMetalnessSize, mat.normalMetalnessIdx);
				vec4 nm = texture(normalMetalnessAtlas, nmuvt);
				#if NORMAL_MAP
				mappedNormal = nm.rgb * 2.0 - 1.0;
				//mappedNormal.z *= 0.7;
				mappedNormal = normalize(mappedNormal);
				#endif
				metalness = nm.a;
			}
		}

		#if NORMAL_MAP
		normal = normalize(tbn * mappedNormal);
		#endif

		vec3 specularColor = mix(f0, baseColor, metalness);
		vec3 diffuseColor = baseColor - baseColor * metalness;

		if (dot(baseColor, vec3(1.0)) == 3.0) {
			accum += accumBRDF * vec3(17.0, 12.0, 4.0) * 100.0;
		}

		viewDir = -R;
		//generateBounce(P, R, pdf, noise, roughness, intersection, normal);
		//return R * 0.5 + 0.5;
		vec3 brdf = performBounce(P, R, normal, intersection, diffuseColor, specularColor, roughness, noise);

		if (dot(R, normal) < 0)
			break;

		//vec3 brdf = evaluateBRDF(baseColor, vec3(0.04), roughness, R, viewDir, normal) / pdf;
		accumBRDF *= brdf * max(dot(normal, R), 0);

		worldIntersect(P, R, dist, matID, tbn, intersection, uvt);

		normal = tbn[2];
		uv = uvt.xy;

		if (dist < 0.0) {
			accum += background(R, noise) * accumBRDF;
			break;
		}

		//if (isnan(dist)) return vec3(1, 0, 0);
		//if (isinf(dist)) return vec3(0, 0, 1);

		//return normal * 0.5 + 0.5;
		//return vec3(uv, 1.0);
		//return vec3(1.0);
		//return vec3(float(matID % 2), float(matID % 3) / 3.0, float(matID % 4) / 4.0);

		//vec3 dlight = applyLight(intersection, normal, -R, noise, mat);
		//accum += dlight;
		//kd = mat.kd;
	}

	if (any(isnan(accum))) return vec3(1, 0, 0);

	return accum;
}

void main()
{
	uvec2 coord = gl_GlobalInvocationID.xy + invocationOffset;
	vec2 size = vec2(imageSize(outTex));
	vec4 previous = imageLoad(outTex, ivec2(coord));

	vec3 accum = vec3(0.0);
	vec2 uv = (vec2(coord) * 2.0 - size) / size.y;

	vec4 rng;
	rng.x = hash(uv.x);
	rng.y = hash(uv.y + rng.x);
	rng.z = hash(rng.x + rng.y);
	rng.w = hash(dot(vec3(1.0), rng.xyz) + seed);
	rand2(rng);

    for (int i = 0; i < SAMPLES; i++) {
		vec2 noise = vec2(rand2(rng), rand2(rng)) * 2.0 - vec2(1.0);
		vec2 ndc = 2.0 * (vec2(coord) + noise * 0.75) / size - vec2(1.0);

		vec3 eyeRayDir = normalize(vec3(
			ndc.x * tanHalfFoV * size.x / size.y,
			ndc.y * tanHalfFoV,
			-1.0
		));

		vec3 focalPos = eyePos + eyeRayDir * focalDist;

		float piNoise = M_PI * noise.x;
		vec2 circularNoise = noise.y * vec2(cos(piNoise), sin(piNoise));
		vec3 rayPos = eyePos + vec3(circularNoise * aperture / size, 0.0);
		vec3 rayDir = eyeRot * normalize(focalPos - rayPos);

		accum += calculateColour(rayPos, rayDir, rng);
	}

	double prevIt = previous.w;
	double nextIt = prevIt + 1.0;

	dvec3 prev = previous.rgb;
	dvec3 curr = accum;
	curr /= double(SAMPLES);
	prev *= prevIt;
	prev += curr;
	prev /= nextIt;

	vec3 colour = vec3(prev);

	if (!any(isnan(colour)) && !any(isinf(colour))) {
		imageStore(outTex, ivec2(coord), vec4(colour, nextIt));
	}

	// ITU-R Recommendation BT. 601
	//float luminance = dot(colour, vec3(0.299, 0.587, 0.114));
	//imageStore(outHighPassTex, ivec2(coord), vec4(colour * smoothstep(0.7, 1.0, luminance) - 0.7, 1));
}
