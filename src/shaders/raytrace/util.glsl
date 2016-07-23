float hash(float n)
{
	return fract(sin(n) * 43758.543123);
}

float rand(float x)
{
	float i = floor(x);
	float f = fract(x + seed);
	float u = f * f * (3.0 - 2.0 * f);
	return mix(hash(i), hash(i + 1.0), u);
}

float rand2(inout vec4 state)
{
	// Hachisuka 2015
	const vec4 q = vec4(1225.0, 1585.0, 2457.0, 2098.0);
	const vec4 r = vec4(1112.0, 367.0, 92.0, 265.0);
	const vec4 a = vec4(3423.0, 2646.0, 1707.0, 1999.0);
	const vec4 m = vec4(4194287.0, 4194277.0, 4194191.0, 4194167.0);

	vec4 b = floor(state / q);
	vec4 p = a * (state - b * q) - b * r;
	b = (sign(-p) + vec4(1.0)) * vec4(0.5) * m;
	state = p + b;

	return fract(dot(state / m, vec4(1.0, -1.0, 1.0, -1.0)));
}