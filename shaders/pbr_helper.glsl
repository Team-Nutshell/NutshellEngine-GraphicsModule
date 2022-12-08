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