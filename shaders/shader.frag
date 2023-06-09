void main() {
	vec3 finalColor = vec3(0.0, 0.0, 0.0);

	vec2 dim = vec2(pC.width, pC.height);
	vec2 newUv = (2.0 * (uv * dim) - dim) / dim.y;
	vec2 uvO = newUv;
	
	for (float i = 0.0; i < 5.0; i += 1.0) {
		newUv = fract(newUv * 10.0) - 0.5;
		float d = length(newUv) * dot(uvO, vec2(0.5, 1.0) * 20.0);
		d = length(d * uvO);
		d = sin(d * 8.0 - pC.time * 2.0) - 0.75;
		d = abs(log(d));
		finalColor += d;
	}

	outColor = vec4(finalColor, 0.0);	
}