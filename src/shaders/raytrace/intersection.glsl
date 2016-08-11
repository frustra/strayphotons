void aabbIntersectFast(vec3 P, vec3 R, vec3 invR, vec3 aabb1, vec3 aabb2, out float tmin, out float tmax)
{
	vec3 t1 = (aabb1 - P) * invR;
	vec3 t2 = (aabb2 - P) * invR;
	vec3 tmins = min(t1, t2);
	vec3 tmaxs = max(t1, t2);
	tmin = max(max(tmins.x, tmins.y), tmins.z);
	tmax = min(min(tmaxs.x, tmaxs.y), tmaxs.z);
}

void aabbIntersectFull(vec3 P, vec3 R, vec3 invR, vec3 aabb1, vec3 aabb2, out float tmin, out float tmax, out vec3 normal)
{
	vec3 t1 = (aabb1 - P) * invR;
	vec3 t2 = (aabb1 - P) * invR;
	vec3 tmins = min(t1, t2);
	vec3 tmaxs = max(t1, t2);
	tmax = min(min(tmaxs.x, tmaxs.y), tmaxs.z);

	// TODO(pushrax): this can be done without branches
	if (tmins.x > tmins.y && tmins.x > tmins.z) {
		tmin = tmins.x;
		normal = vec3(-sign(R.x), 0, 0);
	} else if (tmins.y > tmins.z) {
		tmin = tmins.y;
		normal = vec3(0, -sign(R.y), 0);
	} else {
		tmin = tmins.z;
		normal = vec3(0, 0, -sign(R.z));
	}
}

void triIntersect(vec3 P, vec3 R, vec3 v0, vec3 v1, vec3 v2, out vec4 uvtw)
{
	// Moller-Trumbore algorithm, optimized for no branching.
	vec3 v01 = v1 - v0;
	vec3 v02 = v2 - v0;
	vec3 pv = cross(R, v02);
	vec3 tv = P - v0;
	vec3 qv = cross(tv, v01);

	uvtw.x = dot(tv, pv);
	uvtw.y = dot(R, qv);
	uvtw.z = dot(v02, qv);

	float det = dot(v01, pv);
	uvtw.xyz /= det;

	uvtw.w = 1.0 - uvtw.x - uvtw.y;
}