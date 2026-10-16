#pragma once

#include "gbtg/model.h"
#include <functional>
#include <array>
#include <memory>
#include <unordered_map>

namespace gbtg
{
float animation_absolute(float value); // Xbox 0x00152BB0

struct ResourceErrorState;
int compare_animation_resource_type(const char* first, const char* second); // Xbox 0x000FBF50
struct ModelAnimationState;
long double animation_remainder(float value, float divisor); // Xbox 0x00152BE0
long double retail_sin(float value); // typed entry to Xbox 0x00153320
long double retail_cos(float value); // Xbox 0x00153230
long double retail_asin(float value); // Xbox 0x00152E90
long double retail_atan2(float y, float x); // Xbox 0x001530E0
long double retail_reciprocal_sqrt(float value); // Typed entry to Xbox 0x001534C0
long double retail_square_root(float value); // Typed entry to Xbox 0x00152B30
void normalize_model_matrix(float matrix[12], float target); // Xbox 0x00103AF0
bool invert_model_matrix(float output[12], const float input[12]); // Xbox 0x00104400

// One joint after 0x2E8A0. quat is xyzw, translation and scale are in joint space.
struct JointPose
{
    Quat rotation;
    Vec3 translation;
    Vec3 scale{1.f, 1.f, 1.f};
};

struct AnimationEventClock
{
    float position = 0.f; // player +0x14
    float previous = 0.f; // player +0x10C
    float speed = 0.f;    // player +0x1C
};

// Xbox 0x0002DFC0. Callback phases are 0 (begin), 1 (end), 2 (active).
bool dispatch_animation_event(std::span<const u8> event, u32 expected_size, u32 type_flags,
                              const AnimationEventClock& clock,
                              const std::function<void(int phase)>& callback);

using AnimationEventCallback = std::function<void(std::span<const u8> event, int phase)>;
void dispatch_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                               const AnimationEventCallback& callback); // Xbox 0x0002DE10
void begin_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                            const AnimationEventCallback& callback); // Xbox 0x0002DF30
void end_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                          const AnimationEventCallback& callback); // Xbox 0x0002DE70

// Linear-interp sample of a MINA block (player+0x2B == 0, blend weight 1).
// `time_norm` is seconds/duration, the value the cutscene writes to the anim player at +0x14.
// `loop` is player+0x2A == 0. Cutscene init (0x31AC0 from 0x2D8E0) stores 1, so playback clamps.
std::vector<JointPose> sample_mina(std::span<const u8> mina, float time_norm, bool loop = false);
// Root channel selected by 0x0002E8A0's param_4, and the loop displacement
// decoded by 0x0002E750 / 0x0002E650. Callers test the root-frame count first.
JointPose sample_mina_root(std::span<const u8> mina, float time_norm);
Vec3 mina_root_cycle_translation(std::span<const u8> mina);

struct MinaPlayback : AnimationEventClock
{
    std::span<const u8> mina; // player +0x00, owned/borrowed resource also at +0xB0
    float rate_factor = 1.f; // +0x18
    float duration = 0.f;    // +0x20
    float weight = 1.f;      // +0x24
    i16 cycles = 0;          // +0x28
    u8 mode = 0;             // +0x2A: loop, clamp, ping-pong
    u8 sampling_mode = 0;    // +0x2B
};

struct AnimationRoot
{
    Quat rotation{0.f, 0.f, 0.f, 1.f};
    Vec3 translation{};
};

struct AnimationPose
{
    std::vector<JointPose> joints;
    std::array<float, 64> auxiliary{}; // allocation trailer is 0x100 bytes
};

struct AnimationResourceRecord
{
    u32 resource_slot = 0; // +0x80, one-based index into loaded resources
    i32 references = 0;   // +0x84
    std::string name;     // +0x00, original 0x80-byte name buffer
    u32 hash = 0;         // +0x88, record stride 0x8C
};
struct AnimationLoadedResource
{
    std::span<const u8> cpu; // +8
    std::span<const u8> gpu; // +0xC
    std::span<const u8> cpu_storage; // +0x10
    std::span<const u8> gpu_storage; // +0x14
    u32 provider = 0;       // +0
    u32 provider_slot = 0;  // +4, record stride 0x18
};
struct AnimationResourceProvider
{
    u32 handle = 0;
    std::function<u32(u32, const AnimationResourceRecord&, const void*&)> find;
    std::function<bool(u32, const void*, AnimationLoadedResource&)> load;
    std::function<void(u32, const char*, const AnimationLoadedResource&)> unload;
    int category = 0;
    const void* context = nullptr;
};
struct AnimationResourceHook
{
    const char* type = nullptr;
    std::function<void(const AnimationLoadedResource&)> relocate; // +4
    std::function<void(const AnimationLoadedResource&)> post_load; // +8
    std::function<void(const AnimationLoadedResource&)> unload; // +0xC
};
struct AnimationResourceRegistry
{
    std::vector<AnimationResourceRecord> records; // DAT_004DF250, used records
    std::vector<AnimationLoadedResource> loaded; // DAT_004DF09C
    std::vector<AnimationResourceRecord> deferred[2]; // DAT_004DF244 / DAT_004DF248
    std::array<AnimationResourceProvider, 16> providers; // DAT_004DF0A0
    std::array<AnimationResourceHook, 16> hooks; // DAT_004DF258
    u32 provider_serial = 0; // DAT_0042195C
    u32 current_deferred = 0; // DAT_004DF24C: which deferred list unloads go on.
    i32 flushing = -1; // DAT_004DF240: the deferred list 0x00036180 is unloading, or -1.
    size_t record_limit = 32;
    size_t loaded_limit = 32;
    AnimationResourceRegistry(); // host storage for pools initialized in 0x00033030 / 0x00035A90
};
struct AnimationBundleRecord
{
    std::string name; // +0, stride 0xA0
    i32 references = 0; // +0x88
    std::span<const u8> cpu; // +0x90
    std::span<const u8> gpu; // +0x94
    std::span<const u8> cpu_storage; // +0x98
    std::span<const u8> gpu_storage; // +0x9C
};
struct AnimationBundlePool
{
    std::span<AnimationBundleRecord> records; // +0x10, count +0xC
    bool active; // +0x24, pool stride 0x2C
};
struct ExecutableResourceRecord
{
    std::string name; // +0, stride 0x8C
    std::span<const u8> cpu; // +0x80
    std::span<const u8> gpu; // +0x84
    u32 hash = 0; // +0x88
};
struct ExecutableResourcePool
{
    std::span<ExecutableResourceRecord> records; // +4, count +0
    std::string type; // +8
    bool active; // +0x28, stride 0x2C
};
bool resource_names_match(const char* first, const char* second); // Xbox 0x000357D0
bool find_bundle_resource(std::span<AnimationBundlePool> pools, const char* name,
    AnimationBundlePool*& pool, AnimationBundleRecord*& record); // Xbox 0x00034A70
u32 find_bundle_resource_provider(std::span<AnimationBundlePool> pools, const char* name,
    const void*& resolved); // Xbox 0x00034A30
bool load_bundle_resource_provider(const void* resolved, AnimationLoadedResource& output); // Xbox 0x00034AF0
void unload_bundle_resource_provider(std::span<AnimationBundlePool> pools, const char* name); // Xbox 0x00034B30
ExecutableResourceRecord* find_executable_resource_record(std::span<ExecutableResourceRecord> records,
    const char* name, u32 hash); // Xbox 0x00034CB0
ExecutableResourceRecord* find_executable_resource(std::span<ExecutableResourcePool> pools,
    const char* name, bool include_inactive); // Xbox 0x00034D00
u32 find_executable_resource_provider(std::span<ExecutableResourcePool> pools, const char* name,
    const void*& resolved); // Xbox 0x00034E90
bool load_executable_resource_provider(const void* resolved, AnimationLoadedResource& output); // Xbox 0x00034EC0
void register_executable_resource_pool(std::vector<ExecutableResourcePool>& pools,
    std::span<ExecutableResourceRecord> records, const char* type, bool active); // Xbox 0x00034E00
struct ClonedExecutableResource
{
    std::unique_ptr<u8[]> bytes;
    size_t size = 0;
};
ClonedExecutableResource clone_executable_resource(std::span<ExecutableResourcePool> pools,
    const char* name); // Xbox 0x00107720
u32 animation_resource_hash(const char* name); // Xbox 0x000358B0
AnimationResourceRecord* find_animation_resource(std::vector<AnimationResourceRecord>& records,
                                                   const char* name); // Xbox 0x00036030
void register_animation_resource_provider(AnimationResourceRegistry& registry,
    std::function<u32(u32, const AnimationResourceRecord&, const void*&)> find,
    std::function<bool(u32, const void*, AnimationLoadedResource&)> load,
    std::function<void(u32, const char*, const AnimationLoadedResource&)> unload,
    int category, const void* context); // Xbox 0x00035540
bool relocate_animation_bundle_pool(AnimationBundlePool& pool, AnimationResourceRegistry& registry,
    const u32& loading, const std::function<void()>& pump_audio); // Xbox 0x00034730, EBX pool; DAT_004DEDA0 loading
std::span<const u8> acquire_animation_resource(AnimationResourceRegistry& registry,
                                               const char* name); // Xbox 0x00035E90
std::span<const u8> acquire_named_animation_resource(AnimationResourceRegistry& registry,
    const char* name, ResourceErrorState& error); // Xbox 0x00035B60
void release_animation_resource(AnimationResourceRegistry& registry,
                                 std::span<const u8> resource); // Xbox 0x00035C00
void retain_animation_resource(AnimationResourceRegistry& registry,
    std::span<const u8> resource); // Xbox 0x00035BA0
bool unload_animation_resource(AnimationResourceRegistry& registry,
    std::span<const u8> resource); // Xbox 0x00035C60, EAX the resource: deferred unless a flush is running
void flush_animation_resource_unloads(AnimationResourceRegistry& registry); // Xbox 0x00036180
bool unload_unreferenced_animation_resources(AnimationResourceRegistry& registry); // Xbox 0x00035D30
bool animation_resource_held(AnimationResourceRegistry& registry, std::span<const u8> resource,
    u32& referenced); // Xbox 0x00036210, EAX the resource

void blend_animation_root(AnimationRoot& output, const AnimationRoot* first,
                          const AnimationRoot* second, float fraction); // Xbox 0x000312C0
void blend_animation_joints(std::span<JointPose> output, std::span<const JointPose> first,
                            std::span<const JointPose> second, float fraction); // Xbox 0x0002CE70
void blend_animation_masked(std::span<JointPose> output, std::span<const JointPose> first,
                             std::span<const JointPose> second, std::span<const u8> second_flags,
                             float fraction); // Xbox 0x0002D1A0
void blend_animation_additive(std::span<JointPose> output, std::span<const JointPose> first,
                               std::span<const JointPose> second, std::span<const u8> first_flags,
                               std::span<const u8> second_flags, float fraction); // Xbox 0x0002CFB0
using AnimationAuxiliaryCallback = long double (*)(bool, bool, float, float, float);
long double mix_auxiliary_linear(bool first_present, bool second_present, float first,
                                 float second, float fraction); // Xbox 0x0002D6E0
long double mix_auxiliary_additive(bool first_present, bool second_present, float first,
                                   float second, float fraction); // Xbox 0x0002D700
long double mix_auxiliary_masked(bool first_present, bool second_present, float first,
                                 float second, float fraction); // Xbox 0x0002D740
void mix_animation_auxiliary(AnimationPose& output, const MinaPlayback& first_player,
                              const AnimationPose& first, const MinaPlayback& second_player,
                              const AnimationPose& second, float fraction,
                              AnimationAuxiliaryCallback callback); // Xbox 0x00031950
void mix_animation_linear(AnimationPose& output, AnimationRoot* output_root,
                           const MinaPlayback& first_player, const AnimationPose& first,
                           const AnimationRoot* first_root, const MinaPlayback& second_player,
                           const AnimationPose& second, const AnimationRoot* second_root,
                           float fraction); // Xbox 0x00031FC0

// Xbox 0x0002D450. The 49th auxiliary channel is retained by the output.
void blend_animation_pose(AnimationPose& output, const AnimationPose& first,
                          const AnimationPose& second, float fraction);
// Xbox 0x00031760. MINA offsets are resolved against the resource span.
void sample_mina_auxiliary(const MinaPlayback& player, std::array<float, 64>& output);

struct MinaPlayer : MinaPlayback
{
    std::unique_ptr<AnimationPose> pose;      // +0x10
    std::unique_ptr<AnimationPose> old_pose;  // +0xB4
    AnimationRoot root;                    // +0xB8
    AnimationRoot cached_root;             // +0xD4
    AnimationRoot reference_root;          // +0xF0
    float blend_remaining = 0.f;           // +0x110
    float blend_duration = 0.25f;          // +0x114, initialized by 0x0002D780
    float step = 0.f;                      // +0x118
    u32 ticks = 0;                         // +0x11C
    float age = 0.f;                       // +0x120
    bool sampled = false;                  // +0x124
    u8 paused = 0;                         // +0x125, retail increments this byte
    bool owned_resource = false;           // +0x126
    std::string resource_name;             // +0x2C
    AnimationResourceRegistry* resource_registry = nullptr; // host mapping of the global registry
};

std::unique_ptr<MinaPlayer> make_mina_player(); // Xbox 0x0002D780
void release_mina_player(std::unique_ptr<MinaPlayer>& player,
                          AnimationResourceRegistry& registry); // Xbox 0x0002D850

// Xbox 0x0002DBB0, for the cursor's linear (+0x2B == 0) sampler.
void tick_mina_player(MinaPlayer& player, float dt, bool skip_pose);
// Xbox 0x0002D8E0: borrowed resource (EAX == 0), as called by 0x00030880.
bool start_mina_player(MinaPlayer& player, std::span<const u8> mina, float position,
                         float duration, u8 mode, i16 direction, const char* name = nullptr);
bool start_named_mina_player(MinaPlayer& player, AnimationResourceRegistry& registry,
    const char* name, float position, float duration, u8 mode, i16 direction,
    ResourceErrorState& error); // Xbox 0x0002DB50
// Xbox 0x00031A30 and 0x00031AC0. Resource-relative directories stay in mina.
void initialize_mina_playback(MinaPlayback& player, std::span<const u8> mina,
                              float position, float rate_factor, u8 mode, u8 sampling_mode);
void initialize_mina_playback(MinaPlayback& player, std::span<const u8> mina,
                              float position, u8 mode, u8 sampling_mode);

struct AnimationRecord
{
    std::string_view name;
    std::string_view animation;
    std::string_view events;
    float duration;          // +0x104
    float position;          // +0x108
    float old_pose_duration; // +0x10C
    float fade_duration;     // +0x110
    std::int8_t mode;        // +0x114
    std::int8_t priority;    // +0x115
    std::int8_t blend_mode;  // +0x116
    std::int8_t slot;        // +0x117
};

struct AnimationAlias
{
    std::string_view name;
    std::string_view target;
};

struct AnimationTable
{
    std::span<const AnimationRecord> records;
    std::span<const AnimationAlias> aliases;
};

struct AnimationMix
{
    float duration = 0.f;          // +4
    float weight = -10.f;          // +8, DAT_0032B1FC
    int blend_mode = 0x21;         // +0xC
    float fade_duration = 0.2f;    // +0x10
    float position = 0.f;          // +0x14
    i16 mode = 0;                 // +0x18
    int priority = 0x31;           // +0x1C
    float old_pose_duration = 0.2f;// +0x20
    std::string_view events;       // +0x24
    std::span<const u8> borrowed_animation; // +0xA4
    std::span<const u8> borrowed_events; // +0xA8
    const AnimationRecord* record = nullptr; // +0xAC
};

struct AnimationControllerSlot
{
    std::unique_ptr<MinaPlayer> player; // +0
    std::span<const u8> events;         // +4
    bool borrowed_events = false;      // +8
    int priority = 0;                  // +0xC
    int blend_mode = 0;                // +0x10
    float fade_fraction = 0.f;         // +0x14
    int fade_mode = 0;                 // +0x18
    float fade_duration = 0.f;         // +0x1C
    float weight = 0.f;                // +0x20
    const AnimationRecord* record = nullptr; // +0x24, stride 0x28
};

struct AnimationController
{
    std::function<void(std::span<const u8>, u32, float, void*, int)> event_callback; // +0
    void* event_owner = nullptr;           // +4
    MinaPlayback combined_player;          // +8
    std::unique_ptr<AnimationPose> combined_pose; // embedded player +0x10
    AnimationRoot combined_root;           // +0x34
    MinaPlayback* selected_player = nullptr; // +0x50
    AnimationPose* selected_pose = nullptr; // selected player +0x10, host pointer mapping
    AnimationRoot* selected_root = nullptr; // +0x54
    std::vector<AnimationControllerSlot> slots; // +0x58
    bool dirty = false;                    // +0x5C
    bool idle = false;                     // +0x60
    u16 slot_count = 0;                    // +0x64
    u16 joint_capacity = 0;                // +0x66
};

struct ActorAnimationMessage
{
    int id; // 7, message +0
    float position; // +4
    u32 slot; // +8
    std::span<const u8> event; // +0xC
    int phase; // +0x10
};
struct AnimationCallbackArray
{
    u16 stride = 0; // +0
    std::vector<u8> bytes; // allocation +4
    u32 used = 0; // end +8 minus allocation +4
};
struct AnimationCallbackPool
{
    std::array<std::unique_ptr<AnimationCallbackArray>, 3> arrays; // +0, +4, +8
    u32 count = 0; // +0xC
};
void remove_animation_callback(AnimationCallbackPool& pool, u16 id); // Xbox 0x0002F570
void release_animation_callback_pool(std::unique_ptr<AnimationCallbackPool>& pool); // Xbox 0x0002F2D0
struct ActorAnimationState
{
    const AnimationTable* table = nullptr; // actor +0x48
    std::span<const u8> table_resource;
    int table_owned = 0; // +0x4C
    std::unique_ptr<AnimationController> controller; // +0x50
    std::unique_ptr<AnimationCallbackPool> pose_callbacks; // +0x54
    AnimationCallbackPool* callback_owner = nullptr; // +0x5C
    u8 callback_flags = 0; // +0x60
    u16 callback_id = 0; // +0x64
    int retain_position = 0; // +0x84
    std::function<void(const ActorAnimationMessage&)> message_handler; // actor vtable +0xC
};
void clear_actor_animation(ActorAnimationState& actor, ModelAnimationState* model,
    AnimationResourceRegistry& registry); // Xbox 0x0003AC20
struct AnimationTableResourceViews
{
    std::unordered_map<const u8*, const AnimationTable*> tables{{nullptr, nullptr}};
};
void set_actor_animation_table(ActorAnimationState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const AnimationTableResourceViews& views); // Xbox 0x0003AA30
void animation_actor_event(std::span<const u8> event, u32 slot, float position,
    ActorAnimationState& actor, int phase); // Xbox 0x0003B3C0
AnimationController* ensure_actor_animation_controller(ActorAnimationState& actor); // Xbox 0x0003ACB0
void start_controller_animation_mix(AnimationController& controller, u32 slot,
    const AnimationMix& mix, const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state); // Xbox 0x00030B10
void start_actor_animation_mix(ActorAnimationState& actor, u32 slot, const AnimationMix& mix,
    const char* name, AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state); // Xbox 0x0003ADA0
void start_actor_animation(ActorAnimationState& actor, const AnimationRecord& record,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state); // Xbox 0x0003A9E0
void set_actor_animation_position(ActorAnimationState& actor, u32 slot, float position); // Xbox 0x0003B2E0
bool actor_animation_completed(const ActorAnimationState& actor, u32 slot); // Xbox 0x0003B290
bool start_actor_initial_animation(ActorAnimationState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state); // Xbox 0x0003ACE0

std::unique_ptr<AnimationController> make_animation_controller(u32 slot_count); // Xbox 0x00030740
void release_animation_controller(std::unique_ptr<AnimationController>& controller,
    bool send_end, AnimationResourceRegistry& registry); // Xbox 0x000307C0
bool start_controller_animation(AnimationController& controller, u32 slot,
    std::span<const u8> resource, const char* name, const AnimationMix& mix,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state); // Xbox 0x00030880
void clear_animation_controller_slot(AnimationController& controller, u32 slot, bool send_end,
                                       AnimationResourceRegistry& registry); // Xbox 0x00031240
bool tick_animation_controller_slot(AnimationController& controller, u32 slot, float dt, bool skip_pose,
                                      AnimationResourceRegistry& registry); // Xbox 0x00030E30
bool tick_animation_controller(AnimationController& controller, float dt, bool skip_pose,
                                 AnimationResourceRegistry& registry); // Xbox 0x00030BA0
void combine_animation_controller(AnimationController& controller); // Xbox 0x000310F0
void mix_controller_poses(int mode, AnimationPose& output, AnimationRoot* output_root,
                           const MinaPlayback& first_player, const AnimationPose& first,
                           const AnimationRoot* first_root, const MinaPlayback& second_player,
                           const AnimationPose& second, const AnimationRoot* second_root,
                           float fraction); // Xbox 0x00031040

using AnimationMatrix = std::array<float, 12>;
struct ModelPoseBuffer
{
    int count = 0; // +0
    bool dirty = false; // +4
    std::vector<AnimationMatrix> matrices; // +0x10
    std::vector<std::array<float, 16>> packed_matrices; // +0x10 when +4 is zero
};
void multiply_affine_matrices(float out[12], const float a[12], const float b[12]); // Xbox 0x001041B0
const AnimationMatrix& model_pose_matrix(const ModelPoseBuffer& pose, int joint); // Xbox 0x00031530
const AnimationMatrix& model_joint_world_matrix(const ModelPoseBuffer& pose, int index,
    const Joint& joint, const AnimationMatrix& world); // Xbox 0x000316B0
struct ModelBoneBinding
{
    u16 kind = 0;
    i16 joint = -1;
    AnimationMatrix matrix{};
};
void model_bone_world_matrix(const ModelBoneBinding& binding, const ModelPoseBuffer* pose,
    const AnimationMatrix& world, AnimationMatrix& output); // Xbox 0x00051E40
void model_bone_world_point(const ModelBoneBinding& binding, const ModelPoseBuffer* pose,
    const AnimationMatrix* world, Vec3& output); // Xbox 0x00051CE0
struct ModelResourceBlock
{
    u32 type;
    u32 value;
};
struct ModelAnimationState
{
    ModelPoseBuffer* primary = nullptr; // model instance +0x164
    ModelPoseBuffer* alternate = nullptr; // +0x168
    int joint_count = 0; // skeleton +0x20
    std::array<float*, 48> auxiliary_targets{}; // model instance +0x1CC
    std::span<const Joint> skeleton; // model instance +0x1BC, skeleton joints +0x24
    std::span<const ModelResourceBlock> resource_blocks; // CPU runtime +0 / +4
    std::array<u8, 16> node_flags{}; // model instance +0x294, two bits per node
};
using ModelNodeFlagCache = std::array<u32, 64>; // DAT_005078D8
int model_animation_joint_count(const ModelAnimationState& model); // Xbox 0x000F9760 / 0x00156A20
void set_model_node_animation(ModelAnimationState& model, u8 node, u8 value,
                                ModelNodeFlagCache& cache); // Xbox 0x000F96F0
void apply_model_node_animation(ModelAnimationState& model, u32 values,
                                  ModelNodeFlagCache& cache); // Xbox 0x000320F0
void initialize_model_pose(ModelPoseBuffer& output); // Xbox 0x000314C0
void reset_model_pose(ModelAnimationState& output, const AnimationMatrix& matrix); // Xbox 0x00030F90
void write_model_auxiliary(ModelAnimationState& output, std::span<const float, 48> values); // Xbox 0x0002CD80
void clear_model_auxiliary(ModelAnimationState& output); // Xbox 0x0002CE00
using SkeletonCallback = std::function<void(const AnimationMatrix&, const AnimationMatrix&,
                                             AnimationMatrix&, size_t, void*, void*)>;
struct SkeletonCallbackRecord
{
    u16 joint = 0xffff;
    SkeletonCallback callback;
};
struct SkeletonDispatchContext
{
    bool enabled; // actor +0x54 record, +0xC
    void* first_context; // context +0, then +4
    std::span<const SkeletonCallbackRecord> callbacks; // context +4, then +4
};
// The callback list ends with joint 0xFFFF, as in the runtime skeleton record.
void evaluate_mina_skeleton(const MinaPlayback& player, const AnimationPose& pose,
                             std::span<const Joint> joints, std::span<AnimationMatrix> output,
                             std::span<const SkeletonCallbackRecord> callbacks,
                             void* first_context, void* second_context); // Xbox 0x0002FAC0
void evaluate_mina_skeleton_plain(const MinaPlayback& player, AnimationPose& pose,
                                   std::span<const Joint> joints,
                                   std::span<AnimationMatrix> output); // Xbox 0x00030100
void dispatch_mina_skeleton(const SkeletonDispatchContext* context, const MinaPlayback& player,
    AnimationPose& pose, std::span<const Joint> joints, std::span<AnimationMatrix> output,
    void* second_context); // Xbox 0x00030FF0
void apply_controller_to_model(AnimationController& controller, ModelAnimationState& model,
    bool skip_pose, const SkeletonDispatchContext* context, ModelNodeFlagCache& cache,
    const AnimationMatrix& reset_matrix); // Xbox 0x00030C70
void subtract_skeleton_bind_points(std::span<AnimationMatrix> output,
                                    std::span<const Joint> joints, int count); // Xbox 0x0002F720

const AnimationRecord* find_animation_record(const AnimationTable& table, std::string_view name); // Xbox 0x00032180
void append_animation_number(std::string& output, i32 value); // Xbox 0x000FBEA0
const AnimationAlias* find_animation_alias(const AnimationTable& table, std::string_view name); // Xbox 0x00032210
const AnimationRecord* find_animation(const AnimationTable& table, std::string_view name); // Xbox 0x00032150
const AnimationRecord* find_actor_animation(const AnimationTable* table, const char* name,
                                            const char* variant, u32& random_state); // Xbox 0x0003AAA0
AnimationMix animation_mix(const AnimationRecord& record, bool retain_position); // Xbox 0x000322A0
const AnimationTable& button_bat_animation_table(); // executable data at 0x0033AC24

// Xbox 0x00031B50 / 0x00031D00. +0x10C is set separately by 0x0002DBB0.
void advance_mina_playback(MinaPlayback& player, float dt, AnimationRoot* root,
                           AnimationRoot* cached_root, bool sampled);
void retime_mina_playback(MinaPlayback* player, float duration, bool& dirty); // Xbox 0x00030DB0
bool mina_playback_completed(const MinaPlayback* player); // Xbox 0x0003B290

// Column-major 4x4 skin palette, one matrix per joint (Xbox stores the same
// transform as a 3x4). Rigid vertices index one slot. 0x2FAC0 concatenates
// parent * local; 0x2F720 then subtracts the joint bind point.
void skin_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, float* out, int max_joints);

// Column-major palette whose frame-0 pose is identity on the mesh. `delta` is the
// translation that puts skeleton bind points onto the vertices (0x2FAC0's bind
// point is not in the same space as the position stream).
void skin_draw_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& rest,
                        const std::vector<JointPose>& pose, Vec3 delta, float* out, int max_joints);

} // namespace gbtg
