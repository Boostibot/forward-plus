#pragma once
#include "lib/math.h"
#include "lib/defines.h"

typedef struct Camera
{
    //All of these fields should be treated as read only. Use the appropriate setters 
    // so that all of the representations remain in check!
    Vec3 pos;
    Vec2 up_local;
    Vec3 look_dir;
    f32 yaw;
    f32 pitch;
    f32 roll;
    
    //f32 pitch_limit;

    //if is set to true, adjust look_dir (and yaw, pitch, roll) when changing direction keeping the camera looking 
    // at the same spot. Similarly makes camera_set_angles perserve look_dir's length. 
    //There are xxx_locked variants of these functions that allow this behaviour to be overriden.
    bool is_locked_on;

    //specifies if should use ortographic projection.
    //If is true uses top/bot/left/right. The area of the rectangle must not be 0!
    bool is_ortographic;  
    bool _padding[2];

    //is_ortographic == false
    f32 near_plane;
    f32 far_plane;
    f32 width_over_height; //aspect ratio written for fools like me
    f32 fov;
    
    //is_ortographic == true
    f32 top;
    f32 bot;
    f32 left;
    f32 right;
} Camera;

void camera_set_perspective(Camera* camera, f32 fov, f32 width_over_height, f32 near_plane, f32 far_plane);
void camera_set_ortographic(Camera* camera, f32 top, f32 bot, f32 left, f32 right); 
void camera_set_locked_on(Camera* camera, bool is_locked_on);

void camera_set_position(Camera* camera, Vec3 pos);
void camera_set_look_at(Camera* camera, Vec3 look_at);
void camera_set_look_dir(Camera* camera, Vec3 look_dir);
void camera_set_angles(Camera* camera, f32 yaw, f32 pitch);
void camera_set_roll(Camera* camera, f32 roll);
void camera_set_position_locked(Camera* camera, Vec3 pos, bool keep_locked_on);
void camera_set_angles_locked(Camera* camera, f32 yaw, f32 pitch, bool keep_locked_on);

Vec3 camera_get_up_dir(Camera camera);
Vec3 camera_get_look_dir(Camera camera);
Vec3 camera_get_look_at(Camera camera);
Vec3 camera_get_right_dir(Camera camera);

Mat4 camera_get_view_matrix(Camera camera);
Mat4 camera_get_projection_matrix(Camera camera);

void camera_set_perspective(Camera* camera, f32 fov, f32 width_over_height, f32 near_plane, f32 far_plane)
{
    camera->fov = fov;
    camera->near_plane = near_plane;
    camera->far_plane = far_plane;
    camera->width_over_height = width_over_height;
    camera->is_ortographic = false;
}

void camera_set_ortographic(Camera* camera, f32 top, f32 bot, f32 left, f32 right)
{
    camera->top = top;
    camera->bot = bot;
    camera->left = left;
    camera->right = right;
    camera->is_ortographic = true;
}

void camera_set_position_locked(Camera* camera, Vec3 pos, bool keep_locked_on)
{
    if(keep_locked_on)
    {
        Vec3 look_at = camera_get_look_at(*camera);
        camera->pos = pos;
        camera_set_look_at(camera, look_at);
    }
    else
        camera->pos = pos;
}

void camera_set_position(Camera* camera, Vec3 pos)
{
    camera_set_position_locked(camera, pos, camera->is_locked_on);
}

void camera_set_locked_on(Camera* camera, bool is_locked_on)
{
    camera->is_locked_on = is_locked_on;
}

void camera_set_roll(Camera* camera, f32 roll)
{
    camera->up_local = vec2(cosf(roll), sinf(roll));
    camera->roll = roll;
}

#define CAM_PITCH_EPSILON 1e-3f

void camera_set_angles_locked(Camera* camera, f32 yaw, f32 pitch, bool keep_locked_on)
{
    pitch = CLAMP(pitch, -PI/2 + CAM_PITCH_EPSILON, PI/2 - CAM_PITCH_EPSILON);

    camera->yaw = yaw;
    camera->pitch = pitch;

    Vec3 look_dir = {0};
    look_dir.x = cosf(yaw) * cosf(pitch);
    look_dir.y = sinf(pitch);
    look_dir.z = sinf(yaw) * cosf(pitch);
    
    if(keep_locked_on == false)
        camera->look_dir = look_dir;
    else
        camera->look_dir = vec3_scale(look_dir, vec3_len(camera->look_dir));
}

void camera_set_angles(Camera* camera, f32 yaw, f32 pitch)
{
    camera_set_angles_locked(camera, yaw, pitch, camera->is_locked_on);
}

void camera_set_angles_relative_to_up_dir(Camera* camera, f32 yaw, f32 pitch, f32 offset_yaw, f32 offst_pitch)
{
    f32 epsilon = 1e-6f;
    f32 rot_yaw = yaw;
    f32 rot_pitch = pitch;
    if(fabsf(camera->roll) > epsilon)
    {
        f32 c = cosf(-camera->roll);
        f32 s = sinf(-camera->roll);
        
        //2D rotation matrix by -roll angle
        rot_yaw   = c*yaw - s*pitch;
        rot_pitch = s*yaw + c*pitch;
    }

    camera_set_angles(camera, rot_yaw + offset_yaw, rot_pitch + offst_pitch);
}

void camera_set_look_dir(Camera* camera, Vec3 look_dir)
{
    Vec3 look_dir_norm = vec3_norm(look_dir);
    camera->yaw = atan2f(look_dir_norm.z,look_dir_norm.x);
    camera->pitch = atan2f(look_dir_norm.y, hypotf(look_dir_norm.z,look_dir_norm.x));
    camera->look_dir = look_dir;
}

void camera_set_look_at(Camera* camera, Vec3 look_at)
{
    Vec3 look_dir = vec3_sub(look_at, camera->pos);
    camera_set_look_dir(camera, look_dir);
}

Mat4 camera_get_projection_matrix(Camera camera)
{
    Mat4 projection = {0};
    if(camera.is_ortographic)
        projection = mat4_ortographic_projection(camera.bot, camera.top, camera.left, camera.right, camera.near_plane, camera.far_plane);
    else
        projection = mat4_perspective_projection(camera.fov, camera.width_over_height, camera.near_plane, camera.far_plane);
    return projection;
}

Vec3 camera_get_up_dir(Camera camera)
{
    Vec3 up = vec3(0, 1, 0);
    Vec3 forward = vec3_norm(camera.look_dir);
    Vec3 right = vec3_norm(vec3_cross(camera.look_dir, up));
    Vec3 tilted_up = vec3_cross(right, forward);

    if(camera.up_local.x == 0 && camera.up_local.y == 0)
        return tilted_up;

    Vec3 up_dir = vec3_add(vec3_scale(tilted_up, camera.up_local.x), vec3_scale(right, camera.up_local.y));
    return vec3_norm(up_dir);
}
Vec3 camera_get_look_dir(Camera camera)
{
    return vec3_norm(camera.look_dir);
}

Vec3 camera_get_look_at(Camera camera)
{
    return vec3_add(camera.look_dir, camera.pos);
}

Mat4 camera_get_view_matrix(Camera camera)
{
    Vec3 look_at = camera_get_look_at(camera);
    Mat4 view = mat4_look_at(camera.pos, look_at, camera_get_up_dir(camera));
    return view;
}

Vec3 camera_get_right_dir(Camera camera)
{
    return vec3_cross(camera_get_look_dir(camera), camera_get_up_dir(camera));
}