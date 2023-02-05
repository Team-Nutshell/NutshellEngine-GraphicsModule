#pragma once
#include "vec2.h"
#include <string>

namespace nml {

struct mat3;
struct mat4;

//  xx | yx
// ----|----
//  xy | yy
struct mat2 {
	vec2 x;
	vec2 y;
	
	// Constructors
	mat2();
	mat2(float _value);
	mat2(float _xx, float _xy, float _yx, float _yy);
	mat2(float _xx, float _xy, vec2 _y);
	mat2(vec2 _x, float _yx, float _yy);
	mat2(vec2 _x, vec2 _y);
	mat2(const float* _ptr);
	mat2(mat3 _mat);
	mat2(mat4 _mat);

	// Operators
	mat2& operator+=(const mat2& other);
	mat2& operator-=(const mat2& other);
	mat2& operator*=(const mat2& other);
	mat2& operator*=(const float other);
	mat2& operator/=(const float other);
	vec2& operator[](size_t index);
	const vec2& operator[](size_t index) const;

	// Functions
	float det() const;

	float* data();
};

// Operators
mat2 operator+(mat2 lhs, const mat2& rhs);
mat2 operator-(mat2 lhs, const mat2& rhs);
mat2 operator*(mat2 lhs, const mat2& rhs);
vec2 operator*(mat2 lhs, const vec2& rhs);
mat2 operator*(mat2 lhs, const float rhs);
mat2 operator*(float lhs, const mat2& rhs);
mat2 operator/(mat2 lhs, const float rhs);
bool operator==(const mat2& lhs, const mat2& rhs);
bool operator!=(const mat2& lhs, const mat2& rhs);

// Functions
mat2 transpose(const mat2& mat);
mat2 inverse(const mat2& mat);

std::string to_string(const mat2& mat);

}