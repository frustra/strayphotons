vec3 orientByNormal(float phi, float tht, vec3 normal)
{
	float sintht = sin(tht);
	float xs = sintht * cos(phi);
	float ys = cos(tht);
	float zs = sintht * sin(phi);

	vec3 up = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
	vec3 tangent1 = normalize(up - normal * dot(up, normal));
	vec3 tangent2 = normalize(cross(tangent1, normal));
	return normalize(xs * tangent1 + ys * normal + zs * tangent2);
}

// Generates cosine-distributed vector oriented by an input up vector (normal).
vec3 cosineDirection(vec3 normal, vec2 noise, out float pdf)
{
	float phi = 2.0 * M_PI * noise.y;
	float costht = sqrt(1.0 - noise.x);
	float tht = acos(costht);
	pdf = costht * (1.0 / M_PI);
	return orientByNormal(phi, tht, normal);
}

// Generates GGX-distributed vector oriented by an input up vector (normal).
vec3 GGXDirection(vec3 normal, float roughness, vec2 noise)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;

	float phi = 2.0 * M_PI * noise.y;
	float f = (1.0 - noise.x) / ((alphaSq - 1.0) * noise.x + 1.0);
	float tht = acos(sqrt(f));
	return orientByNormal(phi, tht, normal);
}

vec3 BRDF_Diffuse_Lambert(vec3 diffuseColor) {
	return diffuseColor * (1.0 / M_PI);
}

// Trowbridge-Reitz NDF with the GGX normalization factor
// [Blinn 1997, "Models of Light Reflection for Computer Synthesized Pictures"]
// [Walter 2007, "Microfacet Models for Refraction through Rough Surfaces"]
// [Burley (Disney) 2012, "Practical Physically Based Shading in Film and Game Production"]
float BRDF_D_GGX(float roughness, float NdotM) {
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;
	float d = (NdotM * alphaSq - NdotM) * NdotM + 1;
	return alphaSq / (M_PI * d * d);
}

// Tuned to match behavior of Smith visibility approximation [Karis 2013, "Real Shading in Unreal Engine 4"]
// Includes denominator of base BRDF simplified in; Vis = G / (4 * NdotL * NdotV).
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float BRDF_V_Schlick(float roughness, float NdotV, float NdotL) {
	float alpha = roughness * roughness;
	float k = alpha * 0.5;

	// Calculate both terms G'(l) and G'(v) at once.
	float visV = NdotV * (1 - k) + k;
	float visL = NdotL * (1 - k) + k;
	return 0.25 / (visL * visV + 0.01);
}

// Schlick approximation of the Fresnel term.
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// [Karis 2013, "Real Shading in Unreal Engine 4"] (note, not using same spherical Gaussian approximation but still relevant)
vec3 BRDF_F_Schlick(vec3 specularColor, float VdotH) {
	float Fc = pow(1 - VdotH, 5);
	return clamp(50.0 * specularColor.g, 0, 1) * Fc + (1 - Fc) * specularColor;
}

vec3 evaluateBRDF(vec3 diffuseColor, vec3 specularColor, float roughness, vec3 L, vec3 V, vec3 N) {
	vec3 H = normalize(V + L);
	float NdotV = abs(dot(N, V)) + 1e-5; // see [Lagarde/Rousiers 2014]
	float NdotL = saturate(dot(N, L));
	//float NdotH = saturate(dot(N, H));
	float NdotH = abs(dot(N, H)) + 1e-4;
	float VdotH = saturate(dot(V, H));

	// float D = BRDF_D_Blinn(roughness, NdotH);
	//float Vis = 0.25; // implicit case
	//vec3 F = specularColor; // no fresnel

	float D = BRDF_D_GGX(roughness, NdotH);
	float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
	vec3 F = BRDF_F_Schlick(specularColor, VdotH);

	vec3 diffuse = BRDF_Diffuse_Lambert(diffuseColor);

	//// spec = D * G * F / (4 * NdotL * NdotV) = D * Vis * F
	//// pdf = D * NdotH / (4 * VdotH)
	//// spec / pdf = Vis * F * 4 * VdotH / NdotH

	vec3 specular = D * Vis * F;
	//vec3 specularIS = Vis * F * 4 * VdotH;// / NdotH;

	return diffuse + specular;
}

void generateBounce(inout vec3 P, inout vec3 R, out float pdf, vec2 noise, float roughness, vec3 intersection, vec3 normal)
{
	//vec3 H = GGXDirection(normal, roughness, noise);
	//R = normalize(reflect(R, H));
	R = cosineDirection(normal, noise, pdf);
	P = intersection;
}

vec3 performBounce(inout vec3 P, inout vec3 R, vec3 N, vec3 intersection, vec3 diffuseColor, vec3 specularColor, float roughness, vec2 noise)
{
	vec3 V = -R;
	P = intersection;

	if ((noise.x + noise.y) * 0.5 < roughness) {
		// diffuse
		float pdf;
		R = cosineDirection(N, noise, pdf);
		return BRDF_Diffuse_Lambert(diffuseColor) / pdf;
	} else {
		// specular
		vec3 H = GGXDirection(N, roughness, noise);
		vec3 L = normalize(reflect(R, H));

		float NdotV = abs(dot(N, V)) + 1e-5; // see [Lagarde/Rousiers 2014]
		float NdotL = saturate(dot(N, L));
		float NdotH = saturate(dot(N, H));
		float VdotH = saturate(dot(V, H));

		//float D = BRDF_D_GGX(roughness, NdotH);
		float Vis = BRDF_V_Schlick(roughness, NdotV, NdotL);
		vec3 F = BRDF_F_Schlick(specularColor, VdotH);

		//// spec = D * G * F / (4 * NdotL * NdotV) = D * Vis * F
		//// pdf = D * NdotH / (4 * VdotH)
		//// spec / pdf = Vis * F * 4 * VdotH / NdotH

		R = L;
		return Vis * F * 4 * VdotH / (NdotH + 1e-3);
	}
}
