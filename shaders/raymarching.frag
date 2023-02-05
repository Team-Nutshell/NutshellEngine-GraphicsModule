#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
	float time;
	uint width;
	uint height;
	vec3 cameraPosition;
	vec3 cameraDirection;
} pC;

#include "raymarching_helper.glsl"
#include "scene.glsl"

// Raymarching
Object raymarch(vec3 o, vec3 d) {
	Object object = Object(0.0, Material(vec3(0.0), vec2(0.0)));
	for (int i = 0; i < MAX_STEPS; i++) {
		vec3 p = o + object.dist * d;
		Object objectHit = scene(p);
		if (abs(objectHit.dist) < EPSILON) {
			break;
		}
		object.dist += objectHit.dist;
		object.mat = objectHit.mat;
		if (object.dist > MAX_DISTANCE) {
			break;
		}
	}

	return object;
}

// Compute normal
vec3 normal(vec3 p) {
	const vec2 e = vec2(EPSILON, 0.0);
	const vec3 n = vec3(scene(p).dist) - vec3(scene(p - e.xyy).dist, scene(p - e.yxy).dist, scene(p - e.yyx).dist);

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

vec3 shade(vec3 p, vec3 d, vec3 n, vec3 lightPos, vec3 lightColor, vec3 diffuse, float metallic, float roughness) {
	const vec3 l = normalize(lightPos - p);
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

// Shadows
float shadows(vec3 p, vec3 n, vec3 lightPos) {
	float res = 1.0;
	float dist = 0.01;
	float lightSize = 0.15;
	for (int i = 0; i < MAX_STEPS; i++) {
		float hit = scene(p + lightPos * dist).dist;
		res = min(res, hit / (dist * lightSize));
		dist += hit;
		if (hit < EPSILON || dist > 60.0) {
			break;
		}
	}

	return clamp(res, 0.0, 1.0);
}

// Ambient Occlusion
float ambientOcclusion(vec3 p, vec3 n) {
	float ao = 0.0;
	float weight = 1.0;
	for (int i = 0; i < 8; i++) {
		float len = 0.01 + 0.02 * float(i * i);
		float dist = scene(p + n * len).dist;
		ao += (len - dist) * weight;
		weight *= 0.85;
	}

	return 1.0 - clamp(0.6 * ao, 0.0, 1.0);
}

// Render
vec3 render(vec3 o, vec3 d) {
	Light l[LIGHTS_COUNT] = lights();
	const float fogDensity = 0.0008;

	float frac = 1.0;
	vec3 color = vec3(0.0, 0.0, 0.0);
	for (uint depth = 0; depth < MAX_BOUNCES + 1; depth++) {
		vec3 localColor = vec3(0.0, 0.0, 0.0);

		// Raymarch
		const Object object = raymarch(o, d);
		const vec3 p = o + object.dist * d;
		
		const vec3 n = normal(p);
		const float metallic = object.mat.metallicRoughness.x;
		if (object.dist <= MAX_DISTANCE) {
			// Object properties
			const vec3 diffuse = object.mat.diffuse;
			const float roughness = object.mat.metallicRoughness.y;
			const float ao = ambientOcclusion(p, n);

			// Local color
			for (int i = 0; i < LIGHTS_COUNT; i++) {
				localColor += shade(p, d, n, l[i].position, l[i].color, diffuse, metallic, roughness) * shadows(p, n, l[i].position);
			}
			localColor *= ao;
			localColor = mix(localColor, background(p), 1.0 - exp(-fogDensity * object.dist * object.dist));
		}
		else {
			color += background(p) * frac;
			return color;
		}

		color += localColor * frac;
		frac *= metallic;

		// Early stop as the impact on the final color will not be consequent
		if (frac < 0.05) {
			break;
		}

		o = p + (n * EPSILON);
		d = reflect(d, n);
	}

	return color;
}

void main() {
	const mat3 cameraMatrix = camera();

	const vec2 dim = vec2(pC.width, pC.height);
	const vec2 newUv = (2.0 * (uv * dim) - dim) / pC.height;
	vec3 d = cameraMatrix * normalize(vec3(newUv, 2.0));

	vec3 color = render(vec3(-pC.cameraPosition.x, pC.cameraPosition.yz), d);
	
	outColor = vec4(color, 1.0);
}