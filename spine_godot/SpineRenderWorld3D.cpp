/****************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *****************************************************************************/

#include "SpineRenderWorld3D.h"

#include "SpineSprite3D.h"
#include <algorithm>
#include <spine/BlendMode.h>

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#endif

namespace {
static uint64_t get_material_key(const Ref<Material> &material) {
	return material.is_valid() ? (uint64_t)material->get_instance_id() : 0;
}

static uint64_t quantize_float_key(float value, float scale = 1000.0f) {
	return (uint64_t)CLAMP((int)Math::round(value * scale), -1048576, 1048576);
}

static void hash_mix(uint64_t &seed, uint64_t value) {
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	seed ^= value ^ (seed << 6) ^ (seed >> 2);
}

static uint64_t make_visible_material_cache_key(int blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, const Ref<Material> &custom_material, int render_priority, bool lighting_enabled, bool generated_normal_map_enabled, bool generated_normal_map_preview, bool two_color_tint, float generated_normal_scale, float generated_normal_max_slope, float generated_normal_dark_suppression, float generated_standard_specular, float generated_standard_roughness, float generated_standard_metallic, float generated_shader_light_scale, float generated_shader_ambient, float visible_alpha_cutoff) {
	uint64_t key = 0x5350524e4d41544cULL;
	hash_mix(key, (uint64_t)blend_mode);
	hash_mix(key, texture.is_valid() ? (uint64_t)texture->get_instance_id() : 0);
	hash_mix(key, normal_map.is_valid() ? (uint64_t)normal_map->get_instance_id() : 0);
	hash_mix(key, custom_material.is_valid() ? (uint64_t)custom_material->get_instance_id() : 0);
	hash_mix(key, (uint64_t)(CLAMP(render_priority, -128, 127) + 128));
	hash_mix(key, lighting_enabled ? 1ULL : 0ULL);
	hash_mix(key, generated_normal_map_enabled ? 1ULL : 0ULL);
	hash_mix(key, generated_normal_map_preview ? 1ULL : 0ULL);
	hash_mix(key, two_color_tint ? 1ULL : 0ULL);
	hash_mix(key, quantize_float_key(generated_normal_scale));
	hash_mix(key, quantize_float_key(generated_normal_max_slope));
	hash_mix(key, quantize_float_key(generated_normal_dark_suppression));
	hash_mix(key, quantize_float_key(generated_standard_specular));
	hash_mix(key, quantize_float_key(generated_standard_roughness));
	hash_mix(key, quantize_float_key(generated_standard_metallic));
	hash_mix(key, quantize_float_key(generated_shader_light_scale));
	hash_mix(key, quantize_float_key(generated_shader_ambient));
	hash_mix(key, quantize_float_key(visible_alpha_cutoff));
	return key;
}

static Ref<ShaderMaterial> make_normal_map_preview_material(const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, int render_priority, float alpha_cutoff) {
	Ref<Shader> shader = memnew(Shader);
	shader->set_code(
			"shader_type spatial;\n"
			"render_mode unshaded, cull_disabled, depth_prepass_alpha;\n"
			"uniform sampler2D spine_texture : source_color;\n"
			"uniform sampler2D spine_normal_texture : hint_normal;\n"
			"uniform bool spine_has_normal_texture = false;\n"
			"uniform float spine_alpha_cutoff = 0.0;\n"
			"void fragment() {\n"
			"	vec4 diffuse_sample = texture(spine_texture, UV);\n"
			"	vec4 normal_sample = texture(spine_normal_texture, UV);\n"
			"	vec2 normal_xy = normal_sample.rg * 2.0 - 1.0;\n"
			"	float normal_z = sqrt(max(1.0 - dot(normal_xy, normal_xy), 0.0));\n"
			"	vec3 preview_normal = vec3(normal_xy * 0.5 + 0.5, normal_z * 0.5 + 0.5);\n"
			"	ALBEDO = spine_has_normal_texture ? preview_normal : vec3(1.0, 0.0, 1.0);\n"
			"	ALPHA = diffuse_sample.a;\n"
			"	if (ALPHA <= spine_alpha_cutoff) discard;\n"
			"}\n");

	Ref<ShaderMaterial> material = memnew(ShaderMaterial);
	material->set_shader(shader);
	material->set_render_priority(render_priority);
	material->set_shader_parameter(StringName("spine_texture"), diffuse_texture);
	material->set_shader_parameter(StringName("spine_normal_texture"), normal_texture);
	material->set_shader_parameter(StringName("spine_has_normal_texture"), normal_texture.is_valid());
	material->set_shader_parameter(StringName("spine_alpha_cutoff"), alpha_cutoff);
	return material;
}

static BitField<Mesh::ArrayFormat> make_dark_color_array_flags() {
	return (BitField<Mesh::ArrayFormat>)((uint64_t)Mesh::ARRAY_FORMAT_CUSTOM0 | ((uint64_t)Mesh::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT));
}

static Ref<ShaderMaterial> make_two_color_material(int blend_mode, const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, int render_priority, bool lighting_enabled, bool use_normal_texture, float normal_scale, float standard_specular, float standard_roughness, float standard_metallic, float light_scale, float ambient, float alpha_cutoff) {
	Ref<Shader> shader = memnew(Shader);
	String render_mode = "render_mode ";
	if (!lighting_enabled) {
		render_mode += "unshaded, ";
	}
	render_mode += "cull_disabled, depth_draw_never";
	switch ((spine::BlendMode)blend_mode) {
		case spine::BlendMode_Additive:
		case spine::BlendMode_Screen:
			render_mode += ", blend_add";
			break;
		case spine::BlendMode_Multiply:
			render_mode += ", blend_mul";
			break;
		case spine::BlendMode_Normal:
		default:
			render_mode += ", blend_mix";
			break;
	}
	render_mode += ";\n";

	shader->set_code(
			String("shader_type spatial;\n") +
			render_mode +
			"uniform sampler2D spine_texture : source_color;\n"
			"uniform sampler2D spine_normal_texture : hint_normal;\n"
			"uniform bool spine_use_normal_texture = false;\n"
			"uniform float spine_normal_scale = 1.0;\n"
			"uniform float spine_light_scale = 1.0;\n"
			"uniform float spine_ambient = 0.0;\n"
			"uniform float spine_specular = 0.5;\n"
			"uniform float spine_roughness = 0.5;\n"
			"uniform float spine_metallic = 0.0;\n"
			"uniform float spine_alpha_cutoff = 0.0;\n"
			"varying vec4 spine_dark_color;\n"
			"void vertex() {\n"
			"	spine_dark_color = CUSTOM0;\n"
			"}\n"
			"void fragment() {\n"
			"	vec4 diffuse_sample = texture(spine_texture, UV);\n"
			"	vec4 light_color = COLOR;\n"
			"	vec3 two_color_rgb = ((diffuse_sample.a - 1.0) * spine_dark_color.a + 1.0 - diffuse_sample.rgb) * spine_dark_color.rgb + diffuse_sample.rgb * light_color.rgb;\n"
			"	ALBEDO = two_color_rgb * spine_light_scale;\n"
			"	ALPHA = diffuse_sample.a * light_color.a;\n"
			"	if (ALPHA <= spine_alpha_cutoff) discard;\n"
			"	if (spine_use_normal_texture) {\n"
			"		NORMAL_MAP = texture(spine_normal_texture, UV).rgb;\n"
			"		NORMAL_MAP_DEPTH = spine_normal_scale;\n"
			"	}\n"
			"	EMISSION = ALBEDO * spine_ambient;\n"
			"	SPECULAR = spine_specular;\n"
			"	ROUGHNESS = spine_roughness;\n"
			"	METALLIC = spine_metallic;\n"
			"}\n");

	Ref<ShaderMaterial> material = memnew(ShaderMaterial);
	material->set_shader(shader);
	material->set_render_priority(render_priority);
	material->set_shader_parameter(StringName("spine_texture"), diffuse_texture);
	material->set_shader_parameter(StringName("spine_normal_texture"), normal_texture);
	material->set_shader_parameter(StringName("spine_use_normal_texture"), use_normal_texture && normal_texture.is_valid());
	material->set_shader_parameter(StringName("spine_normal_scale"), normal_scale);
	material->set_shader_parameter(StringName("spine_light_scale"), light_scale);
	material->set_shader_parameter(StringName("spine_ambient"), ambient);
	material->set_shader_parameter(StringName("spine_specular"), standard_specular);
	material->set_shader_parameter(StringName("spine_roughness"), standard_roughness);
	material->set_shader_parameter(StringName("spine_metallic"), standard_metallic);
	material->set_shader_parameter(StringName("spine_alpha_cutoff"), alpha_cutoff);
	return material;
}

static uint64_t make_shadow_material_cache_key(const Ref<Texture> &texture, float shadow_alpha_cutoff, int shadow_casting_mode) {
	uint64_t key = 0x5350524e53484457ULL;
	hash_mix(key, texture.is_valid() ? (uint64_t)texture->get_instance_id() : 0);
	hash_mix(key, quantize_float_key(shadow_alpha_cutoff));
	hash_mix(key, (uint64_t)shadow_casting_mode);
	return key;
}
}

void SpineRenderWorld3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_auto_collect_descendants", "v"), &SpineRenderWorld3D::set_auto_collect_descendants);
	ClassDB::bind_method(D_METHOD("is_auto_collect_descendants"), &SpineRenderWorld3D::is_auto_collect_descendants);
	ClassDB::bind_method(D_METHOD("set_render_plane", "v"), &SpineRenderWorld3D::set_render_plane);
	ClassDB::bind_method(D_METHOD("get_render_plane"), &SpineRenderWorld3D::get_render_plane);
	ClassDB::bind_method(D_METHOD("set_world_order", "v"), &SpineRenderWorld3D::set_world_order);
	ClassDB::bind_method(D_METHOD("get_world_order"), &SpineRenderWorld3D::get_world_order);
	ClassDB::bind_method(D_METHOD("set_material_render_priority", "v"), &SpineRenderWorld3D::set_material_render_priority);
	ClassDB::bind_method(D_METHOD("get_material_render_priority"), &SpineRenderWorld3D::get_material_render_priority);
	ClassDB::bind_method(D_METHOD("set_max_parts_per_frame", "v"), &SpineRenderWorld3D::set_max_parts_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_parts_per_frame"), &SpineRenderWorld3D::get_max_parts_per_frame);
	ClassDB::bind_method(D_METHOD("get_last_sprite_count"), &SpineRenderWorld3D::get_last_sprite_count);
	ClassDB::bind_method(D_METHOD("get_last_part_count"), &SpineRenderWorld3D::get_last_part_count);
	ClassDB::bind_method(D_METHOD("get_last_run_count"), &SpineRenderWorld3D::get_last_run_count);
	ClassDB::bind_method(D_METHOD("get_last_shadow_run_count"), &SpineRenderWorld3D::get_last_shadow_run_count);
	ClassDB::bind_method(D_METHOD("render_now"), &SpineRenderWorld3D::render_now);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_collect_descendants"), "set_auto_collect_descendants", "is_auto_collect_descendants");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_plane", PROPERTY_HINT_RANGE, "-128,127,1"), "set_render_plane", "get_render_plane");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "world_order", PROPERTY_HINT_RANGE, "-32768,32767,1"), "set_world_order", "get_world_order");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_render_priority", PROPERTY_HINT_RANGE, "-128,127,1"), "set_material_render_priority", "get_material_render_priority");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_parts_per_frame", PROPERTY_HINT_RANGE, "0,1000000,1"), "set_max_parts_per_frame", "get_max_parts_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_sprite_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "", "get_last_sprite_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_part_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "", "get_last_part_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_run_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "", "get_last_run_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_shadow_run_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "", "get_last_shadow_run_count");
}

SpineRenderWorld3D::SpineRenderWorld3D() :
		mesh_instance_3d(nullptr),
		shadow_mesh_instance_3d(nullptr),
		auto_collect_descendants(true),
		render_plane(0),
		world_order(0),
		material_render_priority(0),
		max_parts_per_frame(0),
		last_sprite_count(0),
		last_part_count(0),
		last_run_count(0),
		last_shadow_run_count(0) {
}

void SpineRenderWorld3D::_notification(int what) {
	switch (what) {
		case NOTIFICATION_READY:
			ensure_runtime_nodes();
			set_process(true);
			set_process_priority(1000000);
			break;
		case NOTIFICATION_PROCESS:
			if (auto_collect_descendants) {
				render_now();
			}
			break;
	}
}

void SpineRenderWorld3D::ensure_runtime_nodes() {
	if (!mesh_instance_3d) {
		mesh_instance_3d = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("SpineRenderWorld3DMesh")));
	}
	if (!mesh_instance_3d) {
		mesh_instance_3d = memnew(MeshInstance3D);
		mesh_instance_3d->set_name("SpineRenderWorld3DMesh");
		add_child(mesh_instance_3d);
	}
	if (!runtime_mesh.is_valid()) runtime_mesh.instantiate();
	mesh_instance_3d->set_mesh(runtime_mesh);
	mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mesh_instance_3d->set_extra_cull_margin(10.0f);
	mesh_instance_3d->set_sorting_use_aabb_center(false);

	if (!shadow_mesh_instance_3d) {
		shadow_mesh_instance_3d = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("SpineRenderWorld3DShadowMesh")));
	}
	if (!shadow_mesh_instance_3d) {
		shadow_mesh_instance_3d = memnew(MeshInstance3D);
		shadow_mesh_instance_3d->set_name("SpineRenderWorld3DShadowMesh");
		add_child(shadow_mesh_instance_3d);
	}
	if (!shadow_runtime_mesh.is_valid()) shadow_runtime_mesh.instantiate();
	shadow_mesh_instance_3d->set_mesh(shadow_runtime_mesh);
	shadow_mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
	shadow_mesh_instance_3d->set_extra_cull_margin(10.0f);
	shadow_mesh_instance_3d->set_sorting_use_aabb_center(false);
	shadow_mesh_instance_3d->set_visible(false);
}

void SpineRenderWorld3D::begin_frame() {
	parts.clear();
	last_sprite_count = 0;
	last_part_count = 0;
	last_run_count = 0;
	last_shadow_run_count = 0;
}

Ref<Material> SpineRenderWorld3D::resolve_visible_material(int blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, const Ref<Material> &custom_material, bool lighting_enabled, bool generated_normal_map_enabled, bool generated_normal_map_preview, bool two_color_tint, float generated_normal_scale, float generated_normal_max_slope, float generated_normal_dark_suppression, float generated_standard_specular, float generated_standard_roughness, float generated_standard_metallic, float generated_shader_light_scale, float generated_shader_ambient, float visible_alpha_cutoff) const {
	const int clamped_priority = CLAMP(material_render_priority, -128, 127);
	const Ref<Texture> material_normal_map = normal_map;
	Ref<Texture2D> material_texture_2d = texture;
	Ref<Texture2D> material_normal_texture_2d = material_normal_map;
	const uint64_t cache_key = make_visible_material_cache_key(blend_mode, texture, material_normal_map, custom_material, clamped_priority, lighting_enabled, generated_normal_map_enabled, generated_normal_map_preview, two_color_tint, generated_normal_scale, generated_normal_max_slope, generated_normal_dark_suppression, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient, visible_alpha_cutoff);

	auto it = visible_material_cache.find(cache_key);
	if (it != visible_material_cache.end()) {
		return it->second;
	}

	if (generated_normal_map_preview) {
		Ref<Material> preview_material = make_normal_map_preview_material(material_texture_2d, material_normal_texture_2d, clamped_priority, visible_alpha_cutoff);
		visible_material_cache.emplace(cache_key, preview_material);
		return preview_material;
	}

	if (custom_material.is_valid()) {
		Ref<ShaderMaterial> source_shader_material = custom_material;
		if (source_shader_material.is_valid()) {
			Ref<ShaderMaterial> runtime_shader_material = source_shader_material->duplicate(true);
			if (!runtime_shader_material.is_valid()) {
				runtime_shader_material.instantiate();
				runtime_shader_material->set_shader(source_shader_material->get_shader());
			}
			runtime_shader_material->set_render_priority(clamped_priority);
			runtime_shader_material->set_shader_parameter(StringName("spine_texture"), material_texture_2d);
			runtime_shader_material->set_shader_parameter(StringName("spine_normal_texture"), material_normal_texture_2d);
			runtime_shader_material->set_shader_parameter(StringName("spine_use_normal_texture"), generated_normal_map_enabled && material_normal_texture_2d.is_valid());
			runtime_shader_material->set_shader_parameter(StringName("spine_normal_scale"), generated_normal_scale);
			runtime_shader_material->set_shader_parameter(StringName("spine_normal_max_slope"), generated_normal_max_slope);
			runtime_shader_material->set_shader_parameter(StringName("spine_normal_dark_suppression"), generated_normal_dark_suppression);
			runtime_shader_material->set_shader_parameter(StringName("spine_light_scale"), generated_shader_light_scale);
			runtime_shader_material->set_shader_parameter(StringName("spine_ambient"), generated_shader_ambient);
			runtime_shader_material->set_shader_parameter(StringName("spine_specular"), generated_standard_specular);
			runtime_shader_material->set_shader_parameter(StringName("spine_roughness"), generated_standard_roughness);
			runtime_shader_material->set_shader_parameter(StringName("spine_metallic"), generated_standard_metallic);
			runtime_shader_material->set_shader_parameter(StringName("spine_alpha_cutoff"), visible_alpha_cutoff);
			visible_material_cache.emplace(cache_key, runtime_shader_material);
			return runtime_shader_material;
		}

		Ref<Material> duplicated = custom_material->duplicate();
		if (!duplicated.is_valid()) {
			visible_material_cache.emplace(cache_key, custom_material);
			return custom_material;
		}

		duplicated->set("render_priority", clamped_priority);
		Ref<BaseMaterial3D> base_material = duplicated;
		if (base_material.is_valid()) {
			base_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			base_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			base_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
			base_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
			base_material->set_albedo(Color(generated_shader_light_scale, generated_shader_light_scale, generated_shader_light_scale, 1.0f));
			base_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
			if (generated_shader_ambient > 0.0f) {
				base_material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
				base_material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, texture);
				base_material->set_emission(Color(1.0f, 1.0f, 1.0f));
				base_material->set_emission_energy_multiplier(generated_shader_ambient);
			}
			if (generated_normal_map_enabled && material_normal_map.is_valid()) {
				base_material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
				base_material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, material_normal_map);
				base_material->set_normal_scale(generated_normal_scale);
			}
			if (lighting_enabled) {
				base_material->set_metallic(generated_standard_metallic);
				base_material->set_specular(generated_standard_specular);
				base_material->set_roughness(generated_standard_roughness);
				base_material->set_specular_mode(generated_standard_specular <= 0.000001f ? BaseMaterial3D::SPECULAR_DISABLED : BaseMaterial3D::SPECULAR_SCHLICK_GGX);
			}
			base_material->set("alpha_scissor_threshold", 0.0f);
		}

		visible_material_cache.emplace(cache_key, duplicated);
		return duplicated;
	}

	if (two_color_tint) {
		Ref<Material> material = make_two_color_material(blend_mode, material_texture_2d, material_normal_texture_2d, clamped_priority, lighting_enabled, generated_normal_map_enabled, generated_normal_scale, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient, visible_alpha_cutoff);
		visible_material_cache.emplace(cache_key, material);
		return material;
	}

	Ref<StandardMaterial3D> material = memnew(StandardMaterial3D);
	material->set_shading_mode(lighting_enabled ? BaseMaterial3D::SHADING_MODE_PER_PIXEL : BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_render_priority(clamped_priority);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
	material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
	material->set("alpha_scissor_threshold", 0.0f);
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_albedo(Color(generated_shader_light_scale, generated_shader_light_scale, generated_shader_light_scale, 1.0f));
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
	if (generated_shader_ambient > 0.0f) {
		material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, texture);
		material->set_emission(Color(1.0f, 1.0f, 1.0f));
		material->set_emission_energy_multiplier(generated_shader_ambient);
	}
	if (generated_normal_map_enabled && material_normal_map.is_valid()) {
		material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
		material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, material_normal_map);
		material->set_normal_scale(generated_normal_scale);
	}
	if (lighting_enabled) {
		material->set_metallic(generated_standard_metallic);
		material->set_specular(generated_standard_specular);
		material->set_roughness(generated_standard_roughness);
		material->set_specular_mode(generated_standard_specular <= 0.000001f ? BaseMaterial3D::SPECULAR_DISABLED : BaseMaterial3D::SPECULAR_SCHLICK_GGX);
	}
	switch ((spine::BlendMode)blend_mode) {
		case spine::BlendMode_Additive: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD); break;
		case spine::BlendMode_Multiply: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MUL); break;
		case spine::BlendMode_Screen: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD); break;
		case spine::BlendMode_Normal:
		default:
			material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);
			break;
	}

	visible_material_cache.emplace(cache_key, material);
	return material;
}

Ref<Material> SpineRenderWorld3D::resolve_shadow_material(const Ref<Texture> &texture, float shadow_alpha_cutoff, int shadow_casting_mode) const {
	const float clamped_cutoff = CLAMP(shadow_alpha_cutoff, 0.0f, 1.0f);
	const int clamped_mode = CLAMP(shadow_casting_mode, 0, 3);
	const uint64_t cache_key = make_shadow_material_cache_key(texture, clamped_cutoff, clamped_mode);
	auto it = shadow_material_cache.find(cache_key);
	if (it != shadow_material_cache.end()) {
		return it->second;
	}

	Ref<StandardMaterial3D> material = memnew(StandardMaterial3D);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	const bool double_sided_shadows = clamped_mode == GeometryInstance3D::SHADOW_CASTING_SETTING_DOUBLE_SIDED;
	material->set_cull_mode(double_sided_shadows ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
	material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, false);
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
	material->set("alpha_scissor_threshold", clamped_cutoff);
	material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);

	shadow_material_cache.emplace(cache_key, material);
	return material;
}

void SpineRenderWorld3D::submit_part(int object_order, int collector_object_index, int slot_order, int tie_breaker, const Ref<Material> &material, bool casts_shadow, const Ref<Material> &shadow_material, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedInt32Array &indices) {
	if (vertices.is_empty() || indices.is_empty()) return;
	if (max_parts_per_frame > 0 && (int)parts.size() >= max_parts_per_frame) return;

	RenderPart part;
	part.material_key = get_material_key(material);
	part.shadow_material_key = get_material_key(shadow_material);
	part.render_plane = render_plane;
	part.world_order = world_order;
	part.object_order = object_order;
	part.collector_object_index = collector_object_index;
	part.slot_order = slot_order;
	part.tie_breaker = tie_breaker;
	part.material = material;
	part.shadow_material = shadow_material;
	part.vertices = vertices;
	part.uvs = uvs;
	part.normals = normals;
	part.tangents = tangents;
	part.colors = colors;
	part.dark_colors = dark_colors;
	part.indices = indices;
	part.visible = material.is_valid();
	part.casts_shadow = casts_shadow && shadow_material.is_valid();
	parts.push_back(part);
}

void SpineRenderWorld3D::submit_run(int object_order, int collector_object_index, int first_slot_order, int last_slot_order, const Ref<Material> &material, bool casts_shadow, const Ref<Material> &shadow_material, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedInt32Array &indices) {
	submit_part(object_order, collector_object_index, first_slot_order, MAX(0, last_slot_order - first_slot_order), material, casts_shadow, shadow_material, vertices, uvs, normals, tangents, colors, dark_colors, indices);
}

void SpineRenderWorld3D::collect_from_node(Node *node, int &collector_object_index) {
	if (!node) return;
	for (int i = 0; i < node->get_child_count(); ++i) {
		Node *child = node->get_child(i);
		SpineRenderWorld3D *nested_world = Object::cast_to<SpineRenderWorld3D>(child);
		if (nested_world && nested_world != this) {
			continue;
		}
		SpineSprite3D *sprite = Object::cast_to<SpineSprite3D>(child);
		if (sprite && sprite->is_stream_renderer_enabled()) {
			const int object_index = collector_object_index++;
			last_sprite_count++;
			sprite->submit_render_parts(this, object_index);
		}
		collect_from_node(child, collector_object_index);
	}
}

void SpineRenderWorld3D::render_now() {
	ensure_runtime_nodes();
	begin_frame();
	int collector_object_index = 0;
	collect_from_node(this, collector_object_index);
	end_frame();
}

void SpineRenderWorld3D::end_frame() {
	last_part_count = (int)parts.size();
	rebuild_stream_mesh();
}

void SpineRenderWorld3D::append_part_to_arrays(const RenderPart &part, PackedVector3Array &vertices, PackedVector2Array &uvs, PackedVector3Array &normals, PackedFloat32Array &tangents, PackedColorArray &colors, PackedFloat32Array &dark_colors, PackedInt32Array &indices) const {
	const int vertex_offset = vertices.size();
	for (int i = 0; i < part.vertices.size(); ++i) vertices.push_back(part.vertices[i]);
	for (int i = 0; i < part.uvs.size(); ++i) uvs.push_back(part.uvs[i]);
	for (int i = 0; i < part.normals.size(); ++i) normals.push_back(part.normals[i]);
	for (int i = 0; i < part.tangents.size(); ++i) tangents.push_back(part.tangents[i]);
	for (int i = 0; i < part.colors.size(); ++i) colors.push_back(part.colors[i]);
	for (int i = 0; i < part.dark_colors.size(); ++i) dark_colors.push_back(part.dark_colors[i]);
	for (int i = 0; i < part.indices.size(); ++i) indices.push_back(part.indices[i] + vertex_offset);
}

void SpineRenderWorld3D::rebuild_stream_mesh() {
	if (!runtime_mesh.is_valid()) return;
	if (!shadow_runtime_mesh.is_valid()) return;
	runtime_mesh->clear_surfaces();
	shadow_runtime_mesh->clear_surfaces();

	if (parts.empty()) {
		if (mesh_instance_3d) mesh_instance_3d->set_visible(false);
		if (shadow_mesh_instance_3d) shadow_mesh_instance_3d->set_visible(false);
		return;
	}

	std::stable_sort(parts.begin(), parts.end(), [](const RenderPart &a, const RenderPart &b) {
		if (a.render_plane != b.render_plane) return a.render_plane < b.render_plane;
		if (a.world_order != b.world_order) return a.world_order < b.world_order;
		if (a.object_order != b.object_order) return a.object_order < b.object_order;
		if (a.collector_object_index != b.collector_object_index) return a.collector_object_index < b.collector_object_index;
		if (a.slot_order != b.slot_order) return a.slot_order < b.slot_order;
		return a.tie_breaker < b.tie_breaker;
	});

	int i = 0;
	while (i < (int)parts.size()) {
		while (i < (int)parts.size() && !parts[i].visible) i++;
		if (i >= (int)parts.size()) break;

		const uint64_t material_key = parts[i].material_key;
		Ref<Material> material = parts[i].material;
		PackedVector3Array vertices;
		PackedVector2Array uvs;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedColorArray colors;
		PackedFloat32Array dark_colors;
		PackedInt32Array indices;

		while (i < (int)parts.size() && parts[i].visible && parts[i].material_key == material_key) {
			append_part_to_arrays(parts[i], vertices, uvs, normals, tangents, colors, dark_colors, indices);
			i++;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TANGENT] = tangents;
		arrays[Mesh::ARRAY_COLOR] = colors;
		const bool has_dark_colors = dark_colors.size() == vertices.size() * 4;
		if (has_dark_colors) arrays[Mesh::ARRAY_CUSTOM0] = dark_colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), has_dark_colors ? make_dark_color_array_flags() : (BitField<Mesh::ArrayFormat>)0);
		runtime_mesh->surface_set_material(runtime_mesh->get_surface_count() - 1, material);
		last_run_count++;
	}

	i = 0;
	while (i < (int)parts.size()) {
		while (i < (int)parts.size() && !parts[i].casts_shadow) i++;
		if (i >= (int)parts.size()) break;

		const uint64_t material_key = parts[i].shadow_material_key;
		Ref<Material> material = parts[i].shadow_material;
		PackedVector3Array vertices;
		PackedVector2Array uvs;
		PackedVector3Array normals;
		PackedFloat32Array tangents;
		PackedColorArray colors;
		PackedFloat32Array dark_colors;
		PackedInt32Array indices;

		while (i < (int)parts.size() && parts[i].casts_shadow && parts[i].shadow_material_key == material_key) {
			append_part_to_arrays(parts[i], vertices, uvs, normals, tangents, colors, dark_colors, indices);
			i++;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TANGENT] = tangents;
		arrays[Mesh::ARRAY_COLOR] = colors;
		const bool has_dark_colors = dark_colors.size() == vertices.size() * 4;
		if (has_dark_colors) arrays[Mesh::ARRAY_CUSTOM0] = dark_colors;
		arrays[Mesh::ARRAY_INDEX] = indices;
		shadow_runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), has_dark_colors ? make_dark_color_array_flags() : (BitField<Mesh::ArrayFormat>)0);
		shadow_runtime_mesh->surface_set_material(shadow_runtime_mesh->get_surface_count() - 1, material);
		last_shadow_run_count++;
	}

	if (mesh_instance_3d) mesh_instance_3d->set_visible(runtime_mesh->get_surface_count() > 0);
	if (shadow_mesh_instance_3d) shadow_mesh_instance_3d->set_visible(shadow_runtime_mesh->get_surface_count() > 0);
}

void SpineRenderWorld3D::set_auto_collect_descendants(bool v) {
	auto_collect_descendants = v;
	set_process(auto_collect_descendants);
}

void SpineRenderWorld3D::set_render_plane(int v) {
	render_plane = CLAMP(v, -128, 127);
}

void SpineRenderWorld3D::set_world_order(int v) {
	world_order = CLAMP(v, -32768, 32767);
}

void SpineRenderWorld3D::set_material_render_priority(int v) {
	material_render_priority = CLAMP(v, -128, 127);
}

void SpineRenderWorld3D::set_max_parts_per_frame(int v) {
	max_parts_per_frame = MAX(0, v);
}
