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

#include "SpineCommon.h"
#include <unordered_map>
#include <vector>

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/texture.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#else
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/array_mesh.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"
#endif

class SpineSprite3D;

class SpineRenderWorld3D : public Node3D {
	GDCLASS(SpineRenderWorld3D, Node3D)

public:
	struct RenderPart {
		uint64_t material_key = 0;
		uint64_t shadow_material_key = 0;
		int render_plane = 0;
		int world_order = 0;
		int object_order = 0;
		int collector_object_index = 0;
		int slot_order = 0;
		int tie_breaker = 0;
		Ref<Material> material;
		Ref<Material> shadow_material;
		PackedVector3Array vertices;
		PackedVector2Array uvs;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedColorArray colors;
		PackedFloat32Array dark_colors;
		PackedInt32Array indices;
		bool visible = true;
		bool casts_shadow = false;
	};

protected:
	static void _bind_methods();
	void _notification(int what);

private:
	MeshInstance3D *mesh_instance_3d;
	MeshInstance3D *shadow_mesh_instance_3d;
	Ref<ArrayMesh> runtime_mesh;
	Ref<ArrayMesh> shadow_runtime_mesh;
	std::vector<RenderPart> parts;
	mutable std::unordered_map<uint64_t, Ref<Material>> visible_material_cache;
	mutable std::unordered_map<uint64_t, Ref<Material>> shadow_material_cache;

	bool auto_collect_descendants;
	int render_plane;
	int world_order;
	int material_render_priority;
	int max_parts_per_frame;
	int last_sprite_count;
	int last_part_count;
	int last_run_count;
	int last_shadow_run_count;

	void ensure_runtime_nodes();
	void collect_from_node(Node *node, int &collector_object_index);
	void rebuild_stream_mesh();
	void append_part_to_arrays(const RenderPart &part, PackedVector3Array &vertices, PackedVector2Array &uvs, PackedVector3Array &normals, PackedFloat32Array &tangents, PackedColorArray &colors, PackedFloat32Array &dark_colors, PackedInt32Array &indices) const;

public:
	SpineRenderWorld3D();

	void begin_frame();
	Ref<Material> resolve_visible_material(int blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, const Ref<Material> &custom_material, bool lighting_enabled, bool generated_normal_map_enabled, bool generated_normal_map_preview, bool two_color_tint, float generated_normal_scale, float generated_normal_max_slope, float generated_normal_dark_suppression, float generated_standard_specular, float generated_standard_roughness, float generated_standard_metallic, float generated_shader_light_scale, float generated_shader_ambient, float visible_alpha_cutoff) const;
	Ref<Material> resolve_shadow_material(const Ref<Texture> &texture, float shadow_alpha_cutoff, int shadow_casting_mode) const;
	void submit_part(int object_order, int collector_object_index, int slot_order, int tie_breaker, const Ref<Material> &material, bool casts_shadow, const Ref<Material> &shadow_material, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedInt32Array &indices);
	void submit_run(int object_order, int collector_object_index, int first_slot_order, int last_slot_order, const Ref<Material> &material, bool casts_shadow, const Ref<Material> &shadow_material, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedInt32Array &indices);
	void end_frame();
	void render_now();

	void set_auto_collect_descendants(bool v);
	bool is_auto_collect_descendants() const { return auto_collect_descendants; }
	void set_render_plane(int v);
	int get_render_plane() const { return render_plane; }
	void set_world_order(int v);
	int get_world_order() const { return world_order; }
	void set_material_render_priority(int v);
	int get_material_render_priority() const { return material_render_priority; }
	void set_max_parts_per_frame(int v);
	int get_max_parts_per_frame() const { return max_parts_per_frame; }
	int get_last_sprite_count() const { return last_sprite_count; }
	int get_last_part_count() const { return last_part_count; }
	int get_last_run_count() const { return last_run_count; }
	int get_last_shadow_run_count() const { return last_shadow_run_count; }
};
