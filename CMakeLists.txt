cmake_minimum_required(VERSION 3.20)
project(GbtGDecomp LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

# Solution folders organize projects; source_group below organizes their files.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "Tools")

# Enforce the project header convention without inspecting fetched dependencies.
file(GLOB_RECURSE GBTG_FORBIDDEN_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
)
if(GBTG_FORBIDDEN_HEADERS)
    message(FATAL_ERROR "Project-owned headers must use .h: ${GBTG_FORBIDDEN_HEADERS}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio compiles a project's sources serially without /MP. Ninja schedules
# its own jobs and would only warn about the flag.
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/MP>)
endif()

include(FetchContent)

# Build directories share one copy of each fetched source tree when it has been
# seeded under cache/_deps; each directory still builds it out of its own _deps.
foreach(dep zlib imgui ffmpeg_lgpl)
    string(TOUPPER "${dep}" _gbtg_dep_upper)
    set(_gbtg_dep_src "${CMAKE_CURRENT_SOURCE_DIR}/cache/_deps/${dep}-src")
    if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_${_gbtg_dep_upper} AND IS_DIRECTORY "${_gbtg_dep_src}")
        set(FETCHCONTENT_SOURCE_DIR_${_gbtg_dep_upper} "${_gbtg_dep_src}")
    endif()
endforeach()

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(zlib)

option(GBTG_WITH_SDL "Fetch SDL3 for a windowed tick loop (optional)" OFF)

if(GBTG_WITH_SDL)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.20
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_library(gbtg_lib STATIC
    src/gbtg/resources/io.cpp
    src/gbtg/resources/resource.cpp
    src/gbtg/resources/bundle.cpp
    src/gbtg/graphics/texture.cpp
    src/gbtg/models/model.cpp
    src/gbtg/text/language.cpp
    src/gbtg/text/loctext.cpp
    src/gbtg/text/runtime_localtext.cpp
    src/gbtg/text/localtext_resources.cpp
    src/engine/frontend/dialogue_tags.cpp
    src/engine/scene/dialogue_resource_owner.cpp
    src/engine/scene/scene_resource_memory.cpp
    src/engine/scene/scene_actor_services.cpp
    src/engine/scene/scene_actor_activation.cpp
    src/engine/scene/scene_actor_update.cpp
    src/engine/scene/scene_view_actors.cpp
    src/engine/scene/scene_mirror.cpp
    src/engine/scene/scene_view_present.cpp
    src/engine/graphics/light_volume_draw.cpp
    src/engine/graphics/light_volume_records.cpp
    src/engine/graphics/light_cones.cpp
    src/engine/graphics/door_shadow_draw.cpp
    src/engine/graphics/xg_shader_assembly.cpp
    src/engine/graphics/xg_pixel_shader.cpp
    src/engine/entities/body_stand_alert_tick.cpp
    src/engine/entities/body_walk_alert_tick.cpp
    src/engine/entities/body_walk_target_state.cpp
    src/engine/entities/body_hold_state.cpp
    src/engine/entities/body_throw_state.cpp
    src/engine/entities/bone_debris_actor.cpp
    src/engine/entities/body_hit_state.cpp
    src/engine/entities/body_dizzy_state.cpp
    src/engine/entities/body_die_state.cpp
    src/engine/entities/body_anim_state.cpp
    src/engine/scripts/script_event_sequence.cpp
    src/engine/frontend/scoreboard_update.cpp
    src/engine/frontend/scoreboard_draw.cpp
    src/engine/entities/body_knockdown_state.cpp
    src/engine/entities/avatar_motion_blur.cpp
    src/engine/graphics/actor_snapshots.cpp
    src/engine/graphics/glow_pass.cpp
    src/engine/scene/scene_control_update.cpp
    src/engine/scene/scene_controls.cpp
    src/engine/scene/attract_control.cpp
    src/engine/scene/trophy_heads_control.cpp
    src/engine/scene/haunted_chairs_control.cpp
    src/engine/scene/relics_store_control.cpp
    src/engine/scene/scene_fx_control.cpp
    src/engine/scene/chapter2_scene4_control.cpp
    src/engine/scene/background_morphs.cpp
    src/engine/scene/background_cloths.cpp
    src/engine/graphics/background_cloth.cpp
    src/engine/graphics/background_cloth_draw.cpp
    src/engine/scene/scene_point_lights.cpp
    src/engine/graphics/scene_point_light_pass.cpp
    src/engine/scene/scene_light_grid.cpp
    src/engine/scene/super_scary_control.cpp
    src/engine/scene/map_view_control.cpp
    src/engine/scene/gameselect_control.cpp
    src/engine/scene/scene_actor_removal.cpp
    src/engine/scene/ghoul_intro.cpp
    src/engine/scene/scene_shadow_casters.cpp
    src/engine/scene/scene_gameplay_time.cpp
    src/engine/scene/scene_body_messages.cpp
    src/engine/scene/scene_mind_messages.cpp
    src/engine/entities/body_move_actions.cpp
    src/engine/entities/body_state_actions.cpp
    src/engine/entities/body_pickup_state.cpp
    src/engine/entities/camera_actor_classes.cpp
    src/engine/entities/camera_director.cpp
    src/engine/entities/body_move_action_slots.cpp
    src/engine/entities/body_gravity_motion.cpp
    src/engine/camera/camera_mind_rig.cpp
    src/engine/camera/camera_mind_requests.cpp
    src/engine/camera/camera_body_slots.cpp
    src/engine/entities/actor_callout_resources.cpp
    src/engine/scene/executable_resources.cpp
    src/engine/scene/factory_scene_runtime.cpp
    src/engine/scene/scene_markers.cpp
    src/engine/scene/scene_marker_handlers.cpp
    src/engine/entities/player_spawn.cpp
    src/engine/entities/area_actor.cpp
    src/engine/entities/actor_model_pieces.cpp
    src/engine/entities/weapon_actor.cpp
    src/engine/entities/weapon_motion.cpp
    src/engine/entities/weapon_breakage.cpp
    src/engine/entities/trigger_actor.cpp
    src/engine/entities/scene_actor_factory_owner.cpp
    src/engine/entities/actor_word_messages.cpp
    src/engine/entities/cinema_mind_lifecycle.cpp
    src/engine/entities/cinema_mind_requests.cpp
    src/engine/entities/cinema_mind_control.cpp
    src/engine/entities/mind_target_policy.cpp
    src/engine/entities/mind_action_requests.cpp
    src/engine/entities/body_orientation.cpp
    src/engine/entities/cinema_mind_messages.cpp
    src/engine/entities/cinema_mind_tick.cpp
    src/engine/entities/cinema_mind_movement.cpp
    src/engine/entities/cinema_mind_request_update.cpp
    src/engine/navigation/mind_vertical_navigation.cpp
    src/engine/navigation/mind_escape_position.cpp
    src/engine/navigation/mind_navigation_angles.cpp
    src/engine/navigation/mind_navigation_costs.cpp
    src/engine/entities/player_children.cpp
    src/engine/entities/player_mind_setup.cpp
    src/engine/entities/player_parent_lifecycle.cpp
    src/engine/entities/player_attributes.cpp
    src/engine/entities/player_mind_goals.cpp
    src/engine/entities/player_mind_events.cpp
    src/engine/entities/player_mind_feedback.cpp
    src/engine/entities/player_mind_requests.cpp
    src/engine/entities/player_mind_control.cpp
    src/engine/entities/player_input_binding.cpp
    src/engine/entities/player_mind_targets.cpp
    src/engine/entities/player_mind_target_control.cpp
    src/engine/entities/player_strategy_entry.cpp
    src/engine/entities/player_attachment_helpers.cpp
    src/engine/entities/player_attachment.cpp
    src/engine/entities/player_construction.cpp
    src/engine/entities/player_goal_cleanup.cpp
    src/engine/entities/player_destruction.cpp
    src/engine/navigation/cell_registration.cpp
    src/engine/navigation/cell_reach.cpp
    src/engine/navigation/grid_preparation.cpp
    src/engine/navigation/model_grid.cpp
    src/engine/entities/player_messages.cpp
    src/engine/entities/player_overlay.cpp
    src/engine/graphics/billboard_queue.cpp
    src/engine/graphics/world_sprite_passes.cpp
    src/engine/navigation/floor_tracking.cpp
    src/engine/navigation/path_routes.cpp
    src/engine/navigation/path_search.cpp
    src/engine/navigation/floor_connections.cpp
    src/engine/navigation/player_navigation.cpp
    src/engine/navigation/player_avoidance.cpp
    src/engine/navigation/player_navigation_control.cpp
    src/engine/entities/body_collision.cpp
    src/engine/entities/segment_records.cpp
    src/engine/entities/body_contacts.cpp
    src/engine/entities/contact_prop_actor.cpp
    src/engine/entities/furnace_actor.cpp
    src/engine/entities/player_update.cpp
    src/engine/entities/player_strategy_goals.cpp
    src/engine/entities/player_strategy.cpp
    src/engine/entities/strategy_target_queue.cpp
    src/engine/entities/strategy_target_selection.cpp
    src/engine/entities/butler_strategy.cpp
    src/engine/entities/body_struggle_state.cpp
    src/engine/entities/body_player_death_state.cpp
    src/engine/entities/body_grabbed_state.cpp
    src/engine/entities/imp_fire_strategy.cpp
    src/engine/entities/skeleton_strategy.cpp
    src/engine/entities/zombie_strategy.cpp
    src/engine/entities/spider_strategy.cpp
    src/engine/entities/ugly_girl_strategy.cpp
    src/engine/entities/ribs_strategy.cpp
    src/engine/entities/giant_hand_strategy.cpp
    src/engine/entities/haunted_coat_strategy.cpp
    src/engine/entities/haunted_picture_strategy.cpp
    src/engine/entities/body_spawn_ghouly_state.cpp
    src/engine/entities/body_yoyo_spin_state.cpp
    src/engine/entities/haunted_door_strategy.cpp
    src/engine/entities/imp_flamethrower_strategy.cpp
    src/engine/entities/body_transform_state.cpp
    src/engine/entities/body_furnace_hit_state.cpp
    src/engine/entities/chicken_strategy.cpp
    src/engine/entities/imp_flying_strategy.cpp
    src/engine/entities/imp_ninja_strategy.cpp
    src/engine/entities/jessie_clyde_strategy.cpp
    src/engine/entities/single_shot_weapon.cpp
    src/engine/entities/vampire_chicken_strategy.cpp
    src/engine/entities/medusa_strategy.cpp
    src/engine/entities/pirate_strategy.cpp
    src/engine/entities/hunchback_strategy.cpp
    src/engine/entities/ghouly_key_actor.cpp
    src/engine/entities/water_pistol_weapon.cpp
    src/engine/entities/body_death_state.cpp
    src/engine/entities/scare_figure.cpp
    src/engine/entities/super_scary_shock.cpp
    src/engine/entities/spider_web.cpp
    src/engine/entities/grim_reaper_strategy.cpp
    src/engine/graphics/blob_shadows.cpp
    src/engine/entities/projectile_weapon.cpp
    src/engine/entities/thrown_projectile.cpp
    src/engine/entities/bob_weapon.cpp
    src/engine/entities/ghouly_cutscene_setup.cpp
    src/engine/entities/ghouly_bg_break_setup.cpp
    src/engine/entities/ghouly_swap_setup.cpp
    src/engine/entities/bg_break_key.cpp
    src/engine/entities/body_arc_state.cpp
    src/engine/entities/body_arc_attack_state.cpp
    src/engine/graphics/shader_parameters.cpp
    src/engine/entities/haunted_tv_strategy.cpp
    src/engine/entities/haunted_chair_strategy.cpp
    src/engine/entities/mummy_strategy.cpp
    src/engine/entities/candle_weapon.cpp
    src/engine/entities/container_weapon.cpp
    src/engine/entities/gun_weapon.cpp
    src/engine/entities/extinguisher_weapon.cpp
    src/engine/entities/fx_custom_electric.cpp
    src/engine/entities/generate_ghouly_state.cpp
    src/engine/entities/body_shocked_state.cpp
    src/engine/entities/body_scrabble_state.cpp
    src/engine/entities/body_scoop_whack_state.cpp
    src/engine/entities/body_flip_state.cpp
    src/engine/entities/body_hover_state.cpp
    src/engine/entities/body_jump_on_state.cpp
    src/engine/entities/body_walk_state.cpp
    src/engine/entities/body_flamethrower_state.cpp
    src/engine/entities/body_shoot_state.cpp
    src/engine/entities/body_hit_stream_state.cpp
    src/engine/entities/body_endure_state.cpp
    src/engine/entities/body_stunned_state.cpp
    src/engine/entities/body_spin_dizzy_state.cpp
    src/engine/entities/body_drone_dance_state.cpp
    src/engine/entities/sss_dropper.cpp
    src/engine/entities/sss_door.cpp
    src/engine/entities/body_look_for_weapon_state.cpp
    src/engine/entities/body_slide_state.cpp
    src/engine/entities/animated_light.cpp
    src/engine/entities/body_swing_state.cpp
    src/engine/entities/actor_runtime_queries.cpp
    src/engine/entities/actor_view_visibility.cpp
    src/engine/entities/fx_anim_emitter.cpp
    src/engine/entities/powerup_awards.cpp
    src/engine/entities/intro_notices.cpp
    src/engine/entities/player_mind_messages.cpp
    src/engine/entities/player_mind_request_update.cpp
    src/engine/entities/player_mind_owner.cpp
    src/engine/entities/player_factory_owner.cpp
    src/engine/camera/camera_pulse.cpp
    src/engine/camera/view_frustum.cpp
    src/engine/frontend/brew_meter.cpp
    src/engine/entities/player_state.cpp
    src/engine/entities/body_motion_controller.cpp
    src/engine/entities/body_health.cpp
    src/engine/entities/body_resources.cpp
    src/engine/entities/body_animation_setup.cpp
    src/engine/entities/body_action_selection.cpp
    src/engine/entities/body_attachment_cleanup.cpp
    src/engine/entities/body_construction.cpp
    src/engine/entities/player_body_motion.cpp
    src/engine/entities/body_channels.cpp
    src/engine/entities/body_timers.cpp
    src/engine/entities/body_timer_requests.cpp
    src/engine/entities/body_wash.cpp
    src/engine/entities/actor_shape_bounds.cpp
    src/engine/entities/actor_collision_culling.cpp
    src/engine/entities/model_sphere_collision.cpp
    src/engine/entities/collision_shape_dispatch.cpp
    src/engine/entities/physical_mesh_queries.cpp
    src/engine/entities/mesh_sphere_geometry.cpp
    src/engine/entities/actor_collision_queries.cpp
    src/engine/entities/actor_shape_push.cpp
    src/engine/graphics/vector_normalize.cpp
    src/engine/graphics/d3dx_matrix.cpp
    src/engine/graphics/d3dx_view.cpp
    src/engine/entities/body_apply.cpp
    src/engine/entities/body_message_helpers.cpp
    src/engine/entities/body_shape_push.cpp
    src/engine/entities/contact_sweep.cpp
    src/engine/entities/player_body_messages.cpp
    src/engine/entities/player_body_collision.cpp
    src/engine/entities/body_action_ids.cpp
    src/engine/entities/body_gaze.cpp
    src/engine/entities/body_attachment_requests.cpp
    src/engine/entities/body_request_selection.cpp
    src/engine/entities/player_control_update.cpp
    src/engine/entities/body_animation_commands.cpp
    src/engine/entities/body_effect_commands.cpp
    src/engine/entities/body_animation_dispatch.cpp
    src/engine/entities/body_message_dispatch.cpp
    src/engine/entities/body_damage_notices.cpp
    src/engine/entities/body_damage_tables.cpp
    src/engine/entities/body_damage_motion.cpp
    src/engine/entities/body_spatial_notifications.cpp
    src/engine/entities/body_damage_animation.cpp
    src/engine/entities/body_damage_processors.cpp
    src/engine/entities/body_damage_dispatch.cpp
    src/engine/entities/body_hit_messages.cpp
    src/engine/entities/body_source_contacts.cpp
    src/engine/entities/body_death_effects.cpp
    src/engine/entities/body_death_drops.cpp
    src/engine/entities/ghouly_reference_names.cpp
    src/engine/entities/actor_template_creation.cpp
    src/engine/runtime/reference_names.cpp
    src/engine/navigation/spawn_candidates.cpp
    src/engine/audio/body_sound_commands.cpp
    src/engine/scene/dialogue_scene_runtime.cpp
    src/engine/frontend/dialogue_sound_tags.cpp
    src/engine/frontend/dialogue_page_voice.cpp
    src/engine/frontend/dialogue_widget_lifecycle.cpp
    src/engine/frontend/dialogue_widget_construction.cpp
    src/engine/frontend/dialogue_widget_update.cpp
    src/engine/frontend/dialogue_widget_bat.cpp
    src/engine/frontend/dialogue_tag_actions.cpp
    src/engine/frontend/dialogue_substitutions.cpp
    src/engine/frontend/dialogue_time.cpp
    src/engine/frontend/dialogue_measurement.cpp
    src/engine/frontend/dialogue_page_layout.cpp
    src/engine/frontend/dialogue_page_setup.cpp
    src/engine/frontend/dialogue_resources.cpp
    src/engine/frontend/dialogue_trophies.cpp
    src/engine/frontend/dialogue_widget_parameters.cpp
    src/engine/frontend/dialogue_text_pool.cpp
    src/engine/frontend/dialogue_text_metrics.cpp
    src/engine/frontend/dialogue_widget_state.cpp
    src/engine/frontend/dialogue_input.cpp
    src/engine/frontend/input_activation.cpp
    src/engine/frontend/player_health_display.cpp
    src/engine/frontend/scoreboard_messages.cpp
    src/engine/frontend/scoreboard_hud.cpp
    src/engine/frontend/score_awards.cpp
    src/engine/audio/listener_settings.cpp
    src/engine/audio/native_audio.cpp
    src/gbtg/graphics/font.cpp
    src/gbtg/audio/wavebank.cpp
    src/gbtg/resources/xbe.cpp
    src/gbtg/resources/catalog.cpp
    src/gbtg/scripts/script.cpp
    src/gbtg/scripts/cutscene.cpp
    src/gbtg/animation/anim.cpp
    src/engine/entities/actor_animation_messages.cpp
    src/gbtg/runtime/performance.cpp
    src/gbtg/resources/resource_error.cpp
    src/gbtg/runtime/object_factory.cpp
    src/gbtg/runtime/actor_spawn_factory.cpp
    src/gbtg/models/model_runtime.cpp
    src/gbtg/models/model_render.cpp
    src/gbtg/models/model_relocation.cpp
    src/gbtg/models/model_texture.cpp
    src/gbtg/models/model_resource.cpp
    src/gbtg/models/model_views.cpp
    src/gbtg/audio/sound_runtime.cpp
)
target_include_directories(gbtg_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
# Stable std headers only; see the header for what may go in it.
set(GBTG_PCH "${CMAKE_CURRENT_SOURCE_DIR}/src/pch/gbtg_pch.h")
target_precompile_headers(gbtg_lib PRIVATE "${GBTG_PCH}")
if(TARGET zlibstatic)
    target_link_libraries(gbtg_lib PUBLIC zlibstatic)
else()
    target_link_libraries(gbtg_lib PUBLIC zlib)
endif()

add_executable(gbtg-catalog src/tools/catalog_main.cpp)
target_link_libraries(gbtg-catalog PRIVATE gbtg_lib)

add_executable(gbtg-dump src/tools/dump_main.cpp)
target_link_libraries(gbtg-dump PRIVATE gbtg_lib)

add_executable(gbtg-model-check src/tools/model_check_main.cpp)
target_link_libraries(gbtg-model-check PRIVATE gbtg_lib)

include(CTest)
if(BUILD_TESTING)
    add_executable(gbtg-build-guard-tests tests/inspector_build_guards.cpp)
    target_include_directories(gbtg-build-guard-tests PRIVATE "${CMAKE_SOURCE_DIR}/include")
    add_test(NAME game-excludes-inspector-api COMMAND gbtg-build-guard-tests)
    add_executable(gbtg-model-tests tests/model_tests.cpp)
    target_link_libraries(gbtg-model-tests PRIVATE gbtg_lib)
    add_test(NAME model-render-inputs COMMAND gbtg-model-tests)
    add_executable(gbtg-object-factory-tests tests/object_factory_tests.cpp)
    target_link_libraries(gbtg-object-factory-tests PRIVATE gbtg_lib)
    add_test(NAME object-factory-lifecycle COMMAND gbtg-object-factory-tests)
    add_executable(gbtg-model-runtime-tests tests/model_runtime_tests.cpp)
    target_link_libraries(gbtg-model-runtime-tests PRIVATE gbtg_lib)
    add_test(NAME model-runtime-lookup-copy COMMAND gbtg-model-runtime-tests)
    add_executable(gbtg-model-relocation-tests tests/model_relocation_tests.cpp)
    target_link_libraries(gbtg-model-relocation-tests PRIVATE gbtg_lib)
    add_test(NAME model-resource-relocation COMMAND gbtg-model-relocation-tests)
endif()

add_executable(gbtg-xemu-patch src/tools/xemu_patch_main.cpp)
target_link_libraries(gbtg-xemu-patch PRIVATE gbtg_lib)

add_executable(gbtg
    src/engine/main.cpp
    src/engine/runtime/host_config.cpp
    src/engine/runtime/aid.cpp
    src/engine/entities/entity.cpp
    src/engine/entities/actor_transform.cpp
    src/engine/camera/entity_camera.cpp
    src/engine/entities/entity_collision.cpp
    src/engine/entities/entity_heap.cpp
    src/engine/entities/entity_model.cpp
    src/engine/entities/actor_runtime.cpp
    src/engine/entities/actor_event_messages.cpp
    src/engine/entities/actor_base_messages.cpp
    src/engine/entities/ghouly_box_messages.cpp
    src/engine/entities/ghouly_requests.cpp
    src/engine/navigation/navigation.cpp
    src/engine/navigation/occupancy.cpp
    src/engine/entities/actor_world_lists.cpp
    src/engine/entities/actor_render_registration.cpp
    src/engine/entities/actor_lifecycle.cpp
    src/engine/entities/actor_factory_owner.cpp
    src/engine/entities/actor_model_anchors.cpp
    src/engine/entities/actor_pose_updates.cpp
    src/engine/entities/actor_bind_tree.cpp
    src/engine/entities/actor_live_attachments.cpp
    src/engine/entities/actor_sound_pose.cpp
    src/engine/camera/actor_camera_effects.cpp
    src/engine/camera/view_initialization.cpp
    src/engine/camera/display_initialization.cpp
    src/engine/camera/display_setup_math.cpp
    src/engine/camera/runtime_display_setup.cpp
    src/engine/camera/display_clip_limits.cpp
    src/engine/graphics/actor_effect_nodes.cpp
    src/engine/graphics/actor_effect_draw.cpp
    src/engine/graphics/scene_render_helpers.cpp
    src/engine/graphics/scene_light_upload.cpp
    src/engine/graphics/scene_environment_setup.cpp
    src/engine/graphics/scene_model_draw.cpp
    src/engine/graphics/model_tree_dispatch.cpp
    src/engine/graphics/model_renderer_boot.cpp
    src/engine/graphics/model_skeleton_draw.cpp
    src/engine/graphics/model_matrix_array_draw.cpp
    src/engine/graphics/model_shader_states.cpp
    src/engine/graphics/model_transform_draw.cpp
    src/engine/graphics/shader_optimization_driver.cpp
    src/engine/graphics/model_vertex_shader_cache.cpp
    src/engine/graphics/model_vertex_shader_draw.cpp
    src/engine/graphics/model_shader_parameter_draw.cpp
    src/engine/graphics/model_blendshape_draw.cpp
    src/engine/graphics/screen_model_draw.cpp
    src/engine/entities/button_bat_draw.cpp
    src/engine/graphics/model_push_buffer_draw.cpp
    src/engine/graphics/model_render_nodes.cpp
    src/engine/graphics/model_shader_fragments.cpp
    src/engine/graphics/vertex_shader_code.cpp
    src/engine/graphics/shader_schedule_cost.cpp
    src/engine/graphics/shader_validation_storage.cpp
    src/engine/graphics/shader_pair_optimizer.cpp
    src/engine/graphics/shader_merge_optimizer.cpp
    src/engine/graphics/shader_swizzle.cpp
    src/engine/graphics/shader_merge_pass.cpp
    src/engine/graphics/shader_merge_patterns.cpp
    src/engine/graphics/shader_schedule_issue.cpp
    src/engine/graphics/shader_schedule_init.cpp
    src/engine/graphics/shader_schedule_update.cpp
    src/engine/graphics/shader_schedule_record.cpp
    src/engine/graphics/shader_schedule_hazards.cpp
    src/engine/graphics/shader_schedule_cost_inputs.cpp
    src/engine/graphics/shader_rewrite_context.cpp
    src/engine/runtime/native_cpp_exception.cpp
    src/engine/graphics/shader_register_rewrite.cpp
    src/engine/graphics/shader_reorder.cpp
    src/engine/graphics/shader_rewrite_outputs.cpp
    src/engine/graphics/shader_rewrite_writes.cpp
    src/engine/graphics/shader_rewrite_storage.cpp
    src/engine/graphics/shader_rewrite_program.cpp
    src/engine/graphics/shader_validation_reserved_register.cpp
    src/engine/graphics/shader_validation_cleanup.cpp
    src/engine/graphics/shader_validation_erase.cpp
    src/engine/graphics/shader_validation_operand_tree.cpp
    src/engine/graphics/shader_validation_operand_order.cpp
    src/engine/graphics/shader_validation_constructors.cpp
    src/engine/graphics/shader_validation_serializer.cpp
    src/engine/graphics/shader_validation_accessors.cpp
    src/engine/graphics/shader_validation_channel_ops.cpp
    src/engine/graphics/shader_validation_containers.cpp
    src/engine/graphics/shader_validation_registers.cpp
    src/engine/graphics/shader_validation_instruction_ops.cpp
    src/engine/graphics/shader_validation_tree_instances.cpp
    src/engine/graphics/shader_validation_sets.cpp
    src/engine/graphics/shader_validation_instruction_constructor.cpp
    src/engine/graphics/shader_validation_destinations.cpp
    src/engine/graphics/shader_validation_pass_lifecycle.cpp
    src/engine/graphics/shader_validation_dependencies.cpp
    src/engine/graphics/shader_validation_instruction_lifecycle.cpp
    src/engine/graphics/shader_validation_packed_program.cpp
    src/engine/graphics/shader_validation_optimizer.cpp
    src/engine/graphics/shader_validation_scheduler.cpp
    src/engine/graphics/shader_validation_program.cpp
    src/engine/graphics/shader_validation_owner_sets.cpp
    src/engine/graphics/shader_validation_pass_traversal.cpp
    src/engine/graphics/shader_validation_registration.cpp
    src/engine/graphics/shader_validation_insert.cpp
    src/engine/graphics/shader_validation_balance.cpp
    src/engine/graphics/shader_validation_channels.cpp
    src/engine/graphics/shader_validation_node_init.cpp
    src/engine/graphics/shader_validation_setup.cpp
    src/engine/graphics/shader_validation_arena.cpp
    src/engine/graphics/scene_ambient_sampling.cpp
    src/engine/graphics/scene_environment_matrices.cpp
    src/engine/graphics/actor_effect_textures.cpp
    src/engine/graphics/actor_effect_stream.cpp
    src/engine/entities/spawn_pool_selection.cpp
    src/engine/entities/spawn_queue_gates.cpp
    src/engine/entities/actor_trail_requests.cpp
    src/engine/entities/actor_scare_requests.cpp
    src/engine/entities/aid_spawn_queue.cpp
    src/engine/entities/aid_spawn_resources.cpp
    src/engine/scene/scene_object_requests.cpp
    src/engine/scene/background_geometry.cpp
    src/engine/scene/background_ambient.cpp
    src/engine/scene/background_sectors.cpp
    src/engine/entities/runtime_list.cpp
    src/engine/scene/background_parts.cpp
    src/engine/scene/background_part_removal.cpp
    src/engine/scene/background_part_destruction.cpp
    src/engine/scene/background_messages.cpp
    src/engine/scene/background_class_owner.cpp
    src/engine/frontend/hud_layout.cpp
    src/engine/frontend/dialogue_text_system.cpp
    src/engine/graphics/scene_d3d_device.cpp
    src/engine/graphics/scene_d3d_present.cpp
    src/engine/graphics/xbox_vertex_program.cpp
    src/engine/graphics/xbox_vertex_hlsl.cpp
    src/engine/graphics/render_allocation_profile.cpp
    src/engine/navigation/grid_regions.cpp
    src/engine/scene/background_construction.cpp
    src/engine/runtime/aid_list_lookup.cpp
    src/engine/scene/background_animation.cpp
    src/engine/scene/background_update.cpp
    src/engine/scene/background_lifetime.cpp
    src/engine/entities/model_sound_lifetime.cpp
    src/engine/entities/physical_shape_events.cpp
    src/engine/entities/actor_matrix_sampling.cpp
    src/engine/entities/euler_wrapping.cpp
    src/engine/entities/random_placement.cpp
    src/engine/entities/bounded_channels.cpp
    src/engine/graphics/effect_channels.cpp
    src/engine/graphics/effect_placement.cpp
    src/engine/entities/model_binding_selection.cpp
    src/engine/graphics/effect_binding.cpp
    src/engine/graphics/effect_command_selection.cpp
    src/engine/graphics/effect_collision_response.cpp
    src/engine/entities/direction_pitch.cpp
    src/engine/runtime/random_state.cpp
    src/engine/camera/plane_collision.cpp
    src/engine/graphics/effect_commands.cpp
    src/engine/graphics/effect_notifications.cpp
    src/engine/graphics/effect_debris.cpp
    src/engine/graphics/effect_trails.cpp
    src/engine/graphics/global_effect_tick.cpp
    src/engine/scene/scene_scare_timers.cpp
    src/engine/scene/scene_object_disposal.cpp
    src/engine/graphics/effect_collision_commands.cpp
    src/engine/graphics/effect_simulation.cpp
    src/engine/graphics/effect_tick.cpp
    src/engine/entities/actor_matrix_setup.cpp
    src/engine/graphics/model_points.cpp
    src/engine/graphics/matrix_inverse.cpp
    src/engine/graphics/effect_particles.cpp
    src/engine/graphics/point_graph.cpp
    src/engine/graphics/point_graph_solver.cpp
    src/engine/graphics/pose_matrix.cpp
    src/engine/scene/background_doors.cpp
    src/engine/cutscenes/door_registration.cpp
    src/engine/scene/background_volumes.cpp
    src/engine/entities/shape_copy.cpp
    src/engine/scene/background_model_nodes.cpp
    src/engine/scene/live_spawn_tick.cpp
    src/engine/audio/bank_lookup.cpp
    src/engine/audio/bank_streams.cpp
    src/engine/audio/stream_restart.cpp
    src/engine/audio/core_listener.cpp
    src/engine/audio/sound_names.cpp
    src/engine/audio/footstep_names.cpp
    src/engine/entities/actor_attachment_requests.cpp
    src/engine/entities/actor_attachment_updates.cpp
    src/engine/entities/actor_frame_update.cpp
    src/engine/entities/actor_frame_render.cpp
    src/engine/entities/button_bat_update.cpp
    src/engine/entities/actor_attachment_events.cpp
    src/engine/audio/resource_requests.cpp
    src/engine/runtime/name_lookup.cpp
    src/engine/entities/actor_lookup.cpp
    src/engine/entities/actor_spawn_eligibility.cpp
    src/engine/entities/shape_transform.cpp
    src/engine/entities/actor_spawn_query.cpp
    src/engine/entities/ghouly_box_controller.cpp
    src/engine/entities/ghouly_spawn_record.cpp
    src/engine/entities/ghouly_spawn_scheduler.cpp
    src/engine/entities/ghouly_spawn_trigger.cpp
    src/engine/entities/ghouly_box_decoder.cpp
    src/engine/input/controller.cpp
    src/engine/saves/save_names.cpp
    src/engine/saves/runtime_profile.cpp
    src/engine/saves/save_summary.cpp
    src/engine/saves/save_root.cpp
    src/engine/saves/xdk_saves.cpp
    src/engine/saves/save_games.cpp
    src/engine/frontend/game_names.cpp
    src/engine/graphics/texture_xpr.cpp
    src/engine/transitions/book_map.cpp
    src/engine/transitions/native_book.cpp
    src/engine/frontend/gameselect_contents.cpp
    src/engine/frontend/gameselect_delete.cpp
    src/engine/frontend/gameselect_features.cpp
    src/engine/frontend/gameselect_scene_replay.cpp
    src/engine/frontend/gameselect_progress.cpp
    src/engine/frontend/button_bat_menu.cpp
    src/engine/frontend/input_stack.cpp
    src/engine/frontend/text_tokens.cpp
    src/engine/scene/lighting.cpp
    src/engine/scene/ambient.cpp
    src/engine/scene/scene_resources.cpp
    src/engine/scene/actor_loading.cpp
    src/engine/runtime/clock.cpp
    src/engine/audio/pcm_out.cpp
    src/engine/audio/core_memory.cpp
    src/engine/audio/core_source.cpp
    src/engine/audio/core_cue.cpp
    src/engine/audio/event_voices.cpp
    src/engine/audio/actor_sound_tick.cpp
    src/engine/frontend/frontend_sounds.cpp
    src/engine/scripts/script_chapter.cpp
    src/engine/graphics/color_blend.cpp
    src/engine/frontend/gameselect_rows.cpp
    src/engine/frontend/gameselect_slots.cpp
    src/engine/frontend/gameselect_naming.cpp
    src/engine/frontend/virtual_keyboard.cpp
    src/engine/frontend/frontend_state.cpp
    src/engine/scene/scene.cpp
    src/engine/scene/scene_pause.cpp
    src/engine/scene/scene_loading.cpp
    src/engine/cutscenes/curve_sampling.cpp
    src/engine/cutscenes/instance_construction.cpp
    src/engine/cutscenes/quaternion_math.cpp
    src/engine/cutscenes/camera_sampling.cpp
    src/engine/cutscenes/camera_setup.cpp
    src/engine/cutscenes/actor_tracks.cpp
    src/engine/cutscenes/event_pause.cpp
    src/engine/cutscenes/event_audio.cpp
    src/engine/cutscenes/event_timing.cpp
    src/engine/cutscenes/event_binding.cpp
    src/engine/cutscenes/event_assets.cpp
    src/engine/cutscenes/event_runtime.cpp
    src/engine/frontend/dialogue_activation.cpp
    src/engine/frontend/dialogue_draw_state.cpp
    src/engine/frontend/dialogue_draw_quads.cpp
    src/engine/frontend/dialogue_draw_text.cpp
    src/engine/frontend/dialogue_draw_portraits.cpp
    src/engine/frontend/dialogue_draw_page.cpp
    src/engine/frontend/dialogue_draw_outer.cpp
    src/engine/camera/cutscene_impulse.cpp
    src/engine/runtime/interpolation.cpp
    src/engine/camera/camera.cpp
    src/engine/scripts/script_samples.cpp
    src/engine/scripts/script_table_lookup.cpp
    src/engine/scripts/script_conditions.cpp
    src/engine/scripts/script_event_tallies.cpp
    src/engine/scripts/script_actions.cpp
    src/engine/scripts/script_actor_query.cpp
    src/engine/scripts/script_dialogue_actions.cpp
    src/engine/scripts/script_vm.cpp
    src/engine/transitions/scene_transitions.cpp
    src/engine/runtime/crt_runtime.cpp
    src/engine/runtime/crt_format.cpp
    src/engine/runtime/crt_strings.cpp
    src/engine/runtime/crt_sort.cpp
    src/engine/runtime/crt_mantissa.cpp
    src/engine/runtime/crt_decimal.cpp
    src/engine/runtime/file_worker.cpp
    src/engine/runtime/xapi_runtime.cpp
    src/engine/graphics/d3d_packets.cpp
    src/engine/runtime/engine_memory.cpp
    src/engine/game.cpp
    src/engine/runtime/platform.cpp
    src/engine/video/movie.cpp
    src/engine/video/movie_session.cpp
    src/engine/video/native_movies.cpp
    src/engine/scene/cutscene_scene_runtime.cpp
    src/engine/scene/cutscene_resource_views.cpp
)
target_link_libraries(gbtg PRIVATE gbtg_lib)
if(WIN32)
    configure_file(src/engine/gbtg.rc.in "${CMAKE_CURRENT_BINARY_DIR}/gbtg.rc" @ONLY)
    target_sources(gbtg PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/gbtg.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/gbtg.rc" APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/gbtg.ico")
    # Controller polling belongs to the platform loop with either graphics backend.
    target_link_libraries(gbtg PRIVATE xinput)
    set(_gbtg_gfx_default d3d11)
else()
    set(_gbtg_gfx_default null)
endif()
set(GBTG_GFX "${_gbtg_gfx_default}" CACHE STRING "Graphics backend: null or d3d11")
if(GBTG_GFX STREQUAL "d3d11")
    target_sources(gbtg PRIVATE src/engine/graphics/gfx_d3d11.cpp)
    target_compile_definitions(gbtg PRIVATE GBTG_GFX_D3D11)
    target_link_libraries(gbtg PRIVATE d3d11 dxgi d3dcompiler user32 xaudio2 ole32)

    # Dear ImGui is compiled into every D3D11 gbtg configuration, which defines
    # GBTG_DEBUG_UI_AVAILABLE. GBTG_DEBUG_UI_ENABLED in include/gbtg/debug_ui.h turns the
    # debug UI on or off. The null backend never sees it. Populate skips add_subdirectory:
    # upstream has no library target that includes the Win32 and D3D11 backends.
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b
        GIT_SHALLOW TRUE
    )
    # CMP0169 wants MakeAvailable, which add_subdirectory's a CMakeLists.txt this
    # tag does not have. Populate only downloads the sources we compile ourselves.
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
    set(GBTG_IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
        "${CMAKE_SOURCE_DIR}/src/engine/graphics/debug_ui.cpp"
    )
    target_sources(gbtg PRIVATE ${GBTG_IMGUI_SOURCES})
    target_compile_definitions(gbtg PRIVATE GBTG_DEBUG_UI_AVAILABLE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
    target_include_directories(gbtg PRIVATE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    target_link_libraries(gbtg PRIVATE imm32)
else()
    target_sources(gbtg PRIVATE src/engine/graphics/gfx_null.cpp)
    target_compile_definitions(gbtg PRIVATE GBTG_GFX_NULL)
endif()
if(GBTG_WITH_SDL)
    target_compile_definitions(gbtg PRIVATE GBTG_WITH_SDL)
    target_link_libraries(gbtg PRIVATE SDL3::SDL3)
endif()

option(GBTG_RETAIL_GAME_ONLY "Only compile retail game. No modifications." ON)
if(GBTG_RETAIL_GAME_ONLY)
    target_compile_definitions(gbtg PRIVATE GBTG_RETAIL_GAME_ONLY)
    target_compile_definitions(gbtg_lib PRIVATE GBTG_RETAIL_GAME_ONLY)
    
    # Warning C4996 - Retail uses strcpy, disable deprecation
    target_compile_definitions(gbtg PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(gbtg_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Host presenter optimizations (src/engine/graphics/render_optimize.h). OFF keeps the retail
# presenter path. Test and runtime targets copy gbtg's definitions below.
option(GBTG_OPTIMIZE "Host presenter optimizations; OFF builds the retail presenter" OFF)
if(GBTG_OPTIMIZE)
    target_compile_definitions(gbtg PRIVATE GBTG_OPTIMIZE=1)
    target_compile_definitions(gbtg_lib PRIVATE GBTG_OPTIMIZE=1)
endif()

# LGPL FFmpeg supplies the Bink decoder. The RAD SDK is not linked. DLLs are copied
# next to gbtg.exe so running the exe does not need a separate install.
if(WIN32)
    set(GBTG_FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip")
    if(EXISTS "${CMAKE_SOURCE_DIR}/cache/ffmpeg-lgpl.zip")
        set(GBTG_FFMPEG_URL "${CMAKE_SOURCE_DIR}/cache/ffmpeg-lgpl.zip")
    endif()
    FetchContent_Declare(
        ffmpeg_lgpl
        URL "${GBTG_FFMPEG_URL}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ffmpeg_lgpl)
    file(GLOB_RECURSE GBTG_FFMPEG_LIBS "${ffmpeg_lgpl_SOURCE_DIR}/*.lib")
    set(GBTG_FFMPEG_AVCODEC "")
    foreach(lib ${GBTG_FFMPEG_LIBS})
        if(lib MATCHES "avcodec\\.lib$")
            set(GBTG_FFMPEG_AVCODEC "${lib}")
            break()
        endif()
    endforeach()
    if(GBTG_FFMPEG_AVCODEC STREQUAL "")
        message(FATAL_ERROR "FFmpeg avcodec.lib was not found under ${ffmpeg_lgpl_SOURCE_DIR}")
    endif()
    cmake_path(GET GBTG_FFMPEG_AVCODEC PARENT_PATH GBTG_FFMPEG_LIBDIR)
    cmake_path(GET GBTG_FFMPEG_LIBDIR PARENT_PATH GBTG_FFMPEG_ROOT)
    target_include_directories(gbtg PRIVATE "${GBTG_FFMPEG_ROOT}/include")
    target_compile_definitions(gbtg PRIVATE GBTG_HAS_FFMPEG)
    target_link_libraries(gbtg PRIVATE
        "${GBTG_FFMPEG_LIBDIR}/avformat.lib"
        "${GBTG_FFMPEG_LIBDIR}/avcodec.lib"
        "${GBTG_FFMPEG_LIBDIR}/swscale.lib"
        "${GBTG_FFMPEG_LIBDIR}/swresample.lib"
        "${GBTG_FFMPEG_LIBDIR}/avutil.lib"
    )
    file(GLOB GBTG_FFMPEG_DLLS "${GBTG_FFMPEG_ROOT}/bin/*.dll")
    foreach(dll ${GBTG_FFMPEG_DLLS})
        add_custom_command(TARGET gbtg POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:gbtg>"
        )
    endforeach()
endif()

if(MSVC)
    # The scene services translation unit passes the default COFF section limit in Debug.
    set_source_files_properties(src/engine/scene/scene_actor_services.cpp PROPERTIES COMPILE_OPTIONS /bigobj)
    target_compile_options(gbtg_lib PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-catalog PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-dump PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg-xemu-patch PRIVATE /W3 /EHsc /utf-8)
    target_compile_options(gbtg PRIVATE /W3 /EHsc /utf-8)
endif()

# The retail transition regression drives controller input and asserts the
# actual scene/map/load sequence. It needs the user's disc assets.
if(BUILD_TESTING)
    add_executable(gbtg-cutscene-resource-tests tests/cutscene_resource_tests.cpp
        src/engine/scene/cutscene_resource_views.cpp
        src/engine/camera/display_clip_limits.cpp)
    target_link_libraries(gbtg-cutscene-resource-tests PRIVATE gbtg_lib)
    add_test(NAME cutscene-resource-views COMMAND gbtg-cutscene-resource-tests)
    add_executable(gbtg-animation-event-tests tests/animation_event_tests.cpp)
    target_link_libraries(gbtg-animation-event-tests PRIVATE gbtg_lib)
    add_test(NAME animation-event-boundaries COMMAND gbtg-animation-event-tests)
    get_target_property(GBTG_TRANSITION_SOURCES gbtg SOURCES)
    list(REMOVE_ITEM GBTG_TRANSITION_SOURCES src/engine/main.cpp src/engine/runtime/platform.cpp)
    add_executable(gbtg-transition-tests EXCLUDE_FROM_ALL tests/scene_transition_tests.cpp tests/camera_display_tests.cpp tests/script_actor_query_tests.cpp tests/scene_render_tests.cpp tests/shader_compiler_tests.cpp tests/shader_schedule_tests.cpp tests/shader_cost_tests.cpp tests/shader_rewrite_tests.cpp tests/native_cpp_exception_tests.cpp tests/shader_validation_constructor_tests.cpp tests/shader_validation_serializer_tests.cpp tests/shader_validation_lifecycle_tests.cpp ${GBTG_TRANSITION_SOURCES})
    add_executable(gbtg-movie-startup-tests EXCLUDE_FROM_ALL tests/movie_startup_tests.cpp ${GBTG_TRANSITION_SOURCES})
    get_target_property(GBTG_TRANSITION_LINKS gbtg LINK_LIBRARIES)
    get_target_property(GBTG_TRANSITION_DEFINITIONS gbtg COMPILE_DEFINITIONS)
    get_target_property(GBTG_TRANSITION_INCLUDES gbtg INCLUDE_DIRECTORIES)
    target_link_libraries(gbtg-transition-tests PRIVATE ${GBTG_TRANSITION_LINKS})
    target_sources(gbtg-transition-tests PRIVATE tests/player_parent_checks.cpp tests/body_frame_checks.cpp tests/player_message_checks.cpp tests/player_update_checks.cpp tests/player_mind_owner_checks.cpp tests/player_factory_owner_checks.cpp)
    target_link_libraries(gbtg-movie-startup-tests PRIVATE ${GBTG_TRANSITION_LINKS})
    if(GBTG_TRANSITION_DEFINITIONS)
        target_compile_definitions(gbtg-transition-tests PRIVATE ${GBTG_TRANSITION_DEFINITIONS})
        target_compile_definitions(gbtg-movie-startup-tests PRIVATE ${GBTG_TRANSITION_DEFINITIONS})
    endif()
    if(GBTG_TRANSITION_INCLUDES)
        target_include_directories(gbtg-transition-tests PRIVATE ${GBTG_TRANSITION_INCLUDES})
        target_include_directories(gbtg-movie-startup-tests PRIVATE ${GBTG_TRANSITION_INCLUDES})
    endif()
    foreach(dll ${GBTG_FFMPEG_DLLS})
        add_custom_command(TARGET gbtg-transition-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:gbtg-transition-tests>"
        )
    endforeach()
    if(MSVC)
        target_compile_options(gbtg-transition-tests PRIVATE /W3 /EHsc /utf-8)
        target_compile_options(gbtg-movie-startup-tests PRIVATE /W3 /EHsc /utf-8)
    endif()
endif()

# Register browsing-only files after the transition target copies engine sources.
file(GLOB_RECURSE GBTG_PUBLIC_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(GBTG_ENGINE_PUBLIC_HEADERS)
foreach(header actor_runtime aid clock debug_ui engine_internal entity game gfx host_config movie platform)
    list(APPEND GBTG_ENGINE_PUBLIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/gbtg/${header}.h")
endforeach()
list(REMOVE_ITEM GBTG_PUBLIC_HEADERS ${GBTG_ENGINE_PUBLIC_HEADERS})
file(GLOB_RECURSE GBTG_LIBRARY_INCLUDES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gbtg/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gbtg/*.inl"
)
file(GLOB_RECURSE GBTG_ENGINE_INCLUDES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.inl"
)
set_source_files_properties(${GBTG_LIBRARY_INCLUDES} ${GBTG_ENGINE_INCLUDES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(gbtg_lib PRIVATE ${GBTG_PUBLIC_HEADERS} ${GBTG_LIBRARY_INCLUDES})
target_sources(gbtg PRIVATE ${GBTG_ENGINE_PUBLIC_HEADERS} ${GBTG_ENGINE_INCLUDES})
if(GBTG_GFX STREQUAL "d3d11")
    set(GBTG_INACTIVE_BACKEND src/engine/graphics/gfx_null.cpp)
else()
    set(GBTG_INACTIVE_BACKEND src/engine/graphics/gfx_d3d11.cpp)
    set_source_files_properties(src/engine/graphics/debug_ui.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(gbtg PRIVATE src/engine/graphics/debug_ui.cpp)
endif()
set_source_files_properties(${GBTG_INACTIVE_BACKEND} PROPERTIES HEADER_FILE_ONLY TRUE)

# The CPU NV2A presentation boundary executes per vertex. Keep game code in
# Debug, but avoid interpreter and replay overhead dominating each frame.
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(SOURCE
        src/engine/graphics/xbox_vertex_program.cpp
        src/engine/graphics/scene_d3d_present.cpp
        APPEND PROPERTY VS_SETTINGS
        "$<$<CONFIG:Debug>:Optimization=MaxSpeed>"
        "$<$<CONFIG:Debug>:InlineFunctionExpansion=AnySuitable>"
        "$<$<CONFIG:Debug>:BasicRuntimeChecks=Default>"
    )
    # Their Debug options differ from the PCH they would otherwise share.
    set_source_files_properties(
        src/engine/graphics/xbox_vertex_program.cpp
        src/engine/graphics/scene_d3d_present.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endif()
target_sources(gbtg PRIVATE ${GBTG_INACTIVE_BACKEND})
if(TARGET gbtg-transition-tests)
    target_sources(gbtg-transition-tests PRIVATE ${GBTG_ENGINE_PUBLIC_HEADERS} ${GBTG_ENGINE_INCLUDES})
endif()
file(GLOB_RECURSE GBTG_PROJECT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.inl"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GBTG_PROJECT_FILES})

set_target_properties(gbtg-catalog gbtg-dump gbtg-model-check gbtg-xemu-patch
    PROPERTIES FOLDER "Tools"
)

# Compile normal game code without tooling hooks. Inspector variants compile the
# same reconstructed sources with GBTG_INSPECTOR_APP, consistently across the ABI.
option(GBTG_BUILD_INSPECTOR "Build the native scene inspector and its tests" ON)
if(WIN32 AND GBTG_GFX STREQUAL "d3d11")
    get_target_property(GBTG_ALL_GAME_SOURCES gbtg SOURCES)
    set(GBTG_SHARED_RUNTIME_SOURCES)
    set(GBTG_GAME_SHELL_SOURCES ${GBTG_ALL_GAME_SOURCES})
    foreach(source IN LISTS GBTG_ALL_GAME_SOURCES)
        if(source MATCHES "^src/engine/.*\\.cpp$" AND
            NOT source MATCHES "(main|platform|gfx_d3d11|gfx_null|debug_ui)\\.cpp$")
            list(APPEND GBTG_SHARED_RUNTIME_SOURCES "${source}")
            list(REMOVE_ITEM GBTG_GAME_SHELL_SOURCES "${source}")
        endif()
    endforeach()
    add_library(gbtg-runtime OBJECT ${GBTG_SHARED_RUNTIME_SOURCES})
    target_precompile_headers(gbtg-runtime PRIVATE "${GBTG_PCH}")
    get_target_property(GBTG_RUNTIME_LINKS gbtg LINK_LIBRARIES)
    get_target_property(GBTG_RUNTIME_DEFINITIONS gbtg COMPILE_DEFINITIONS)
    get_target_property(GBTG_RUNTIME_INCLUDES gbtg INCLUDE_DIRECTORIES)
    get_target_property(GBTG_RUNTIME_OPTIONS gbtg COMPILE_OPTIONS)
    target_link_libraries(gbtg-runtime PUBLIC ${GBTG_RUNTIME_LINKS})
    target_compile_definitions(gbtg-runtime PRIVATE ${GBTG_RUNTIME_DEFINITIONS})
    target_include_directories(gbtg-runtime PRIVATE ${GBTG_RUNTIME_INCLUDES})
    if(GBTG_RUNTIME_OPTIONS)
        target_compile_options(gbtg-runtime PRIVATE ${GBTG_RUNTIME_OPTIONS})
    endif()
    set_property(TARGET gbtg PROPERTY SOURCES "${GBTG_GAME_SHELL_SOURCES}")
    target_link_libraries(gbtg PRIVATE gbtg-runtime)
    foreach(test_target gbtg-transition-tests gbtg-movie-startup-tests)
        if(TARGET ${test_target})
            get_target_property(GBTG_TEST_SOURCES ${test_target} SOURCES)
            list(REMOVE_ITEM GBTG_TEST_SOURCES ${GBTG_SHARED_RUNTIME_SOURCES})
            set_property(TARGET ${test_target} PROPERTY SOURCES "${GBTG_TEST_SOURCES}")
            target_link_libraries(${test_target} PRIVATE gbtg-runtime)
        endif()
    endforeach()

    if(GBTG_BUILD_INSPECTOR)
        get_target_property(GBTG_INSPECTOR_LIBRARY_SOURCES gbtg_lib SOURCES)
        add_library(gbtg-inspector-lib STATIC ${GBTG_INSPECTOR_LIBRARY_SOURCES})
        target_precompile_headers(gbtg-inspector-lib PRIVATE "${GBTG_PCH}")
        foreach(property INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)
            get_target_property(value gbtg_lib ${property})
            if(value)
                set_property(TARGET gbtg-inspector-lib PROPERTY ${property} "${value}")
            endif()
        endforeach()
        get_target_property(GBTG_INSPECTOR_LIBRARY_LINKS gbtg_lib LINK_LIBRARIES)
        target_link_libraries(gbtg-inspector-lib PUBLIC ${GBTG_INSPECTOR_LIBRARY_LINKS})
        target_include_directories(gbtg-inspector-lib PUBLIC "${CMAKE_SOURCE_DIR}/include")
        target_compile_definitions(gbtg-inspector-lib PUBLIC GBTG_INSPECTOR_APP)
        add_library(gbtg-inspector-runtime OBJECT ${GBTG_SHARED_RUNTIME_SOURCES})
        target_precompile_headers(gbtg-inspector-runtime PRIVATE "${GBTG_PCH}")
        set(GBTG_INSPECTOR_RUNTIME_LINKS ${GBTG_RUNTIME_LINKS})
        list(REMOVE_ITEM GBTG_INSPECTOR_RUNTIME_LINKS gbtg_lib)
        target_link_libraries(gbtg-inspector-runtime PUBLIC ${GBTG_INSPECTOR_RUNTIME_LINKS} gbtg-inspector-lib)
        target_compile_definitions(gbtg-inspector-runtime PUBLIC ${GBTG_RUNTIME_DEFINITIONS})
        target_include_directories(gbtg-inspector-runtime PRIVATE ${GBTG_RUNTIME_INCLUDES})
        if(GBTG_RUNTIME_OPTIONS)
            target_compile_options(gbtg-inspector-runtime PRIVATE ${GBTG_RUNTIME_OPTIONS})
        endif()
        set_target_properties(gbtg-inspector-lib gbtg-inspector-runtime PROPERTIES FOLDER "Tools")

        set(GBTG_INSPECTOR_SOURCES
            src/engine/inspector/inspector_audio.cpp
            src/engine/inspector/inspector_session.cpp
            src/engine/inspector/inspector_ui.cpp
            src/engine/graphics/gfx_d3d11.cpp
        )
        set(GBTG_INSPECTOR_IMGUI_SOURCES ${GBTG_IMGUI_SOURCES})
        list(REMOVE_ITEM GBTG_INSPECTOR_IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/src/engine/graphics/debug_ui.cpp")
        add_executable(gbtg-inspector src/engine/inspector/inspector_main.cpp
            ${GBTG_INSPECTOR_SOURCES} ${GBTG_INSPECTOR_IMGUI_SOURCES}
            "${CMAKE_CURRENT_BINARY_DIR}/gbtg.rc")
        target_link_libraries(gbtg-inspector PRIVATE gbtg-inspector-runtime imm32)
        target_include_directories(gbtg-inspector PRIVATE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
        target_compile_definitions(gbtg-inspector PRIVATE GBTG_GFX_D3D11 GBTG_DEBUG_UI
            GBTG_INSPECTOR_APP IMGUI_IMPL_WIN32_DISABLE_GAMEPAD _CRT_SECURE_NO_WARNINGS)
        foreach(dll ${GBTG_FFMPEG_DLLS})
            add_custom_command(TARGET gbtg-inspector POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:gbtg-inspector>")
        endforeach()
        if(MSVC)
            target_compile_options(gbtg-inspector PRIVATE /W3 /EHsc /utf-8)
        endif()
        set_target_properties(gbtg-inspector PROPERTIES FOLDER "Tools")
        if(BUILD_TESTING)
            add_executable(gbtg-inspector-build-guard-tests tests/inspector_build_guards.cpp)
            target_include_directories(gbtg-inspector-build-guard-tests PRIVATE "${CMAKE_SOURCE_DIR}/include")
            target_compile_definitions(gbtg-inspector-build-guard-tests PRIVATE GBTG_INSPECTOR_APP)
            add_test(NAME inspector-enables-inspector-api COMMAND gbtg-inspector-build-guard-tests)
            add_executable(gbtg-inspector-tests tests/inspector_tests.cpp
                ${GBTG_INSPECTOR_SOURCES} ${GBTG_INSPECTOR_IMGUI_SOURCES})
            target_link_libraries(gbtg-inspector-tests PRIVATE gbtg-inspector-runtime imm32 dbghelp)
            target_include_directories(gbtg-inspector-tests PRIVATE "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
            target_compile_definitions(gbtg-inspector-tests PRIVATE GBTG_GFX_D3D11 GBTG_DEBUG_UI
                GBTG_INSPECTOR_APP IMGUI_IMPL_WIN32_DISABLE_GAMEPAD _CRT_SECURE_NO_WARNINGS)
            if(MSVC)
                target_compile_options(gbtg-inspector-tests PRIVATE /W3 /EHsc /utf-8)
                target_link_options(gbtg-inspector-tests PRIVATE /DEBUG)
            endif()
            add_test(NAME inspector-records-and-camera COMMAND gbtg-inspector-tests)
            set_target_properties(gbtg-inspector-tests PROPERTIES FOLDER "Test")
        endif()
    endif()
endif()

if(BUILD_TESTING)
    set_target_properties(
        gbtg-model-tests
        gbtg-object-factory-tests
        gbtg-model-runtime-tests
        gbtg-model-relocation-tests
        gbtg-animation-event-tests
        gbtg-transition-tests
        PROPERTIES FOLDER "Test"
    )
endif()
