#define M_PI 3.1415926535897932384626433832795

const uint MAX_STEPS = 256;
const float MAX_DISTANCE = 1000.0;
const float EPSILON = 0.0001;
const uint MAX_BOUNCES = 2;

struct Material {
	vec3 diffuse;
	vec2 metallicRoughness;
};

struct Object {
	float dist;
	Material mat;
};

struct Light {
	vec3 position;
	vec3 color;
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

float noise(vec2 seed, float freq) {
	const float unit = pC.width / freq;
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

float fbm(vec2 seed, float H, uint numIters) {
	float g = exp2(-H);
	float f = 1.0;
	float a = -1.0;
	float t = 0.0;
	for (uint i = 0; i < numIters; i++) {
		t += a * noise(f * seed);
		f *= 2.0;
		a *= g;
	}

	return t;
}

// Shapes
// Sphere
// r = radius
float shSphere(vec3 p, float r) {
	return length(p) - r;
}

// Quad
// a = first vertex
// b = second vertex
// c = third vertex
// d = fourth vertex
float shQuad(vec3 p, vec3 a, vec3 b, vec3 c, vec3 d) {
	const vec3 ba = b - a;
	const vec3 pa = p - a;
	const vec3 cb = c - b;
	const vec3 pb = p - b;
	const vec3 dc = d - c;
	const vec3 pc = p - c;
	const vec3 ad = a - d;
	const vec3 pd = p - d;
	vec3 n = cross(ba, ad);
	const vec3 pba = ba * clamp(dot(ba, pa) / dot(ba, ba), 0.0, 1.0) - pa;
	const vec3 pcb = cb * clamp(dot(cb, pb) / dot(cb, cb), 0.0, 1.0) - pb;
	const vec3 pdc = dc * clamp(dot(dc, pc) / dot(dc, dc), 0.0, 1.0) - pc;
	const vec3 pad = ad * clamp(dot(ad, pd) / dot(ad, ad), 0.0, 1.0) - pd;
	const float ndotpa = dot(n, pa);

	return sqrt((sign(dot(cross(ba, n), pa)) +
		sign(dot(cross(cb, n), pb)) +
		sign(dot(cross(dc, n), pc)) +
		sign(dot(cross(ad, n), pd)) < 3.0) ?
		min(min(min(dot(pba, pba), dot(pcb, pcb)), dot(pdc, pdc)), dot(pad, pad)) :
		ndotpa * ndotpa / dot(n, n));
}

// Infinite plane
// n = normal, must be normalized
// dist = distance from the origin
float shPlane(vec3 p, vec3 n, float dist) {
	return dot(p, n) + dist;
}

// Box
// b = length, height and width of the box
float shBox(vec3 p, vec3 b) {
	const vec3 d = abs(p) - b;

	return length(max(d, vec3(0.0))) + min(max(max(d.x, d.y), d.z), 0.0);
}

// Infinite box
// b = depends on p, can represent length, height and width
float shBox2(vec2 p, vec2 b) {
	const vec2 d = abs(p) - b;

	return length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0);
}

// Triangle
// a = first vertex
// b = second vertex
// c = third vertex
float shTriangle(vec3 p, vec3 a, vec3 b, vec3 c) {
	const vec3 ba = b - a;
	const vec3 pa = p - a;
	const vec3 cb = c - b;
	const vec3 pb = p - b;
	const vec3 ac = a - c;
	const vec3 pc = p - c;
	const vec3 n = cross(ba, ac);
	const vec3 pba = ba * clamp(dot(ba, pa) / dot(ba, ba), 0.0, 1.0) - pa;
	const vec3 pcb = cb * clamp(dot(cb, pb) / dot(cb, cb), 0.0, 1.0) - pb;
	const vec3 pac = ac * clamp(dot(ac, pc) / dot(ac, ac), 0.0, 1.0) - pc;
	const float ndotpa = dot(n, pa);

	return sqrt((sign(dot(cross(ba, n), pa)) +
		sign(dot(cross(cb, n), pb)) +
		sign(dot(cross(ac, n), pc)) < 2.0) ?
		min(min(dot(pba, pba), dot(pcb, pcb)), dot(pac, pac)) :
		ndotpa * ndotpa / dot(n, n));
}

// Cone
// sc = (sin of the angle, cos of the angle)
// h = height
float shCone(vec3 p, vec2 sc, float h) {
	const vec2 q = h * vec2(sc.x / sc.y, -1.0);
	const vec2 w = vec2(length(p.xz), p.y);
	const vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
	const vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0));
	const float k = sign(q.y);
	const float d = min(dot(a, a), dot(b, b));
	const float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));

	return sign(s) * sqrt(d);
}

// Cylinder
// r = radius
// h = height
float shCylinder(vec3 p, float r, float h) {
	float d = length(p.xz) - r;
	d = max(d, abs(p.y) - h);

	return d;
}

// Torus
// r = radius
// d = distance to the center
float shTorus(vec3 p, float r, float d) {
	return length(vec2(length(p.xz) - d, p.y)) - r;
}

// Capped Torus
// sc = (sin of the angle, cos of the angle)
// r = radius
// d = distance to the center
float shCappedTorus(vec3 p, vec2 sc, float r, float d) {
	p.x = abs(p.x);
	const float k = ((sc.y * p.x) > (sc.x * p.y)) ? dot(p.xy, sc) : length(p.xy);

	return sqrt(dot(p, p) + d * d - 2.0 * d * k) - r;	
}

// Link in a chain
// l = length
// r = radius
// d = distance to the center
float shLink(vec3 p, float l, float r, float d) {
	const vec3 q = vec3(p.x, max(abs(p.y) - l, 0.0), p.z);

	return length(vec2(length(q.xy) - d, q.z)) - r;
}

// Operations
Object opUnion(Object a, Object b) {
	return a.dist < b.dist ? a : b;
}

Object opSmoothUnion(Object a, Object b, float k) {
	const float h = max(k - abs(a.dist - b.dist), 0.0);
	const float newDist = min(a.dist, b.dist) - h * h * 0.25 / k;

	const float bf = (h / k) * (h / k) * 0.5;
	const float blendFactor = (a.dist < b.dist) ? bf : (1.0 - bf);

	const Material newMat = Material(mix(a.mat.diffuse, b.mat.diffuse, blendFactor), mix(a.mat.metallicRoughness, b.mat.metallicRoughness, blendFactor));

	return Object(newDist, newMat);
}

Object opIntersection(Object a, Object b) {
	return a.dist > b.dist ? a : b;
}

Object opSmoothIntersection(Object a, Object b, float k) {
	Object object = opSmoothUnion(Object(-a.dist, a.mat), Object(-b.dist, b.mat), k);
	object.dist *= -1.0;

	return object;
}

Object opDifference(Object a, Object b) {
	return a.dist > -b.dist ? a : Object(-b.dist, b.mat);
}

Object opSmoothDifference(Object a, Object b, float k) {
	Object object = opSmoothUnion(b, Object(-a.dist, a.mat), k);
	object.dist *= -1.0;

	return object;
}

Object opMin(Object o, float x) {
	return Object(min(o.dist, x), o.mat);
}

Object opMax(Object o, float x) {
	return Object(max(o.dist, x), o.mat);
}

Object opRound(Object o, float r) {
	return Object(o.dist - r, o.mat);
}

Object opOnion(Object o, float t) {
	return Object(abs(o.dist) - t, o.mat);
}

// Ray Infinite Repeat
// p = ray position
// n = period on each axis
vec3 rayRepeat(vec3 p, vec3 n) {
	return mod(p, n) - (n / 2.0);
}

// Ray Finite Repeat
// p = ray position
// n = period on each axis
// l = number of repetitions on each axis
vec3 rayFiniteRepeat(vec3 p, float n, vec3 l) {
	return p - n * clamp(round(p / n), -l, l);
}