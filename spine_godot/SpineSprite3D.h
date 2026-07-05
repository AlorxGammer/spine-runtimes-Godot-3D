/****************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *****************************************************************************/

/****************************************************************************
 * Unofficial 3D extension additions.
 *
 * This file is distributed as part of a modified Spine Runtime package and
 * remains subject to the Spine Runtimes License and preserved Esoteric
 * Software notices.
 *****************************************************************************/
#pragma once

#include "SpineAnimationState.h"
#include "SpineRendererObject.h"
#include "SpineSkeleton.h"
#include "SpineSkeletonDataResource.h"
#include "SpineSkin.h"
#include "SpineSprite.h"
#include <spine/Bone.h>
#include <unordered_map>
#include <vector>

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#else
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/array_mesh.h"
#include "scene/resources/standard_material_3d.h"
#endif

class SpineEvent;
class SpineRenderWorld3D;
class SpineSlotNode3D;
class SpineTrackEntry;

class SpineSprite3D : public Node3D, public spine::AnimationStateListenerObject {
	GDCLASS(SpineSprite3D, Node3D)

protected:
	static void _bind_methods();
	void _notification(int what);
	void _get_property_list(List<PropertyInfo> *list) const;
	bool _get(const StringName &property, Variant &value) const;
	bool _set(const StringName &property, const Variant &value);

private:
	enum GeneratedMaterialMode {
		GeneratedMaterialMode_StandardMaterial3D = 0,
		GeneratedMaterialMode_FilteredShader = 1,
	};
	enum RenderBackend {
		RenderBackend_ReferenceMesh = 0,
		RenderBackend_StreamWorld = 1,
		RenderBackend_AutoStreamWorld = 2,
	};

	struct BoneOverrideState {
		String bone_name;
		Transform2D transform;
	};
	struct PendingAnimationCommand {
		enum Type {
			Type_SetAnimation,
			Type_AddAnimation,
			Type_SetEmptyAnimation,
			Type_AddEmptyAnimation,
			Type_SetEmptyAnimations,
			Type_ClearTracks,
			Type_ClearTrack
		};

		Type type;
		String animation_name;
		bool loop;
		int track_id;
		float delay;
		float mix_duration;
	};
	struct RestorableTrackState {
		bool valid;
		String animation_name;
		bool loop;
		int track_id;
		float track_time;
	};
	struct RenderRun {
		int first_slot_order = 0;
		int last_slot_order = 0;
		int render_priority = 0;
		bool visible = false;
		bool casts_shadow = false;
		Ref<Material> material;
		Ref<Material> shadow_material;
		PackedVector3Array vertices;
		PackedVector2Array uvs;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedColorArray colors;
		PackedFloat32Array dark_colors;
		PackedFloat32Array stack_depths;
		PackedInt32Array indices;
	};

	Ref<SpineSkeletonDataResource> skeleton_data_res;
	Ref<SpineSkeleton> skeleton;
	Ref<SpineAnimationState> animation_state;
	Vector<PendingAnimationCommand> pending_animation_commands;
	Vector<RestorableTrackState> restorable_track_states;
	Vector<BoneOverrideState> bone_overrides;
	spine::AnimationState *last_animation_state_object;

	SpineConstant::UpdateMode update_mode;
	float time_scale;
	float pixels_per_unit;
	float depth_offset;
	bool render_embedded_in_parent_slot;
	int render_priority_bias;
	int render_priority_range;
	int render_backend;
	int stream_render_order;
	bool lighting_enabled;
	bool double_sided;
	GeometryInstance3D::ShadowCastingSetting shadow_casting_mode;
	float shadow_alpha_cutoff;
	float visible_alpha_cutoff;
	int generated_material_mode;
	bool generated_normal_map_enabled;
	bool generated_normal_map_preview;
	bool generated_normal_map_flip_y;
	Ref<Texture> generated_normal_map_override;
	float generated_normal_scale;
	float generated_normal_max_slope;
	float generated_normal_dark_suppression;
	float generated_standard_specular;
	float generated_standard_roughness;
	float generated_standard_metallic;
	float generated_shader_light_scale;
	float generated_shader_ambient;

	Ref<Material> normal_material;
	Ref<Material> additive_material;
	Ref<Material> multiply_material;
	Ref<Material> screen_material;
	bool legacy_position_conversion_enabled;

	bool debug_root;
	Color debug_root_color;
	bool debug_bones;
	Color debug_bones_color;
	float debug_bones_thickness;
	bool debug_regions;
	Color debug_regions_color;
	bool debug_meshes;
	Color debug_meshes_color;
	bool debug_bounding_boxes;
	Color debug_bounding_boxes_color;
	bool debug_paths;
	Color debug_paths_color;
	bool debug_clipping;
	Color debug_clipping_color;

	String preview_skin;
	String preview_animation;
	bool preview_frame;
	float preview_time;

	SpineSprite *sprite_2d;
	MeshInstance3D *mesh_instance_3d;
	MeshInstance3D *shadow_mesh_instance_3d;
	MeshInstance3D *debug_mesh_instance_3d;
	Ref<ArrayMesh> runtime_mesh;
	Ref<ArrayMesh> shadow_runtime_mesh;
	Ref<ArrayMesh> debug_runtime_mesh;
	Ref<StandardMaterial3D> debug_material;
	spine::SkeletonClipping *skeleton_clipper;
	mutable std::unordered_map<uint64_t, Ref<Material>> generated_material_cache;
	mutable std::unordered_map<uint64_t, Ref<StandardMaterial3D>> generated_shadow_material_cache;
	mutable std::unordered_map<uint64_t, Ref<Material>> custom_material_priority_cache;
	bool diagnostics_enabled;
	int diagnostics_counter;
	int last_generated_slot_count;
	int last_generated_run_count;
	bool animation_state_ready_emitted;
	bool sprite_signals_connected;
	Callable internal_spine_objects_invalidated_callable;
	Callable animation_started_callable;
	Callable animation_interrupted_callable;
	Callable animation_ended_callable;
	Callable animation_completed_callable;
	Callable animation_disposed_callable;
	Callable animation_event_callable;
	Callable before_animation_state_update_callable;
	Callable before_animation_state_apply_callable;
	Callable before_world_transforms_change_callable;
	Callable world_transforms_changed_callable;

	void ensure_runtime_nodes();
	void connect_sprite_signals();
	void disconnect_sprite_signals();
	void emit_animation_state_ready_deferred();
	void set_bone_override(const String &bone_name, const Transform2D &transform);
	void apply_bone_overrides();
	static Transform2D get_raw_bone_global_transform(spine::Bone *bone);
	static void set_raw_bone_global_transform(spine::Bone *bone, const Transform2D &transform);
	void restore_track_states_if_needed();
	void remember_track_state(Ref<SpineTrackEntry> track_entry);
	void clear_track_state(int track_id);
	void clear_all_track_states();
	void on_internal_spine_objects_invalidated();
	void update_preview_animation();
	void sync_from_sprite();
	void sync_debug_settings_to_sprite();
	void rebuild_runtime_mesh();
	void clear_runtime_mesh_surfaces();
	bool build_render_runs(std::vector<RenderRun> &runs, bool stream_space, SpineRenderWorld3D *render_world, spine::Skeleton *&skeleton_object, Vector3 &slot_stack_axis_local, float &effective_depth_scale, int &slot_count);
	void rebuild_debug_mesh(spine::Skeleton *skeleton_object, const Vector3 &slot_stack_axis_local, float effective_depth_scale, int slot_count);
	SpineSlotNode3D *find_ancestor_slot_node() const;
	SpineRenderWorld3D *find_stream_world() const;
	Vector3 get_slot_stack_axis_local() const;
	float get_effective_depth_scale_for_axis(const Vector3 &axis_local) const;
	float get_effective_depth_scale() const;
	bool should_stream_to_world() const;
	bool get_effective_reverse_slot_stack() const;
	int compute_slot_insert_band_render_priority(int draw_order_index, int slot_count, float t) const;
	int compute_slot_render_priority(int slot_order, int slot_count) const;
	void apply_shadow_mesh_state();
	Ref<Material> resolve_visible_slot_material(spine::BlendMode blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, int render_priority, bool two_color_tint, const Ref<Material> &slot_override = Ref<Material>()) const;
	SpineSlotNode3D *find_slot_node_for_index(int slot_index) const;
	Ref<Material> resolve_shadow_slot_material(const Ref<Texture> &texture) const;
	void sanitize_legacy_2d_transform_if_needed();
	bool sanitize_local_scale_if_needed();
	void callback(spine::AnimationState *state, spine::EventType type, spine::TrackEntry *entry, spine::Event *event) override;
	void on_sprite_animation_started(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry);
	void on_sprite_animation_interrupted(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry);
	void on_sprite_animation_ended(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry);
	void on_sprite_animation_completed(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry);
	void on_sprite_animation_disposed(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry);
	void on_sprite_animation_event(SpineSprite *spine_sprite, Ref<SpineAnimationState> animation_state, Ref<SpineTrackEntry> track_entry, Ref<SpineEvent> event);
	void on_sprite_before_animation_state_update(SpineSprite *spine_sprite);
	void on_sprite_before_animation_state_apply(SpineSprite *spine_sprite);
	void on_sprite_before_world_transforms_change(SpineSprite *spine_sprite);
	void on_sprite_world_transforms_changed(SpineSprite *spine_sprite);
	void log_checkpoint(const String &message) const;

public:
	SpineSprite3D();
	~SpineSprite3D();

	void set_skeleton_data_res(const Ref<SpineSkeletonDataResource> &resource);
	Ref<SpineSkeletonDataResource> get_skeleton_data_res();
	void on_skeleton_data_changed();

	Ref<SpineSkeleton> get_skeleton();
	Ref<SpineAnimationState> get_animation_state();
	Ref<SpineTrackEntry> set_animation(const String &animation_name, bool loop = true, int track_id = 0);
	Ref<SpineTrackEntry> add_animation(const String &animation_name, float delay = 0, bool loop = true, int track_id = 0);
	Ref<SpineTrackEntry> set_empty_animation(int track_id, float mix_duration);
	Ref<SpineTrackEntry> add_empty_animation(int track_id, float mix_duration, float delay);
	void set_empty_animations(float mix_duration);
	void clear_tracks();
	void clear_track(int track_id);
	void update_skeleton(float delta);

	SpineConstant::UpdateMode get_update_mode();
	void set_update_mode(SpineConstant::UpdateMode mode);

	void set_time_scale(float v);
	float get_time_scale();

	void set_pixels_per_unit(float v);
	float get_pixels_per_unit() const;
	void set_depth_offset(float v);
	float get_depth_offset() const;
	void set_render_embedded_in_parent_slot(bool v);
	bool is_render_embedded_in_parent_slot() const { return render_embedded_in_parent_slot; }
	int get_visual_slot_order(int slot_order, int slot_count) const;
	int find_draw_order_index_for_slot_index(int slot_index) const;
	Vector3 get_slot_insert_offset(int draw_order_index, int slot_count) const;
	float get_slot_insert_z(int draw_order_index, int slot_count) const;
	int get_slot_insert_render_priority(int draw_order_index, int slot_count) const;
	void set_render_priority_bias(int v);
	int get_render_priority_bias() const;
	void set_render_priority_range(int v);
	int get_render_priority_range() const;
	void set_render_backend(int v);
	int get_render_backend() const { return render_backend; }
	bool is_stream_renderer_enabled() const { return should_stream_to_world(); }
	void set_stream_render_order(int v);
	int get_stream_render_order() const { return stream_render_order; }
	void set_lighting_enabled(bool v);
	bool is_lighting_enabled() const { return lighting_enabled; }
	void set_double_sided(bool v);
	bool is_double_sided() const { return double_sided; }
	void set_shadow_casting_mode(int v);
	int get_shadow_casting_mode() const { return (int)shadow_casting_mode; }
	void set_shadow_alpha_cutoff(float v);
	float get_shadow_alpha_cutoff() const { return shadow_alpha_cutoff; }
	void set_visible_alpha_cutoff(float v);
	float get_visible_alpha_cutoff() const { return visible_alpha_cutoff; }
	void set_generated_material_mode(int v);
	int get_generated_material_mode() const { return generated_material_mode; }
	void set_generated_normal_map_enabled(bool v);
	bool is_generated_normal_map_enabled() const { return generated_normal_map_enabled; }
	void set_generated_normal_map_preview(bool v);
	bool is_generated_normal_map_preview() const { return generated_normal_map_preview; }
	void set_generated_normal_map_flip_y(bool v);
	bool is_generated_normal_map_flip_y() const { return generated_normal_map_flip_y; }
	void set_generated_normal_map_override(Ref<Texture> texture);
	Ref<Texture> get_generated_normal_map_override() const { return generated_normal_map_override; }
	void set_generated_normal_scale(float v);
	float get_generated_normal_scale() const { return generated_normal_scale; }
	void set_generated_normal_max_slope(float v);
	float get_generated_normal_max_slope() const { return generated_normal_max_slope; }
	void set_generated_normal_dark_suppression(float v);
	float get_generated_normal_dark_suppression() const { return generated_normal_dark_suppression; }
	void set_generated_standard_specular(float v);
	float get_generated_standard_specular() const { return generated_standard_specular; }
	void set_generated_standard_roughness(float v);
	float get_generated_standard_roughness() const { return generated_standard_roughness; }
	void set_generated_standard_metallic(float v);
	float get_generated_standard_metallic() const { return generated_standard_metallic; }
	void set_generated_shader_light_scale(float v);
	float get_generated_shader_light_scale() const { return generated_shader_light_scale; }
	void set_generated_shader_ambient(float v);
	float get_generated_shader_ambient() const { return generated_shader_ambient; }
	void set_legacy_position_conversion_enabled(bool v) { legacy_position_conversion_enabled = v; }
	bool is_legacy_position_conversion_enabled() const { return legacy_position_conversion_enabled; }

	void set_diagnostics_enabled(bool v) { diagnostics_enabled = v; }
	bool is_diagnostics_enabled() const { return diagnostics_enabled; }

	Ref<SpineSkin> new_skin(const String &name);

	void submit_render_parts(SpineRenderWorld3D *render_world, int collector_object_index = 0);

	Transform2D get_global_bone_transform(const String &bone_name);
	void set_global_bone_transform(const String &bone_name, Transform2D transform);

	Ref<Material> get_normal_material();
	void set_normal_material(Ref<Material> material);
	Ref<Material> get_additive_material();
	void set_additive_material(Ref<Material> material);
	Ref<Material> get_multiply_material();
	void set_multiply_material(Ref<Material> material);
	Ref<Material> get_screen_material();
	void set_screen_material(Ref<Material> material);

	bool get_debug_root() const { return debug_root; }
	void set_debug_root(bool v) { debug_root = v; if (sprite_2d) sprite_2d->set_debug_root(v); rebuild_runtime_mesh(); }
	Color get_debug_root_color() const { return debug_root_color; }
	void set_debug_root_color(const Color &v) { debug_root_color = v; if (sprite_2d) sprite_2d->set_debug_root_color(v); rebuild_runtime_mesh(); }
	bool get_debug_bones() const { return debug_bones; }
	void set_debug_bones(bool v) { debug_bones = v; if (sprite_2d) sprite_2d->set_debug_bones(v); rebuild_runtime_mesh(); }
	Color get_debug_bones_color() const { return debug_bones_color; }
	void set_debug_bones_color(const Color &v) { debug_bones_color = v; if (sprite_2d) sprite_2d->set_debug_bones_color(v); rebuild_runtime_mesh(); }
	float get_debug_bones_thickness() const { return debug_bones_thickness; }
	void set_debug_bones_thickness(float v) { debug_bones_thickness = v; if (sprite_2d) sprite_2d->set_debug_bones_thickness(v); rebuild_runtime_mesh(); }
	bool get_debug_regions() const { return debug_regions; }
	void set_debug_regions(bool v) { debug_regions = v; if (sprite_2d) sprite_2d->set_debug_regions(v); rebuild_runtime_mesh(); }
	Color get_debug_regions_color() const { return debug_regions_color; }
	void set_debug_regions_color(const Color &v) { debug_regions_color = v; if (sprite_2d) sprite_2d->set_debug_regions_color(v); rebuild_runtime_mesh(); }
	bool get_debug_meshes() const { return debug_meshes; }
	void set_debug_meshes(bool v) { debug_meshes = v; if (sprite_2d) sprite_2d->set_debug_meshes(v); rebuild_runtime_mesh(); }
	Color get_debug_meshes_color() const { return debug_meshes_color; }
	void set_debug_meshes_color(const Color &v) { debug_meshes_color = v; if (sprite_2d) sprite_2d->set_debug_meshes_color(v); rebuild_runtime_mesh(); }
	bool get_debug_bounding_boxes() const { return debug_bounding_boxes; }
	void set_debug_bounding_boxes(bool v) { debug_bounding_boxes = v; if (sprite_2d) sprite_2d->set_debug_bounding_boxes(v); rebuild_runtime_mesh(); }
	Color get_debug_bounding_boxes_color() const { return debug_bounding_boxes_color; }
	void set_debug_bounding_boxes_color(const Color &v) { debug_bounding_boxes_color = v; if (sprite_2d) sprite_2d->set_debug_bounding_boxes_color(v); rebuild_runtime_mesh(); }
	bool get_debug_paths() const { return debug_paths; }
	void set_debug_paths(bool v) { debug_paths = v; if (sprite_2d) sprite_2d->set_debug_paths(v); rebuild_runtime_mesh(); }
	Color get_debug_paths_color() const { return debug_paths_color; }
	void set_debug_paths_color(const Color &v) { debug_paths_color = v; if (sprite_2d) sprite_2d->set_debug_paths_color(v); rebuild_runtime_mesh(); }
	bool get_debug_clipping() const { return debug_clipping; }
	void set_debug_clipping(bool v) { debug_clipping = v; if (sprite_2d) sprite_2d->set_debug_clipping(v); rebuild_runtime_mesh(); }
	Color get_debug_clipping_color() const { return debug_clipping_color; }
	void set_debug_clipping_color(const Color &v) { debug_clipping_color = v; if (sprite_2d) sprite_2d->set_debug_clipping_color(v); rebuild_runtime_mesh(); }
};
