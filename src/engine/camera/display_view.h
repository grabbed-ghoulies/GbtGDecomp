#pragma once
#include "gbtg/entity.h"
namespace gbtg::entity_detail
{
struct DisplayView
{
    i32 aspect_mode = 0; // +0x3C; 3 selects g_aspect_preset
    i32 origin_x = 0; // +0x48
    i32 origin_y = 0; // +0x4C
    i32 width = 0; // +0x50
    i32 height = 0; // +0x54
    Vec3 position{}; // +0
    Quat rotation{}; // +0xC
    Vec3 forward{}; // +0x1C
    u32 flags = 0; // +0x28
    float near_clip = 0.f; // +0x2C
    float far_clip = 0.f; // +0x30
    float focal = 0.f; // +0x34
    float field_of_view = 0.f; // +0x38
    float pixel_aspect = 0.f; // +0x40
    float focal_pixels = 0.f; // +0x44
    i32 fitted_x = 0; // +0x58
    i32 fitted_y = 0; // +0x5C
    i32 fitted_width = 0; // +0x60
    i32 fitted_height = 0; // +0x64
    std::array<float, 12> transform{}; // +0x68
    std::array<float, 16> projection{}; // +0x98
    u8 mark = 0; // +0x1E0
    std::array<float, 12> rotation_transform{}; // +0xD8.
    std::array<float, 12> inverse_transform{}; // +0x108.
    std::array<float, 42> frustum{}; // Origins +0x138, six planes +0x180.
};
extern DisplayView g_display_records[4]; // DAT_00507018, retail stride 0x1E4
extern DisplayView* g_display_view; // DAT_005077A8
extern i32 g_aspect_preset; // DAT_005077F8
extern u8 g_aspect_exact; // DAT_00509704.
extern EntityCamNode g_cam_anchor[4]; // DAT_004E1680
extern EntityCamNode* g_cam_active[4]; // DAT_004E16A0
extern char g_cam_saved_name[4][0x21]; // DAT_004E16B0
}
namespace gbtg
{
void camera_euler_to_quaternion(const Vec3& angles, Quat& result); // Xbox 0x00104EB0
void set_display_rotation(const Quat& rotation); // Xbox 0x000F5AE0
void set_display_focal_length(i32 source_aspect, float focal); // Xbox 0x000F5D30
void initialize_display_views(u16 width, u16 height, i32& render_width,
    i32& render_height); // Xbox 0x000F5900
}
