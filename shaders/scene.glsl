// Scene
Object scene(vec3 p) {
	Object plane = Object(shPlane(p, vec3(0.0, 1.0, 0.0), 0.15), Material(vec3(0.2 + 0.5 * mod(floor(p.x) + floor(p.z), 2.0)), vec2(0.0, 0.2)));

	Object sphere = Object(shSphere(p, 0.25), Material(vec3(1.0, 0.0, 0.0), vec2(0.1, 0.5)));

	Object torus = Object(shTorus(p, 0.2, 0.75), Material(vec3(0.0, 0.0, 1.0), vec2(0.05, 1.0)));

	Object cylinder = Object(shCylinder(p, 0.1, 0.3), Material(vec3(0.0, 1.0, 0.0), vec2(0.05, 0.5)));

	Object object = opSmoothUnion(plane, sphere, 0.1);
	object = opSmoothDifference(object, torus, 0.02);
	object = opSmoothDifference(object, cylinder, 0.02);

	return object;
}

// Background
vec3 background(vec3 p) {
	return vec3(0.5, 0.5, 1.0);
}

// Camera
vec3 up = vec3(0.0, 1.0, 0.0);

mat3 camera() {
	const vec3 forward = normalize(vec3(-pC.cameraDirection.x, pC.cameraDirection.yz));
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