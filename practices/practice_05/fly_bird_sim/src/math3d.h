#ifndef MATH3D_H
#define MATH3D_H

#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct vec3 {
	float x;
	float y;
	float z;
};

struct vec4 {
	float x;
	float y;
	float z;
	float w;
};

struct mat4 {
	float m[4][4];
};

static inline struct vec3 vec3_create(float x, float y, float z)
{
	struct vec3 v = {x, y, z};
	return v;
}

static inline struct vec3 vec3_add(struct vec3 a, struct vec3 b)
{
	return vec3_create(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline struct vec3 vec3_sub(struct vec3 a, struct vec3 b)
{
	return vec3_create(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline struct vec3 vec3_scale(struct vec3 v, float s)
{
	return vec3_create(v.x * s, v.y * s, v.z * s);
}

static inline float vec3_dot(struct vec3 a, struct vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline struct vec3 vec3_cross(struct vec3 a, struct vec3 b)
{
	return vec3_create(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
			   a.x * b.y - a.y * b.x);
}

static inline float vec3_length_sq(struct vec3 v)
{
	return vec3_dot(v, v);
}

float vec3_length(struct vec3 v);
struct vec3 vec3_normalize(struct vec3 v);
struct vec3 vec3_lerp(struct vec3 a, struct vec3 b, float t);

struct mat4 mat4_identity(void);
struct mat4 mat4_multiply(const struct mat4 *a, const struct mat4 *b);
struct vec4 mat4_mul_vec4(const struct mat4 *m, struct vec4 v);
struct vec3 mat4_mul_point(const struct mat4 *m, struct vec3 p);
struct vec3 mat4_mul_dir(const struct mat4 *m, struct vec3 d);

struct mat4 mat4_translation(float tx, float ty, float tz);
struct mat4 mat4_scaling(float sx, float sy, float sz);
struct mat4 mat4_rotation_x(float rad);
struct mat4 mat4_rotation_y(float rad);
struct mat4 mat4_rotation_z(float rad);
struct mat4 mat4_rotation_yaw_pitch_roll(float yaw, float pitch, float roll);

struct mat4 mat4_look_at(struct vec3 eye, struct vec3 target, struct vec3 up);
struct mat4 mat4_perspective(float fov_rad, float aspect, float near_z,
			     float far_z);

#define DEG_TO_RAD_FACTOR (M_PI / 180.0f)

static inline float deg_to_rad(float deg)
{
	return deg * DEG_TO_RAD_FACTOR;
}

static inline float clampf(float val, float min_v, float max_v)
{
	if (val < min_v)
		return min_v;
	if (val > max_v)
		return max_v;
	return val;
}

#endif /* MATH3D_H */
