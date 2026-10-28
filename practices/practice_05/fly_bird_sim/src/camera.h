#ifndef CAMERA_H
#define CAMERA_H

#include "math3d.h"

#define CAM_DEFAULT_DISTANCE 9.0f
#define CAM_DEFAULT_HEIGHT 3.2f
#define CAM_DEFAULT_PITCH_OFFSET_DEG 6.0f
#define CAM_DEFAULT_BASE_FOV_DEG 65.0f
#define CAM_MAX_FOV_KICK_DEG 16.0f
#define CAM_DEFAULT_SMOOTHNESS 8.0f
#define CAM_LOOK_AHEAD_DISTANCE 8.0f
#define CAM_PITCH_LOOK_FACTOR 3.0f
#define CAM_PITCH_ELEVATION_FACTOR 1.5f
#define CAM_BANK_TILT_FACTOR 0.35f
#define CAM_FOV_LERP_SPEED 4.0f

struct camera {
	struct vec3 eye;
	struct vec3 target;
	struct vec3 up;

	float distance;
	float height;
	float pitch_offset;
	float base_fov;
	float fov;
	float smoothness;
};

void camera_init(struct camera *cam);
void camera_update_chase(struct camera *cam, struct vec3 target_pos,
			 float target_yaw, float target_pitch,
			 float target_roll, float speed, float dt);

#endif /* CAMERA_H */
