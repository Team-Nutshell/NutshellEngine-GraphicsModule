#version 460
#extension GL_GOOGLE_include_directive : enable

#include "raymarching_helper.glsl"
#include "pbr_helper.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
	float time;
	uint width;
	uint height;
} pC;

// Raymarching hit
Object scene(vec3 p) {
	Object plane = Object(shPlane(p, vec3(0.0, 1.0, 0.0), 0.15), 1.0);

	Object sphere = Object(shSphere(p, 0.25), 2.0);

	Object torus = Object(shTorus(p, 0.2, 0.75), 3.0);

	Object cylinder = Object(shCylinder(p, 0.1, 0.3), 4.0);

	Object object = opUnion(plane, sphere);
	object = opDifference(object, torus);
	object = opDifference(object, cylinder);

	return object;
}

// Raymarching no-hit
vec3 background(vec3 p) {
	return vec3(0.5, 0.5, 1.0);
}

// Compute normal
vec3 normal(vec3 p) {
	vec2 e = vec2(EPSILON, 0.0);
	vec3 n = vec3(scene(p).dist) - vec3(scene(p - e.xyy).dist, scene(p - e.yxy).dist, scene(p - e.yyx).dist);

	return normalize(n);
}

// Raymarching
Object raymarch(vec3 o, vec3 d) {
	Object object = Object(0.0, 0.0);
	for (int i = 0; i < MAX_STEPS; i++) {
		vec3 p = o + object.dist * d;
		Object objectHit = scene(p);
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
		material.diffuse = vec3(1.0, 0.0, 0.0);
		material.metallicRoughness = vec2(1.0, 0.5);
		break;

		case 3:
		material.diffuse = vec3(0.0, 0.0, 1.0);
		material.metallicRoughness = vec2(0.35, 1.0);
		break;

		case 4:
		material.diffuse = vec3(0.0, 1.0, 0.0);
		material.metallicRoughness = vec2(0.05, 0.5);
		break;

		default:
		material.diffuse = vec3(1.0);
		material.metallicRoughness = vec2(1.0);
	}

	return material;
}

// Shadows
float shadows(vec3 p, vec3 n, vec3 lightPos) {
	const vec3 d = normalize(lightPos - p);
	const Object object = raymarch(p + n * EPSILON, d);
	if (object.dist <= length(d)) {
		return 0.0;
	}
	
	return 1.0;
}

void main() {
	const vec3 from = vec3(0.0, 1.0, -2.0);
	const vec3 to = vec3(0.0, 0.0, 0.0);
	const vec3 up = vec3(0.0, 1.0, 0.0);

	const vec3 forward = normalize(to - from);
	const vec3 right = normalize(cross(up, forward));
	const vec3 realUp = cross(forward, right);

	const mat3 camera = mat3(right, -realUp, forward);

	const float timeAttenuation = 1000.0;
	vec3 lightPos = vec3(sin(pC.time), 1.0, cos(pC.time));
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
		const vec3 n = normal(p);
		const Material mat = getMaterial(p, object.matId);
		const float metallic = mat.metallicRoughness.x;
		const float roughness = mat.metallicRoughness.y;
		color = shade(p, d, n, lightPos, lightColor, mat.diffuse, metallic, roughness);
		color *= shadows(p, n, lightPos);
		fogColor = background(p);
		color = mix(color, fogColor, 1.0 - exp(-fogDensity * object.dist * object.dist));
	}
	else {
		color = background(p);
	}
	outColor = vec4(color, 1.0);
}