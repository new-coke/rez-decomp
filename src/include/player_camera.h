#ifndef REZ_PLAYER_CAMERA_H
#define REZ_PLAYER_CAMERA_H

struct PlayerCam;
struct PlayerCam;
struct IVec3;
struct PlayerCam;
struct Camera;
struct PlayerCamAngZView;
struct PlayerCamOfsView;
struct PlayerCamAimView;
struct PlayerCameraDataView;
struct PlayerCamAngZView;
struct PlayerCamAngXView;
struct PlayerCamWork;
struct I3ZeroPlayerCameraData;
struct Vec3;

void player_camera_vars_register(void);
int player_camera_start(void * parent);
void player_task_end_child(void *work);
void player_camera_copy(struct PlayerCam *src, struct PlayerCam *dst);
void player_camera_set_aim(struct PlayerCam *w, struct IVec3 *ang);
void *player_camera_get_preset(void);
void player_camera_set_target(struct I3ZeroPlayerCameraData *src);
void player_camera_set_data(struct I3ZeroPlayerCameraData *src);
void player_camera_get_angles(struct IVec3 *out);
void player_camera_get_fwd(struct Vec3 *out);

#endif /* REZ_PLAYER_CAMERA_H */
