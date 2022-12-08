#version 460

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
	uint time;
	uint width;
	uint height;
} pC;

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

// Shapes
float shSphere(vec3 p, float r) {
	return length(p) - r;
}

float shPlane(vec3 p, vec3 n, float dist) {
	return dot(p, n) + dist;
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

// Raymarching hit
Object intersect(vec3 p) {
	Object plane = Object(shPlane(p, vec3(0.0, 1.0, 0.0), 0.075), 1.0);
	Object sphere = Object(shSphere(p, 0.15), 2.0);
	Object object = opUnion(plane, sphere);

	return object;
}

// Raymarching no-hit
vec3 no_intersect(vec3 p) {
	return vec3(0.5, 0.5, 1.0);
}

// Compute normal
vec3 normal(vec3 p) {
	vec2 e = vec2(EPSILON, 0.0);
	vec3 n = vec3(intersect(p).dist) - vec3(intersect(p - e.xyy).dist, intersect(p - e.yxy).dist, intersect(p - e.yyx).dist);

	return normalize(n);
}

// BRDF
float distribution(float NdotH, float roughness) {
	const float a = roughness * roughness;
	const float aSquare = a * a;
	const float NdotHSquare = NdotH * NdotH;
	const float denom = NdotHSquare * (aSquare - 1.0) + 1.0;

	return aSquare / (M_PI * denom * denom);
}

vec3 fresnel(float cosTheta, vec3 f0) {
	return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float g(float NdotV, float roughness) {
	const float r = roughness + 1.0;
	const float k = (r * r) / 8.0;
	const float denom = NdotV * (1.0 - k) + k;

	return NdotV / denom;
}

float smith(float LdotN, float VdotN, float roughness) {
	const float gv = g(VdotN, roughness);
	const float gl = g(LdotN, roughness);

	return gv * gl;
}

vec3 diffuseFresnelCorrection(vec3 ior) {
	const vec3 iorSquare = ior * ior;
	const bvec3 TIR = lessThan(ior, vec3(1.0));
	const vec3 invDenum = mix(vec3(1.0), vec3(1.0) / (iorSquare * iorSquare * (vec3(554.33) * 380.7 * ior)), TIR);
	vec3 num = ior * mix(vec3(0.1921156102251088), ior * 298.25 - 261.38 * iorSquare + 138.43, TIR);
	num += mix(vec3(0.8078843897748912), vec3(-1.07), TIR);

	return num * invDenum;
}

vec3 brdf(float LdotH, float NdotH, float VdotH, float LdotN, float VdotN, vec3 diffuse, float metallic, float roughness) {
	const float d = distribution(NdotH, roughness);
	const vec3 f = fresnel(LdotH, mix(vec3(0.04), diffuse, metallic));
	const vec3 fT = fresnel(LdotN, mix(vec3(0.04), diffuse, metallic));
	const vec3 fTIR = fresnel(VdotN, mix(vec3(0.04), diffuse, metallic));
	const float g = smith(LdotN, VdotN, roughness);
	const vec3 dfc = diffuseFresnelCorrection(vec3(1.05));

	const vec3 lambertian = diffuse / M_PI;

	return (d * f * g) / max(4.0 * LdotN * VdotN, 0.001) + ((vec3(1.0) - fT) * (vec3(1.0 - fTIR)) * lambertian) * dfc;
}

vec3 shade(vec3 p, vec3 d, vec3 lightPos, vec3 lightColor, vec3 diffuse, float metallic, float roughness) {
	const vec3 l = normalize(lightPos - p);
	const vec3 n = normal(p);
	const vec3 v = -d;
	const vec3 h = normalize(v + l);

	const float LdotH = max(dot(l, h), 0.0);
	const float NdotH = max(dot(n, h), 0.0);
	const float VdotH = max(dot(v, h), 0.0);
	const float LdotN = max(dot(l, n), 0.0);
	const float VdotN = max(dot(v, n), 0.0);

	const vec3 brdf = brdf(LdotH, NdotH, VdotH, LdotN, VdotN, diffuse, metallic, roughness);
	
	return lightColor * brdf * LdotN;
}

// Object material
Material getMaterial(vec3 p, float id) {
	Material material;

	switch (int(id)) {
		case 0:
		material.diffuse = vec3(1.0);
		material.metallicRoughness = vec2(1.0);
		break;

		case 1:
		material.diffuse = vec3(0.2 + 0.5 * mod(floor(p.x) + floor(p.z), 2.0));
		material.metallicRoughness = vec2(0.5, 1.0);
		break;

		case 2:
		material.diffuse = vec3(0.2, 1.0, 1.0);
		material.metallicRoughness = vec2(0.05, 0.0);
		break;

		default:
		material.diffuse = vec3(1.0);
		material.metallicRoughness = vec2(1.0);
	}

	return material;
}

// Raymarching
Object raymarch(vec3 o, vec3 d) {
	Object object = Object(0.0, 0.0);
	for (int i = 0; i < MAX_STEPS; i++) {
		vec3 p = o + object.dist * d;
		Object objectHit = intersect(p);
		if (abs(objectHit.dist) < EPSILON) {
			break;
		}
		object.dist += objectHit.dist;
		object.matId = objectHit.matId;
		if (object.dist > MAX_DISTANCE) {
			break;
		}
	}

	return object;
}

void main() {
	const float time = mod(float(pC.time / 1000.0), 1.0);

	const vec3 from = vec3(0.0, 1.0, -2.0);
	const vec3 to = vec3(0.0, 0.0, 0.0);
	const vec3 up = vec3(0.0, 1.0, 0.0);

	const vec3 forward = normalize(to - from);
	const vec3 right = normalize(cross(up, forward));
	const vec3 realUp = cross(forward, right);

	const mat3 camera = mat3(right, -realUp, forward);

	const float timeAttenuation = 1000.0;
	vec3 lightPos = vec3(cos(pC.time / 200.0), 1.0, sin(pC.time / 200.0));
	const vec3 lightColor = vec3(1.0, 1.0, 1.0);

	vec3 fogColor = vec3(1.0, 1.0, 1.0);
	const float fogDensity = 0.0008;

	const vec2 dim = vec2(pC.width, pC.height);
	const vec2 newUv = (2.0 * (uv * dim) - dim) / pC.height;
	vec3 d = camera * normalize(vec3(newUv, 2.0));

	const Object object = raymarch(from, d);
	const vec3 p = from + object.dist * d;

	vec3 color = vec3(0.0, 0.0, 0.0);
	if (object.dist <= MAX_DISTANCE) {
		const Material mat = getMaterial(p, object.matId);
		const float metallic = mat.metallicRoughness.x;
		const float roughness = mat.metallicRoughness.y;
		color = shade(p, d, lightPos, lightColor, mat.diffuse, metallic, roughness);
		fogColor = no_intersect(p);
		color = mix(color, fogColor, 1.0 - exp(-fogDensity * object.dist * object.dist));
	}
	else {
		color = no_intersect(p);
	}
	outColor = vec4(color, 1.0);
}