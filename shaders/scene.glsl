// Scene
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

// Background
vec3 background(vec3 p) {
	return vec3(0.5, 0.5, 1.0);
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

// Camera
vec3 from = vec3(0.0, 1.0, -2.0);
vec3 to = vec3(0.0, 0.0, 0.0);
vec3 up = vec3(0.0, 1.0, 0.0);

mat3 camera() {
	const vec3 forward = normalize(to - from);
	const vec3 right = normalize(cross(up, forward));
	const vec3 realUp = cross(forward, right);

	return mat3(right, -realUp, forward);
}

#define LIGHTS_COUNT 2
// Light
Light[LIGHTS_COUNT] lights() {
	Light l[LIGHTS_COUNT];
	l[0] = Light(vec3(sin(pC.time), 1.0, cos(pC.time)), vec3(1.0, 1.0, 1.0));
	l[1] = Light(vec3(cos(pC.time), 1.0, sin(pC.time)), vec3(1.0, 1.0, 1.0));

	return l;
}