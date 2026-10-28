#include "camera.h"
#include <math.h>

void camera_init(struct camera *cam)
{
	cam->eye = vec3_create(0.0f, 15.0f, -20.0f);
	cam->target = vec3_create(0.0f, 10.0f, 0.0f);
	cam->up = vec3_create(0.0f, 1.0f, 0.0f);

	cam->distance = CAM_DEFAULT_DISTANCE;
	cam->height = CAM_DEFAULT_HEIGHT;
	cam->pitch_offset = deg_to_rad(CAM_DEFAULT_PITCH_OFFSET_DEG);
	cam->base_fov = deg_to_rad(CAM_DEFAULT_BASE_FOV_DEG);
	cam->fov = cam->base_fov;
	cam->smoothness = CAM_DEFAULT_SMOOTHNESS;
}

void camera_update_chase(struct camera *cam, struct vec3 target_pos,
			 float target_yaw, float target_pitch,
			 float target_roll, float speed, float dt)
{
	/* Forward and right direction from target yaw */
	float forward_x = sinf(target_yaw);
	float forward_z = cosf(target_yaw);
	float right_x = cosf(target_yaw);
	float right_z = -sinf(target_yaw);

	/* Desired camera eye position behind and above bird */
	struct vec3 desired_eye = vec3_create(
		target_pos.x - forward_x * cam->distance,
		target_pos.y + cam->height -
			sinf(target_pitch) * CAM_PITCH_ELEVATION_FACTOR,
		target_pos.z - forward_z * cam->distance);

	/* Desired look-at target slightly ahead of bird */
	struct vec3 desired_target = vec3_create(
		target_pos.x + forward_x * CAM_LOOK_AHEAD_DISTANCE,
		target_pos.y + 0.5f -
			sinf(target_pitch + cam->pitch_offset) *
				CAM_PITCH_LOOK_FACTOR,
		target_pos.z + forward_z * CAM_LOOK_AHEAD_DISTANCE);

	/* Smooth dynamic FOV kick based on bird flight speed */
	float speed_factor = (speed - 12.0f) / (36.0f - 12.0f);
	speed_factor = clampf(speed_factor, 0.0f, 1.0f);
	float target_fov =
		cam->base_fov + speed_factor * deg_to_rad(CAM_MAX_FOV_KICK_DEG);

	/* Camera roll banking tilt to heighten dynamic flight feeling */
	float cam_roll = target_roll * CAM_BANK_TILT_FACTOR;
	struct vec3 target_up =
		vec3_create(-right_x * sinf(cam_roll), cosf(cam_roll),
			    -right_z * sinf(cam_roll));

	/* Exponential smoothing / lerp */
	float t = 1.0f - expf(-cam->smoothness * dt);
	if (t > 1.0f)
		t = 1.0f;

	cam->eye = vec3_lerp(cam->eye, desired_eye, t);
	cam->target = vec3_lerp(cam->target, desired_target, t);
	cam->up = vec3_normalize(vec3_lerp(cam->up, target_up, t));
	cam->fov += (target_fov - cam->fov) *
		    (1.0f - expf(-CAM_FOV_LERP_SPEED * dt));
}
