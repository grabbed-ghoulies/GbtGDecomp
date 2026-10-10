#pragma once
#include "ghouly_box_decoder.h"
#include "gbtg/anim.h"

namespace gbtg
{
using RuntimeBodyMotionCallback = void (*)(float*, const float*, float*, u16, void*, float);
struct RuntimeBodyMotionCallbacks
{
    RuntimeBodyMotionCallback first;
    void* first_data;
    RuntimeBodyMotionCallback second;
    void* second_data;
};
struct RuntimeBodyMotionController
{
    GhoulySpawnPool* joints; // +0, stride 2.
    GhoulySpawnPool* dispatch; // +4, native function pointers replace stride 4.
    GhoulySpawnPool* callbacks; // +8, native pointers replace stride 0x10.
    u32 count; // +0xC.
};
u8* insert_runtime_pool_element(GhoulySpawnPool*& pool, i32 index); // Xbox 0x00066310
i32 initialize_runtime_body_motion_controller(RuntimeBodyMotionController& controller); // Xbox 0x0002F3C0
i32 find_or_insert_runtime_body_motion_joint(RuntimeBodyMotionController& controller, u16 joint); // Xbox 0x0002F480
void register_runtime_body_motion_callback(RuntimeBodyMotionController& controller, u16 joint,
    RuntimeBodyMotionCallback callback, void* data); // Xbox 0x0002F350
void dispatch_runtime_body_motion_callbacks(float* output, const float* local, float* parent,
    u16 index, void* controller, float elapsed); // Xbox 0x0002F6A0
float constrain_runtime_body_angle(float current, float target, float limit); // Xbox 0x0002EF90
void extract_runtime_body_angles(const float* matrix, float& roll, float& pitch,
    float& yaw); // Xbox 0x0002F1D0
void adjust_runtime_body_pitch(float* matrix, float target, float weight); // Xbox 0x0002EEB0
void adjust_runtime_body_yaw(float* matrix, float target, float weight); // Xbox 0x0002EF20
float runtime_body_angle_distance(float current, float target, float anchor); // Xbox 0x00100F30
void accelerate_runtime_body_angle(float& current, const float& target, float& velocity,
    float anchor, float maximum, float acceleration, float elapsed); // Xbox 0x001010B0
void update_runtime_body_joint_motion(float* local, const float* parent, float* output,
    u16 joint, void* record, float elapsed); // Xbox 0x0002F060
}
