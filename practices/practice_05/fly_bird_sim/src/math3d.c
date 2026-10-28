#include "math3d.h"
#include <math.h>
#include <string.h>

float vec3_length(struct vec3 v)
{
	return sqrtf(vec3_length_sq(v));
}

struct vec3 vec3_normalize(struct vec3 v)
{
	float len = vec3_length(v);

	if (len > 1e-6f) {
		float inv = 1.0f / len;
		return vec3_create(v.x * inv, v.y * inv, v.z * inv);
	}
	return vec3_create(0.0f, 0.0f, 0.0f);
}

struct vec3 vec3_lerp(struct vec3 a, struct vec3 b, float t)
{
	return vec3_create(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
			   a.z + (b.z - a.z) * t);
}

struct mat4 mat4_identity(void)
{
	struct mat4 res;

	memset(&res, 0, sizeof(res));
	res.m[0][0] = 1.0f;
	res.m[1][1] = 1.0f;
	res.m[2][2] = 1.0f;
	res.m[3][3] = 1.0f;
	return res;
}

struct mat4 mat4_multiply(const struct mat4 *a, const struct mat4 *b)
{
	struct mat4 res;

	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 4; c++) {
			res.m[r][c] = a->m[r][0] * b->m[0][c] +
				      a->m[r][1] * b->m[1][c] +
				      a->m[r][2] * b->m[2][c] +
				      a->m[r][3] * b->m[3][c];
		}
	}
	return res;
}

struct vec4 mat4_mul_vec4(const struct mat4 *m, struct vec4 v)
{
	struct vec4 res;

	res.x = m->m[0][0] * v.x + m->m[0][1] * v.y + m->m[0][2] * v.z +
		m->m[0][3] * v.w;
	res.y = m->m[1][0] * v.x + m->m[1][1] * v.y + m->m[1][2] * v.z +
		m->m[1][3] * v.w;
	res.z = m->m[2][0] * v.x + m->m[2][1] * v.y + m->m[2][2] * v.z +
		m->m[2][3] * v.w;
	res.w = m->m[3][0] * v.x + m->m[3][1] * v.y + m->m[3][2] * v.z +
		m->m[3][3] * v.w;
	return res;
}

struct vec3 mat4_mul_point(const struct mat4 *m, struct vec3 p)
{
	struct vec4 v4 = {p.x, p.y, p.z, 1.0f};
	struct vec4 res = mat4_mul_vec4(m, v4);

	if (fabsf(res.w) > 1e-6f) {
		float inv_w = 1.0f / res.w;
		return vec3_create(res.x * inv_w, res.y * inv_w, res.z * inv_w);
	}
	return vec3_create(res.x, res.y, res.z);
}

struct vec3 mat4_mul_dir(const struct mat4 *m, struct vec3 d)
{
	struct vec3 res;

	res.x = m->m[0][0] * d.x + m->m[0][1] * d.y + m->m[0][2] * d.z;
	res.y = m->m[1][0] * d.x + m->m[1][1] * d.y + m->m[1][2] * d.z;
	res.z = m->m[2][0] * d.x + m->m[2][1] * d.y + m->m[2][2] * d.z;
	return res;
}

struct mat4 mat4_translation(float tx, float ty, float tz)
{
	struct mat4 m = mat4_identity();

	m.m[0][3] = tx;
	m.m[1][3] = ty;
	m.m[2][3] = tz;
	return m;
}

struct mat4 mat4_scaling(float sx, float sy, float sz)
{
	struct mat4 m = mat4_identity();

	m.m[0][0] = sx;
	m.m[1][1] = sy;
	m.m[2][2] = sz;
	return m;
}

struct mat4 mat4_rotation_x(float rad)
{
	struct mat4 m = mat4_identity();
	float c = cosf(rad);
	float s = sinf(rad);

	m.m[1][1] = c;
	m.m[1][2] = -s;
	m.m[2][1] = s;
	m.m[2][2] = c;
	return m;
}

struct mat4 mat4_rotation_y(float rad)
{
	struct mat4 m = mat4_identity();
	float c = cosf(rad);
	float s = sinf(rad);

	m.m[0][0] = c;
	m.m[0][2] = s;
	m.m[2][0] = -s;
	m.m[2][2] = c;
	return m;
}

struct mat4 mat4_rotation_z(float rad)
{
	struct mat4 m = mat4_identity();
	float c = cosf(rad);
	float s = sinf(rad);

	m.m[0][0] = c;
	m.m[0][1] = -s;
	m.m[1][0] = s;
	m.m[1][1] = c;
	return m;
}

/*
 * Combined Yaw (Y) * Pitch (X) * Roll (Z) rotation matrix
 */
struct mat4 mat4_rotation_yaw_pitch_roll(float yaw, float pitch, float roll)
{
	struct mat4 ry = mat4_rotation_y(yaw);
	struct mat4 rx = mat4_rotation_x(pitch);
	struct mat4 rz = mat4_rotation_z(roll);
	struct mat4 temp = mat4_multiply(&ry, &rx);

	return mat4_multiply(&temp, &rz);
}

struct mat4 mat4_look_at(struct vec3 eye, struct vec3 target, struct vec3 up)
{
	struct vec3 f = vec3_normalize(vec3_sub(target, eye));
	struct vec3 r = vec3_normalize(vec3_cross(f, up));
	struct vec3 u = vec3_cross(r, f);
	struct mat4 m = mat4_identity();

	m.m[0][0] = r.x;
	m.m[0][1] = r.y;
	m.m[0][2] = r.z;
	m.m[0][3] = -vec3_dot(r, eye);

	m.m[1][0] = u.x;
	m.m[1][1] = u.y;
	m.m[1][2] = u.z;
	m.m[1][3] = -vec3_dot(u, eye);

	m.m[2][0] = f.x;
	m.m[2][1] = f.y;
	m.m[2][2] = f.z;
	m.m[2][3] = -vec3_dot(f, eye);

	return m;
}

struct mat4 mat4_perspective(float fov_rad, float aspect, float near_z,
			     float far_z)
{
	struct mat4 m;
	float tan_half_fov = tanf(fov_rad * 0.5f);

	memset(&m, 0, sizeof(m));
	m.m[0][0] = 1.0f / (aspect * tan_half_fov);
	m.m[1][1] = 1.0f / tan_half_fov;
	m.m[2][2] = (far_z + near_z) / (far_z - near_z);
	m.m[2][3] = -(2.0f * far_z * near_z) / (far_z - near_z);
	m.m[3][2] = 1.0f;
	m.m[3][3] = 0.0f;

	return m;
}
