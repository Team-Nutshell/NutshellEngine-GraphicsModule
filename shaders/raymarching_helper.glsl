#define M_PI 3.1415926535897932384626433832795

const float MAX_STEPS = 300.0;
const float MAX_DISTANCE = 1000.0;
const float EPSILON = 0.0001;

struct Object {
	float dist;
	float matId;
};

struct Material {
	vec3 diffuse;
	vec2 metallicRoughness;
};

// Random functions
float rand(float seed) {
	return fract(sin(seed));
}

float rand(vec2 seed) {
	return fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(float seed) {
	const float i = floor(seed);
	const float f = fract(seed);

	return mix(rand(i), rand(i + 1.0), smoothstep(0.0, 1.0, f));
}

float noise(vec2 seed) {
	const vec2 i = floor(seed);
	const vec2 f = fract(seed);

	const float a = rand(i);
	const float b = rand(i + vec2(1.0, 0.0));
	const float c = rand(i + vec2(0.0, 1.0));
	const float d = rand(i + vec2(1.0, 1.0));

	const vec2 u = smoothstep(0.0, 1.0, f);

	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float noise(vec2 seed, float width, float freq) {
	const float unit = width / freq;
	const vec2 ij = floor(seed / unit);
	vec2 xy = mod(seed, unit) / unit;
	xy = 0.5 * (1.0 - cos(M_PI * xy));

	const float a = rand(ij);
	const float b = rand(ij + vec2(1.0, 0.0));
	const float c = rand(ij + vec2(0.0, 1.0));
	const float d = rand(ij + vec2(1.0, 1.0));

	const float x1 = mix(a, b, xy.x);
	const float x2 = mix(c, d, xy.x);

	return mix(x1, x2, xy.y);
}

// Shapes
float shSphere(vec3 p, float r) {
	return length(p) - r;
}

float shPlane(vec3 p, vec3 n, float dist) {
	return dot(p, n) + dist;
}

float shBox(vec3 p, vec3 b) {
	const vec3 d = abs(p) - b;

	return length(max(d, vec3(0.0))) + min(max(max(d.x, d.y), d.z), 0.0);
}

float shBox2D(vec2 p, vec2 b) {
	const vec2 d = abs(p) - b;

	return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

float shCylinder(vec3 p, float r, float h) {
	float d = length(p.xz) - r;
	d = max(d, abs(p.y) - h);

	return d;
}

float shTorus(vec3 p, float sr, float lr) {
	return length(vec2(length(p.xz) - lr, p.y)) - sr;
}

// Operations
Object opUnion(Object a, Object b) {
	return a.dist < b.dist ? a : b;
}

Object opIntersection(Object a, Object b) {
	return a.dist > b.dist ? a : b;
}

Object opDifference(Object a, Object b) {
	return a.dist > -b.dist ? a : Object(-b.dist, b.matId);
}