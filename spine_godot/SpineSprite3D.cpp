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
#include "SpineSprite3D.h"

#include "SpineEvent.h"
#include "SpineCommon.h"
#include "SpineRenderWorld3D.h"
#include "SpineRendererObject.h"
#include "SpineSlotNode3D.h"
#include "SpineSprite.h"
#include "SpineTrackEntry.h"
#include <spine/Atlas.h>
#include <spine/BoundingBoxAttachment.h>
#include <spine/ClippingAttachment.h>
#include <spine/MeshAttachment.h>
#include <spine/PathAttachment.h>
#include <spine/RegionAttachment.h>
#include <spine/Vector.h>

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#else
#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#endif

namespace {
struct SpineSprite3DDebugSurfaceBuilder {
	PackedVector3Array line_vertices;
	PackedColorArray line_colors;
	PackedVector3Array triangle_vertices;
	PackedColorArray triangle_colors;

	void add_line(const Vector3 &a, const Vector3 &b, const Color &color) {
		line_vertices.push_back(a);
		line_vertices.push_back(b);
		line_colors.push_back(color);
		line_colors.push_back(color);
	}

	void add_line_loop(const PackedVector3Array &points, const Color &color, bool closed = true) {
		if (points.size() < 2) return;
		for (int i = 1; i < points.size(); ++i) {
			add_line(points[i - 1], points[i], color);
		}
		if (closed) add_line(points[points.size() - 1], points[0], color);
	}

	void add_triangle(const Vector3 &a, const Vector3 &b, const Vector3 &c, const Color &color) {
		triangle_vertices.push_back(a);
		triangle_vertices.push_back(b);
		triangle_vertices.push_back(c);
		triangle_colors.push_back(color);
		triangle_colors.push_back(color);
		triangle_colors.push_back(color);
	}

	void add_quad(const Vector3 &a, const Vector3 &b, const Vector3 &c, const Vector3 &d, const Color &color) {
		add_triangle(a, b, c, color);
		add_triangle(a, c, d, color);
	}
};

static void spine_sprite_3d_hash_mix(uint64_t &seed, uint64_t value) {
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	seed ^= value ^ (seed << 6) ^ (seed >> 2);
}

static uint64_t make_spine_sprite_3d_material_cache_key(uint64_t salt, spine::BlendMode blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, const Ref<Material> &custom_material, int render_priority, bool lighting_enabled, bool double_sided_geometry, bool generated_normal_map_preview, bool two_color_tint) {
	uint64_t key = salt;
	spine_sprite_3d_hash_mix(key, (uint64_t)blend_mode);
	spine_sprite_3d_hash_mix(key, texture.is_valid() ? (uint64_t)texture->get_instance_id() : 0);
	spine_sprite_3d_hash_mix(key, normal_map.is_valid() ? (uint64_t)normal_map->get_instance_id() : 0);
	spine_sprite_3d_hash_mix(key, custom_material.is_valid() ? (uint64_t)custom_material->get_instance_id() : 0);
	spine_sprite_3d_hash_mix(key, (uint64_t)(CLAMP(render_priority, -128, 127) + 128));
	spine_sprite_3d_hash_mix(key, lighting_enabled ? 1ULL : 0ULL);
	spine_sprite_3d_hash_mix(key, double_sided_geometry ? 1ULL : 0ULL);
	spine_sprite_3d_hash_mix(key, generated_normal_map_preview ? 1ULL : 0ULL);
	spine_sprite_3d_hash_mix(key, two_color_tint ? 1ULL : 0ULL);
	return key;
}

static void build_spine_sprite_3d_tangents(const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedInt32Array &indices, const Vector3 &normal, bool flip_green_channel, PackedFloat32Array &tangents) {
	const int vertex_count = vertices.size();
	tangents.resize(vertex_count * 4);
	if (vertex_count <= 0) return;

	PackedVector3Array tangent_accum;
	PackedVector3Array bitangent_accum;
	tangent_accum.resize(vertex_count);
	bitangent_accum.resize(vertex_count);
	for (int i = 0; i < vertex_count; ++i) {
		tangent_accum.set(i, Vector3());
		bitangent_accum.set(i, Vector3());
	}

	const Vector3 fallback_tangent = Math::abs(normal.dot(Vector3(1.0f, 0.0f, 0.0f))) < 0.99f ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);
	const Vector3 fallback_bitangent = normal.cross(fallback_tangent).normalized();
	for (int i = 0; i + 2 < indices.size(); i += 3) {
		const int i0 = indices[i + 0];
		const int i1 = indices[i + 1];
		const int i2 = indices[i + 2];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;

		const Vector3 p0 = vertices[i0];
		const Vector3 p1 = vertices[i1];
		const Vector3 p2 = vertices[i2];
		const Vector2 uv0 = uvs[i0];
		const Vector2 uv1 = uvs[i1];
		const Vector2 uv2 = uvs[i2];

		const Vector3 e1 = p1 - p0;
		const Vector3 e2 = p2 - p0;
		const Vector2 duv1 = uv1 - uv0;
		const Vector2 duv2 = uv2 - uv0;
		const float denom = duv1.x * duv2.y - duv2.x * duv1.y;
		if (Math::abs(denom) < 0.0000001f) {
			tangent_accum.set(i0, tangent_accum[i0] + fallback_tangent);
			tangent_accum.set(i1, tangent_accum[i1] + fallback_tangent);
			tangent_accum.set(i2, tangent_accum[i2] + fallback_tangent);
			bitangent_accum.set(i0, bitangent_accum[i0] + fallback_bitangent);
			bitangent_accum.set(i1, bitangent_accum[i1] + fallback_bitangent);
			bitangent_accum.set(i2, bitangent_accum[i2] + fallback_bitangent);
			continue;
		}

		const float inv_denom = 1.0f / denom;
		Vector3 tangent = (e1 * duv2.y - e2 * duv1.y) * inv_denom;
		Vector3 bitangent = (e2 * duv1.x - e1 * duv2.x) * inv_denom;
		if (tangent.length_squared() < CMP_EPSILON2) tangent = fallback_tangent;
		if (bitangent.length_squared() < CMP_EPSILON2) bitangent = fallback_bitangent;
		tangent_accum.set(i0, tangent_accum[i0] + tangent);
		tangent_accum.set(i1, tangent_accum[i1] + tangent);
		tangent_accum.set(i2, tangent_accum[i2] + tangent);
		bitangent_accum.set(i0, bitangent_accum[i0] + bitangent);
		bitangent_accum.set(i1, bitangent_accum[i1] + bitangent);
		bitangent_accum.set(i2, bitangent_accum[i2] + bitangent);
	}

	for (int i = 0; i < vertex_count; ++i) {
		Vector3 tangent = tangent_accum[i];
		if (tangent.length_squared() < CMP_EPSILON2) tangent = fallback_tangent;
		tangent = (tangent - normal * normal.dot(tangent)).normalized();
		if (tangent.length_squared() < CMP_EPSILON2) tangent = fallback_tangent;

		Vector3 bitangent = bitangent_accum[i];
		if (bitangent.length_squared() < CMP_EPSILON2) bitangent = fallback_bitangent;
		float handedness = normal.cross(tangent).dot(bitangent) < 0.0f ? -1.0f : 1.0f;
		if (flip_green_channel) handedness *= -1.0f;

		tangents.set(i * 4 + 0, tangent.x);
		tangents.set(i * 4 + 1, tangent.y);
		tangents.set(i * 4 + 2, tangent.z);
		tangents.set(i * 4 + 3, handedness);
	}
}

static void build_oriented_spine_sprite_3d_indices(spine::Vector<unsigned short> &source_indices, const PackedVector3Array &vertices, const Vector3 &desired_normal, PackedInt32Array &target_indices) {
	target_indices.resize((int)source_indices.size());
	const int vertex_count = vertices.size();
	for (int i = 0; i < (int)source_indices.size(); i += 3) {
		if (i + 2 >= (int)source_indices.size()) {
			target_indices.set(i, source_indices.buffer()[i]);
			continue;
		}

		const int i0 = source_indices.buffer()[i + 0];
		const int i1 = source_indices.buffer()[i + 1];
		const int i2 = source_indices.buffer()[i + 2];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
			target_indices.set(i + 0, i0);
			target_indices.set(i + 1, i1);
			target_indices.set(i + 2, i2);
			continue;
		}

		const Vector3 a = vertices[i0];
		const Vector3 b = vertices[i1];
		const Vector3 c = vertices[i2];
		const float facing = (b - a).cross(c - a).dot(desired_normal);
		target_indices.set(i + 0, i0);
		if (facing < 0.0f) {
			target_indices.set(i + 1, i2);
			target_indices.set(i + 2, i1);
		} else {
			target_indices.set(i + 1, i1);
			target_indices.set(i + 2, i2);
		}
	}
}

static Vector3 spine_debug_to_3d(float x, float y, float ppu, const Vector3 &depth_offset) {
	return Vector3(x / ppu, -y / ppu, 0.0f) + depth_offset;
}

static PackedVector3Array make_debug_points_from_world_vertices(spine::Vector<float> &vertices, int point_count, float ppu, const Vector3 &depth_offset) {
	PackedVector3Array points;
	const int safe_point_count = MIN(point_count, (int)vertices.size() / 2);
	if (safe_point_count <= 0) return points;
	points.resize(safe_point_count);
	for (int i = 0; i < safe_point_count; ++i) {
		points.set(i, spine_debug_to_3d(vertices.buffer()[i * 2], vertices.buffer()[i * 2 + 1], ppu, depth_offset));
	}
	return points;
}

static void add_debug_mesh_edges(SpineSprite3DDebugSurfaceBuilder &builder, spine::Vector<unsigned short> &indices, spine::Vector<float> &vertices, float ppu, const Vector3 &depth_offset, const Color &color) {
	for (int i = 0; i + 2 < (int)indices.size(); i += 3) {
		const int i0 = indices.buffer()[i];
		const int i1 = indices.buffer()[i + 1];
		const int i2 = indices.buffer()[i + 2];
		const Vector3 a = spine_debug_to_3d(vertices.buffer()[i0 * 2], vertices.buffer()[i0 * 2 + 1], ppu, depth_offset);
		const Vector3 b = spine_debug_to_3d(vertices.buffer()[i1 * 2], vertices.buffer()[i1 * 2 + 1], ppu, depth_offset);
		const Vector3 c = spine_debug_to_3d(vertices.buffer()[i2 * 2], vertices.buffer()[i2 * 2 + 1], ppu, depth_offset);
		builder.add_line(a, b, color);
		builder.add_line(b, c, color);
		builder.add_line(c, a, color);
	}
}

static void add_debug_bone_shape(SpineSprite3DDebugSurfaceBuilder &builder, spine::Bone *bone, float thickness, float ppu, const Vector3 &depth_offset, const Color &color) {
	if (!bone) return;
	float bone_length = bone->getData().getLength();
	if (bone_length == 0.0f) bone_length = thickness * 2.0f;

#ifdef SPINE_RUNTIME_43
	Transform2D bone_transform(spine::MathUtil::Deg_Rad * bone->getAppliedPose().getWorldRotationX(), Vector2(bone->getAppliedPose().getWorldX(), bone->getAppliedPose().getWorldY()));
	bone_transform.scale_basis(Vector2(bone->getAppliedPose().getWorldScaleX(), bone->getAppliedPose().getWorldScaleY()));
#else
	Transform2D bone_transform(spine::MathUtil::Deg_Rad * bone->getWorldRotationX(), Vector2(bone->getWorldX(), bone->getWorldY()));
	bone_transform.scale_basis(Vector2(bone->getWorldScaleX(), bone->getWorldScaleY()));
#endif

	const Vector2 local_a(-thickness, 0.0f);
	const Vector2 local_b(0.0f, thickness);
	const Vector2 local_c(bone_length, 0.0f);
	const Vector2 local_d(0.0f, -thickness);

	const Vector2 world_a = bone_transform.xform(local_a);
	const Vector2 world_b = bone_transform.xform(local_b);
	const Vector2 world_c = bone_transform.xform(local_c);
	const Vector2 world_d = bone_transform.xform(local_d);

	builder.add_quad(
			spine_debug_to_3d(world_a.x, world_a.y, ppu, depth_offset),
			spine_debug_to_3d(world_b.x, world_b.y, ppu, depth_offset),
			spine_debug_to_3d(world_c.x, world_c.y, ppu, depth_offset),
			spine_debug_to_3d(world_d.x, world_d.y, ppu, depth_offset),
			color);
}

static Ref<ShaderMaterial> make_normal_map_preview_material(const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, int render_priority, float alpha_cutoff, bool double_sided_geometry) {
	Ref<Shader> shader = memnew(Shader);
	String vertex_code;
	if (double_sided_geometry) {
		vertex_code =
				"void vertex() {\n"
				"	vec3 view_local = normalize(inverse(mat3(MODELVIEW_MATRIX)) * vec3(0.0, 0.0, 1.0));\n"
				"	VERTEX += view_local * CUSTOM1.x;\n"
				"}\n";
	}
	shader->set_code(
			String("shader_type spatial;\n") +
			String("render_mode unshaded, cull_disabled, depth_prepass_alpha;\n") +
			"uniform sampler2D spine_texture : source_color;\n"
			"uniform sampler2D spine_normal_texture : hint_normal;\n"
			"uniform bool spine_has_normal_texture = false;\n"
			"uniform float spine_alpha_cutoff = 0.0;\n"
			+ vertex_code +
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

static BitField<Mesh::ArrayFormat> make_spine_sprite_3d_custom_array_flags(bool has_dark_colors, bool has_stack_depths) {
	uint64_t flags = 0;
	if (has_dark_colors) {
		flags |= (uint64_t)Mesh::ARRAY_FORMAT_CUSTOM0 | ((uint64_t)Mesh::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT);
	}
	if (has_stack_depths) {
		flags |= (uint64_t)Mesh::ARRAY_FORMAT_CUSTOM1 | ((uint64_t)Mesh::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM1_SHIFT);
	}
	return (BitField<Mesh::ArrayFormat>)flags;
}

static String make_spine_sprite_3d_view_stack_vertex_code() {
	return "	vec3 view_local = normalize(inverse(mat3(MODELVIEW_MATRIX)) * vec3(0.0, 0.0, 1.0));\n"
		   "	VERTEX += view_local * CUSTOM1.x;\n";
}

static String make_spine_sprite_3d_double_sided_lighting_code() {
	return "	if (!FRONT_FACING) {\n"
		   "		NORMAL = -NORMAL;\n"
		   "		BINORMAL = -BINORMAL;\n"
		   "	}\n";
}

static String get_spine_sprite_3d_blend_render_mode(spine::BlendMode blend_mode) {
	switch (blend_mode) {
		case spine::BlendMode_Additive:
		case spine::BlendMode_Screen:
			return "blend_add";
		case spine::BlendMode_Multiply:
			return "blend_mul";
		case spine::BlendMode_Normal:
		default:
			return "blend_mix";
	}
}

static String remove_spine_sprite_3d_render_mode_token(String code, const String &token) {
	int render_mode_pos = code.find("render_mode");
	while (render_mode_pos >= 0) {
		const int statement_end = code.find(";", render_mode_pos);
		if (statement_end < 0) break;

		String statement = code.substr(render_mode_pos, statement_end - render_mode_pos + 1);
		statement = statement.replace(String(", ") + token, "");
		statement = statement.replace(token + String(", "), "");
		statement = statement.replace(token + String(","), "");
		statement = statement.replace(token, "");
		statement = statement.replace("render_mode ,", "render_mode ");
		statement = statement.replace("render_mode  ", "render_mode ");
		statement = statement.replace(", ;", ";");
		statement = statement.replace(",;", ";");

		code = code.substr(0, render_mode_pos) + statement + code.substr(statement_end + 1);
		render_mode_pos = code.find("render_mode", render_mode_pos + statement.length());
	}
	return code;
}

static String ensure_spine_sprite_3d_render_mode_token(String code, const String &token) {
	const int render_mode_pos = code.find("render_mode");
	if (render_mode_pos >= 0) {
		const int statement_end = code.find(";", render_mode_pos);
		if (statement_end >= 0) {
			const String statement = code.substr(render_mode_pos, statement_end - render_mode_pos + 1);
			if (statement.find(token) >= 0) return code;

			String prefix = code.substr(0, statement_end);
			const String stripped = statement.strip_edges();
			if (stripped == "render_mode" || stripped == "render_mode;") {
				prefix += " " + token;
			} else {
				prefix += ", " + token;
			}
			return prefix + code.substr(statement_end);
		}
	}

	const int shader_type_pos = code.find("shader_type");
	if (shader_type_pos >= 0) {
		const int shader_type_end = code.find(";", shader_type_pos);
		if (shader_type_end >= 0) {
			return code.substr(0, shader_type_end + 1) + "\nrender_mode " + token + ";\n" + code.substr(shader_type_end + 1);
		}
	}
	return "render_mode " + token + ";\n" + code;
}

static String normalize_spine_sprite_3d_custom_shader_render_modes(String code, spine::BlendMode blend_mode, bool lighting_enabled) {
	code = remove_spine_sprite_3d_render_mode_token(code, "cull_back");
	code = remove_spine_sprite_3d_render_mode_token(code, "cull_front");
	code = remove_spine_sprite_3d_render_mode_token(code, "cull_disabled");
	code = ensure_spine_sprite_3d_render_mode_token(code, "cull_disabled");

	code = remove_spine_sprite_3d_render_mode_token(code, "blend_mix");
	code = remove_spine_sprite_3d_render_mode_token(code, "blend_add");
	code = remove_spine_sprite_3d_render_mode_token(code, "blend_sub");
	code = remove_spine_sprite_3d_render_mode_token(code, "blend_mul");
	code = remove_spine_sprite_3d_render_mode_token(code, "blend_premul_alpha");
	code = remove_spine_sprite_3d_render_mode_token(code, "blend_disabled");
	code = ensure_spine_sprite_3d_render_mode_token(code, get_spine_sprite_3d_blend_render_mode(blend_mode));

	code = remove_spine_sprite_3d_render_mode_token(code, "depth_draw_opaque");
	code = remove_spine_sprite_3d_render_mode_token(code, "depth_draw_always");
	code = remove_spine_sprite_3d_render_mode_token(code, "depth_draw_never");
	code = remove_spine_sprite_3d_render_mode_token(code, "depth_prepass_alpha");
	code = ensure_spine_sprite_3d_render_mode_token(code, "depth_prepass_alpha");

	code = remove_spine_sprite_3d_render_mode_token(code, "unshaded");
	if (!lighting_enabled) {
		code = ensure_spine_sprite_3d_render_mode_token(code, "unshaded");
	}

	return code;
}

static String make_spine_sprite_3d_custom_shader_code(const String &source_code, spine::BlendMode blend_mode, bool lighting_enabled, bool double_sided_geometry) {
	String code = source_code;
	code = normalize_spine_sprite_3d_custom_shader_render_modes(code, blend_mode, lighting_enabled);
	if (double_sided_geometry && code.find("CUSTOM1.x") < 0) {
		const int vertex_pos = code.find("void vertex()");
		if (vertex_pos >= 0) {
			const int body_pos = code.find("{", vertex_pos);
			if (body_pos >= 0) {
				code = code.substr(0, body_pos + 1) + "\n" + make_spine_sprite_3d_view_stack_vertex_code() + code.substr(body_pos + 1);
			}
		} else {
			const int fragment_pos = code.find("void fragment()");
			const String vertex_function = "void vertex() {\n" + make_spine_sprite_3d_view_stack_vertex_code() + "}\n";
			if (fragment_pos >= 0) {
				code = code.substr(0, fragment_pos) + vertex_function + code.substr(fragment_pos);
			} else {
				code += "\n" + vertex_function;
			}
		}
		const int fragment_pos = code.find("void fragment()");
		if (fragment_pos >= 0) {
			const int body_pos = code.find("{", fragment_pos);
			if (body_pos >= 0) {
				code = code.substr(0, body_pos + 1) + "\n" + make_spine_sprite_3d_double_sided_lighting_code() + code.substr(body_pos + 1);
			}
		}
	}
	int alpha_pos = code.find("ALPHA");
	while (alpha_pos >= 0) {
		int assign_pos = alpha_pos + 5;
		while (assign_pos < code.length() && (code[assign_pos] == ' ' || code[assign_pos] == '\t')) {
			assign_pos++;
		}
		if (assign_pos >= code.length() || code[assign_pos] != '=') {
			alpha_pos = code.find("ALPHA", alpha_pos + 5);
			continue;
		}
		int statement_end = code.find(";", assign_pos);
		if (statement_end < 0) {
			break;
		}
		const String alpha_expression = code.substr(assign_pos + 1, statement_end - assign_pos - 1).strip_edges();
		const String alpha_assignment = "ALPHA = " + alpha_expression + ";\n	if (ALPHA <= spine_alpha_cutoff) discard";
		code = code.substr(0, alpha_pos) + alpha_assignment + code.substr(statement_end);
		alpha_pos = code.find("ALPHA", alpha_pos + alpha_assignment.length());
	}
	if (code.find("ALPHA") < 0) {
		code = code.replace(", blend_mix", "");
		code = code.replace("blend_mix, ", "");
		code = code.replace("blend_mix", "");
	}
	if (code.find("spine_alpha_cutoff") >= 0 && code.find("uniform float spine_alpha_cutoff") < 0) {
		const int shader_type_pos = code.find("shader_type");
		if (shader_type_pos >= 0) {
			const int shader_type_end = code.find(";", shader_type_pos);
			if (shader_type_end >= 0) {
				code = code.substr(0, shader_type_end + 1) + "\nuniform float spine_alpha_cutoff = 0.0;\n" + code.substr(shader_type_end + 1);
			}
		}
	}
	return code;
}

static void apply_spine_sprite_3d_base_material_contract(Ref<BaseMaterial3D> base_material, spine::BlendMode blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, bool lighting_enabled, bool generated_normal_map_enabled, float generated_normal_scale, float generated_standard_specular, float generated_standard_roughness, float generated_standard_metallic, float generated_shader_light_scale, float generated_shader_ambient) {
	if (!base_material.is_valid()) return;

	base_material->set_shading_mode(lighting_enabled ? BaseMaterial3D::SHADING_MODE_PER_PIXEL : BaseMaterial3D::SHADING_MODE_UNSHADED);
	base_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	base_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	base_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
	base_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
	base_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
	base_material->set("alpha_scissor_threshold", 0.0f);

	Color albedo = base_material->get_albedo();
	base_material->set_albedo(Color(albedo.r * generated_shader_light_scale, albedo.g * generated_shader_light_scale, albedo.b * generated_shader_light_scale, albedo.a));

	if (generated_shader_ambient > 0.0f) {
		base_material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
		base_material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, texture);
		base_material->set_emission(Color(1.0f, 1.0f, 1.0f));
		base_material->set_emission_energy_multiplier(generated_shader_ambient);
	}

	if (generated_normal_map_enabled && normal_map.is_valid()) {
		base_material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
		base_material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, normal_map);
		base_material->set_normal_scale(generated_normal_scale);
	}

	if (lighting_enabled) {
		base_material->set_metallic(generated_standard_metallic);
		base_material->set_specular(generated_standard_specular);
		base_material->set_roughness(generated_standard_roughness);
		base_material->set_specular_mode(generated_standard_specular <= 0.000001f ? BaseMaterial3D::SPECULAR_DISABLED : BaseMaterial3D::SPECULAR_SCHLICK_GGX);
	}

	switch (blend_mode) {
		case spine::BlendMode_Additive:
		case spine::BlendMode_Screen:
			base_material->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD);
			break;
		case spine::BlendMode_Multiply:
			base_material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MUL);
			break;
		case spine::BlendMode_Normal:
		default:
			base_material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);
			break;
	}
}

static void set_spine_sprite_3d_shader_material_parameters(const Ref<ShaderMaterial> &material, const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, bool use_normal_texture, float normal_scale, float normal_max_slope, float normal_dark_suppression, float light_scale, float ambient, float specular, float roughness, float metallic, float alpha_cutoff) {
	if (!material.is_valid()) return;

	material->set_shader_parameter(StringName("spine_texture"), diffuse_texture);
	material->set_shader_parameter(StringName("texture_albedo"), diffuse_texture);
	material->set_shader_parameter(StringName("albedo_texture"), diffuse_texture);
	material->set_shader_parameter(StringName("diffuse_texture"), diffuse_texture);
	material->set_shader_parameter(StringName("spine_normal_texture"), normal_texture);
	material->set_shader_parameter(StringName("texture_normal"), normal_texture);
	material->set_shader_parameter(StringName("normal_texture"), normal_texture);
	material->set_shader_parameter(StringName("normal_map"), normal_texture);
	material->set_shader_parameter(StringName("spine_use_normal_texture"), use_normal_texture);
	material->set_shader_parameter(StringName("use_normal_texture"), use_normal_texture);
	material->set_shader_parameter(StringName("spine_normal_scale"), normal_scale);
	material->set_shader_parameter(StringName("normal_scale"), normal_scale);
	material->set_shader_parameter(StringName("spine_normal_max_slope"), normal_max_slope);
	material->set_shader_parameter(StringName("spine_normal_dark_suppression"), normal_dark_suppression);
	material->set_shader_parameter(StringName("spine_light_scale"), light_scale);
	material->set_shader_parameter(StringName("light_scale"), light_scale);
	material->set_shader_parameter(StringName("spine_ambient"), ambient);
	material->set_shader_parameter(StringName("ambient"), ambient);
	material->set_shader_parameter(StringName("spine_specular"), specular);
	material->set_shader_parameter(StringName("specular"), specular);
	material->set_shader_parameter(StringName("spine_roughness"), roughness);
	material->set_shader_parameter(StringName("roughness"), roughness);
	material->set_shader_parameter(StringName("spine_metallic"), metallic);
	material->set_shader_parameter(StringName("metallic"), metallic);
	material->set_shader_parameter(StringName("spine_alpha_cutoff"), alpha_cutoff);
	material->set_shader_parameter(StringName("alpha_cutoff"), alpha_cutoff);
}

static Ref<ShaderMaterial> make_two_color_material(spine::BlendMode blend_mode, const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, int render_priority, bool lighting_enabled, bool double_sided_geometry, bool use_normal_texture, float normal_scale, float standard_specular, float standard_roughness, float standard_metallic, float light_scale, float ambient, float alpha_cutoff) {
	Ref<Shader> shader = memnew(Shader);
	String render_mode = "render_mode ";
	if (!lighting_enabled) {
		render_mode += "unshaded, ";
	}
	render_mode += "cull_disabled, ";
	render_mode += "depth_draw_never";
	switch (blend_mode) {
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
			+ (double_sided_geometry ? make_spine_sprite_3d_view_stack_vertex_code() : String()) +
			"	spine_dark_color = CUSTOM0;\n"
			"}\n"
			"void fragment() {\n"
			+ (double_sided_geometry ? make_spine_sprite_3d_double_sided_lighting_code() : String()) +
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

static Ref<ShaderMaterial> make_single_color_material(spine::BlendMode blend_mode, const Ref<Texture2D> &diffuse_texture, const Ref<Texture2D> &normal_texture, int render_priority, bool lighting_enabled, bool double_sided_geometry, bool use_normal_texture, float normal_scale, float standard_specular, float standard_roughness, float standard_metallic, float light_scale, float ambient, float alpha_cutoff) {
	Ref<Shader> shader = memnew(Shader);
	String render_mode = "render_mode ";
	if (!lighting_enabled) {
		render_mode += "unshaded, ";
	}
	render_mode += "cull_disabled, depth_prepass_alpha";
	switch (blend_mode) {
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
			"void vertex() {\n"
			+ (double_sided_geometry ? make_spine_sprite_3d_view_stack_vertex_code() : String()) +
			"}\n"
			"void fragment() {\n"
			+ (double_sided_geometry ? make_spine_sprite_3d_double_sided_lighting_code() : String()) +
			"	vec4 diffuse_sample = texture(spine_texture, UV);\n"
			"	ALBEDO = diffuse_sample.rgb * COLOR.rgb * spine_light_scale;\n"
			"	ALPHA = diffuse_sample.a * COLOR.a;\n"
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

}

void SpineSprite3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_skeleton_data_res", "skeleton_data_res"), &SpineSprite3D::set_skeleton_data_res);
	ClassDB::bind_method(D_METHOD("get_skeleton_data_res"), &SpineSprite3D::get_skeleton_data_res);
	ClassDB::bind_method(D_METHOD("get_skeleton"), &SpineSprite3D::get_skeleton);
	ClassDB::bind_method(D_METHOD("get_animation_state"), &SpineSprite3D::get_animation_state);
	ClassDB::bind_method(D_METHOD("emit_animation_state_ready_deferred"), &SpineSprite3D::emit_animation_state_ready_deferred);
	ClassDB::bind_method(D_METHOD("on_internal_spine_objects_invalidated"), &SpineSprite3D::on_internal_spine_objects_invalidated);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_started", "spine_sprite", "animation_state", "track_entry"), &SpineSprite3D::on_sprite_animation_started);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_interrupted", "spine_sprite", "animation_state", "track_entry"), &SpineSprite3D::on_sprite_animation_interrupted);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_ended", "spine_sprite", "animation_state", "track_entry"), &SpineSprite3D::on_sprite_animation_ended);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_completed", "spine_sprite", "animation_state", "track_entry"), &SpineSprite3D::on_sprite_animation_completed);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_disposed", "spine_sprite", "animation_state", "track_entry"), &SpineSprite3D::on_sprite_animation_disposed);
	ClassDB::bind_method(D_METHOD("on_sprite_animation_event", "spine_sprite", "animation_state", "track_entry", "event"), &SpineSprite3D::on_sprite_animation_event);
	ClassDB::bind_method(D_METHOD("on_sprite_before_animation_state_update", "spine_sprite"), &SpineSprite3D::on_sprite_before_animation_state_update);
	ClassDB::bind_method(D_METHOD("on_sprite_before_animation_state_apply", "spine_sprite"), &SpineSprite3D::on_sprite_before_animation_state_apply);
	ClassDB::bind_method(D_METHOD("on_sprite_before_world_transforms_change", "spine_sprite"), &SpineSprite3D::on_sprite_before_world_transforms_change);
	ClassDB::bind_method(D_METHOD("on_sprite_world_transforms_changed", "spine_sprite"), &SpineSprite3D::on_sprite_world_transforms_changed);
	ClassDB::bind_method(D_METHOD("set_animation", "animation_name", "loop", "track_id"), &SpineSprite3D::set_animation, DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("add_animation", "animation_name", "delay", "loop", "track_id"), &SpineSprite3D::add_animation, DEFVAL(0), DEFVAL(true), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_empty_animation", "track_id", "mix_duration"), &SpineSprite3D::set_empty_animation);
	ClassDB::bind_method(D_METHOD("add_empty_animation", "track_id", "mix_duration", "delay"), &SpineSprite3D::add_empty_animation);
	ClassDB::bind_method(D_METHOD("set_empty_animations", "mix_duration"), &SpineSprite3D::set_empty_animations);
	ClassDB::bind_method(D_METHOD("clear_tracks"), &SpineSprite3D::clear_tracks);
	ClassDB::bind_method(D_METHOD("clear_track", "track_id"), &SpineSprite3D::clear_track);
	ClassDB::bind_method(D_METHOD("on_skeleton_data_changed"), &SpineSprite3D::on_skeleton_data_changed);
	ClassDB::bind_method(D_METHOD("update_skeleton", "delta"), &SpineSprite3D::update_skeleton);
	ClassDB::bind_method(D_METHOD("new_skin", "name"), &SpineSprite3D::new_skin);
	ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &SpineSprite3D::set_update_mode);
	ClassDB::bind_method(D_METHOD("get_update_mode"), &SpineSprite3D::get_update_mode);
	ClassDB::bind_method(D_METHOD("set_time_scale", "v"), &SpineSprite3D::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &SpineSprite3D::get_time_scale);
	ClassDB::bind_method(D_METHOD("set_pixels_per_unit", "v"), &SpineSprite3D::set_pixels_per_unit);
	ClassDB::bind_method(D_METHOD("get_pixels_per_unit"), &SpineSprite3D::get_pixels_per_unit);
	ClassDB::bind_method(D_METHOD("set_depth_offset", "v"), &SpineSprite3D::set_depth_offset);
	ClassDB::bind_method(D_METHOD("get_depth_offset"), &SpineSprite3D::get_depth_offset);
	ClassDB::bind_method(D_METHOD("set_render_embedded_in_parent_slot", "v"), &SpineSprite3D::set_render_embedded_in_parent_slot);
	ClassDB::bind_method(D_METHOD("is_render_embedded_in_parent_slot"), &SpineSprite3D::is_render_embedded_in_parent_slot);
	ClassDB::bind_method(D_METHOD("set_render_priority_bias", "v"), &SpineSprite3D::set_render_priority_bias);
	ClassDB::bind_method(D_METHOD("get_render_priority_bias"), &SpineSprite3D::get_render_priority_bias);
	ClassDB::bind_method(D_METHOD("set_render_priority_range", "v"), &SpineSprite3D::set_render_priority_range);
	ClassDB::bind_method(D_METHOD("get_render_priority_range"), &SpineSprite3D::get_render_priority_range);
	ClassDB::bind_method(D_METHOD("set_render_backend", "v"), &SpineSprite3D::set_render_backend);
	ClassDB::bind_method(D_METHOD("get_render_backend"), &SpineSprite3D::get_render_backend);
	ClassDB::bind_method(D_METHOD("is_stream_renderer_enabled"), &SpineSprite3D::is_stream_renderer_enabled);
	ClassDB::bind_method(D_METHOD("set_stream_render_order", "v"), &SpineSprite3D::set_stream_render_order);
	ClassDB::bind_method(D_METHOD("get_stream_render_order"), &SpineSprite3D::get_stream_render_order);
	ClassDB::bind_method(D_METHOD("set_lighting_enabled", "v"), &SpineSprite3D::set_lighting_enabled);
	ClassDB::bind_method(D_METHOD("is_lighting_enabled"), &SpineSprite3D::is_lighting_enabled);
	ClassDB::bind_method(D_METHOD("set_double_sided", "v"), &SpineSprite3D::set_double_sided);
	ClassDB::bind_method(D_METHOD("is_double_sided"), &SpineSprite3D::is_double_sided);
	ClassDB::bind_method(D_METHOD("set_shadow_casting_mode", "v"), &SpineSprite3D::set_shadow_casting_mode);
	ClassDB::bind_method(D_METHOD("get_shadow_casting_mode"), &SpineSprite3D::get_shadow_casting_mode);
	ClassDB::bind_method(D_METHOD("set_shadow_alpha_cutoff", "v"), &SpineSprite3D::set_shadow_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("get_shadow_alpha_cutoff"), &SpineSprite3D::get_shadow_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("set_visible_alpha_cutoff", "v"), &SpineSprite3D::set_visible_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("get_visible_alpha_cutoff"), &SpineSprite3D::get_visible_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("set_generated_material_mode", "v"), &SpineSprite3D::set_generated_material_mode);
	ClassDB::bind_method(D_METHOD("get_generated_material_mode"), &SpineSprite3D::get_generated_material_mode);
	ClassDB::bind_method(D_METHOD("set_generated_normal_map_enabled", "v"), &SpineSprite3D::set_generated_normal_map_enabled);
	ClassDB::bind_method(D_METHOD("is_generated_normal_map_enabled"), &SpineSprite3D::is_generated_normal_map_enabled);
	ClassDB::bind_method(D_METHOD("set_generated_normal_map_preview", "v"), &SpineSprite3D::set_generated_normal_map_preview);
	ClassDB::bind_method(D_METHOD("is_generated_normal_map_preview"), &SpineSprite3D::is_generated_normal_map_preview);
	ClassDB::bind_method(D_METHOD("set_generated_normal_map_flip_y", "v"), &SpineSprite3D::set_generated_normal_map_flip_y);
	ClassDB::bind_method(D_METHOD("is_generated_normal_map_flip_y"), &SpineSprite3D::is_generated_normal_map_flip_y);
	ClassDB::bind_method(D_METHOD("set_generated_normal_map_override", "texture"), &SpineSprite3D::set_generated_normal_map_override);
	ClassDB::bind_method(D_METHOD("get_generated_normal_map_override"), &SpineSprite3D::get_generated_normal_map_override);
	ClassDB::bind_method(D_METHOD("set_generated_normal_scale", "v"), &SpineSprite3D::set_generated_normal_scale);
	ClassDB::bind_method(D_METHOD("get_generated_normal_scale"), &SpineSprite3D::get_generated_normal_scale);
	ClassDB::bind_method(D_METHOD("set_generated_normal_max_slope", "v"), &SpineSprite3D::set_generated_normal_max_slope);
	ClassDB::bind_method(D_METHOD("get_generated_normal_max_slope"), &SpineSprite3D::get_generated_normal_max_slope);
	ClassDB::bind_method(D_METHOD("set_generated_normal_dark_suppression", "v"), &SpineSprite3D::set_generated_normal_dark_suppression);
	ClassDB::bind_method(D_METHOD("get_generated_normal_dark_suppression"), &SpineSprite3D::get_generated_normal_dark_suppression);
	ClassDB::bind_method(D_METHOD("set_generated_standard_specular", "v"), &SpineSprite3D::set_generated_standard_specular);
	ClassDB::bind_method(D_METHOD("get_generated_standard_specular"), &SpineSprite3D::get_generated_standard_specular);
	ClassDB::bind_method(D_METHOD("set_generated_standard_roughness", "v"), &SpineSprite3D::set_generated_standard_roughness);
	ClassDB::bind_method(D_METHOD("get_generated_standard_roughness"), &SpineSprite3D::get_generated_standard_roughness);
	ClassDB::bind_method(D_METHOD("set_generated_standard_metallic", "v"), &SpineSprite3D::set_generated_standard_metallic);
	ClassDB::bind_method(D_METHOD("get_generated_standard_metallic"), &SpineSprite3D::get_generated_standard_metallic);
	ClassDB::bind_method(D_METHOD("set_generated_shader_light_scale", "v"), &SpineSprite3D::set_generated_shader_light_scale);
	ClassDB::bind_method(D_METHOD("get_generated_shader_light_scale"), &SpineSprite3D::get_generated_shader_light_scale);
	ClassDB::bind_method(D_METHOD("set_generated_shader_ambient", "v"), &SpineSprite3D::set_generated_shader_ambient);
	ClassDB::bind_method(D_METHOD("get_generated_shader_ambient"), &SpineSprite3D::get_generated_shader_ambient);
	ClassDB::bind_method(D_METHOD("set_legacy_position_conversion_enabled", "v"), &SpineSprite3D::set_legacy_position_conversion_enabled);
	ClassDB::bind_method(D_METHOD("is_legacy_position_conversion_enabled"), &SpineSprite3D::is_legacy_position_conversion_enabled);
	ClassDB::bind_method(D_METHOD("set_diagnostics_enabled", "v"), &SpineSprite3D::set_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("is_diagnostics_enabled"), &SpineSprite3D::is_diagnostics_enabled);
	ClassDB::bind_method(D_METHOD("get_global_bone_transform", "bone_name"), &SpineSprite3D::get_global_bone_transform);
	ClassDB::bind_method(D_METHOD("set_global_bone_transform", "bone_name", "global_transform"), &SpineSprite3D::set_global_bone_transform);

	ClassDB::bind_method(D_METHOD("set_normal_material", "material"), &SpineSprite3D::set_normal_material);
	ClassDB::bind_method(D_METHOD("get_normal_material"), &SpineSprite3D::get_normal_material);
	ClassDB::bind_method(D_METHOD("set_additive_material", "material"), &SpineSprite3D::set_additive_material);
	ClassDB::bind_method(D_METHOD("get_additive_material"), &SpineSprite3D::get_additive_material);
	ClassDB::bind_method(D_METHOD("set_multiply_material", "material"), &SpineSprite3D::set_multiply_material);
	ClassDB::bind_method(D_METHOD("get_multiply_material"), &SpineSprite3D::get_multiply_material);
	ClassDB::bind_method(D_METHOD("set_screen_material", "material"), &SpineSprite3D::set_screen_material);
	ClassDB::bind_method(D_METHOD("get_screen_material"), &SpineSprite3D::get_screen_material);

	ClassDB::bind_method(D_METHOD("set_debug_root", "v"), &SpineSprite3D::set_debug_root);
	ClassDB::bind_method(D_METHOD("get_debug_root"), &SpineSprite3D::get_debug_root);
	ClassDB::bind_method(D_METHOD("set_debug_root_color", "v"), &SpineSprite3D::set_debug_root_color);
	ClassDB::bind_method(D_METHOD("get_debug_root_color"), &SpineSprite3D::get_debug_root_color);
	ClassDB::bind_method(D_METHOD("set_debug_bones", "v"), &SpineSprite3D::set_debug_bones);
	ClassDB::bind_method(D_METHOD("get_debug_bones"), &SpineSprite3D::get_debug_bones);
	ClassDB::bind_method(D_METHOD("set_debug_bones_color", "v"), &SpineSprite3D::set_debug_bones_color);
	ClassDB::bind_method(D_METHOD("get_debug_bones_color"), &SpineSprite3D::get_debug_bones_color);
	ClassDB::bind_method(D_METHOD("set_debug_bones_thickness", "v"), &SpineSprite3D::set_debug_bones_thickness);
	ClassDB::bind_method(D_METHOD("get_debug_bones_thickness"), &SpineSprite3D::get_debug_bones_thickness);
	ClassDB::bind_method(D_METHOD("set_debug_regions", "v"), &SpineSprite3D::set_debug_regions);
	ClassDB::bind_method(D_METHOD("get_debug_regions"), &SpineSprite3D::get_debug_regions);
	ClassDB::bind_method(D_METHOD("set_debug_regions_color", "v"), &SpineSprite3D::set_debug_regions_color);
	ClassDB::bind_method(D_METHOD("get_debug_regions_color"), &SpineSprite3D::get_debug_regions_color);
	ClassDB::bind_method(D_METHOD("set_debug_meshes", "v"), &SpineSprite3D::set_debug_meshes);
	ClassDB::bind_method(D_METHOD("get_debug_meshes"), &SpineSprite3D::get_debug_meshes);
	ClassDB::bind_method(D_METHOD("set_debug_meshes_color", "v"), &SpineSprite3D::set_debug_meshes_color);
	ClassDB::bind_method(D_METHOD("get_debug_meshes_color"), &SpineSprite3D::get_debug_meshes_color);
	ClassDB::bind_method(D_METHOD("set_debug_bounding_boxes", "v"), &SpineSprite3D::set_debug_bounding_boxes);
	ClassDB::bind_method(D_METHOD("get_debug_bounding_boxes"), &SpineSprite3D::get_debug_bounding_boxes);
	ClassDB::bind_method(D_METHOD("set_debug_bounding_boxes_color", "v"), &SpineSprite3D::set_debug_bounding_boxes_color);
	ClassDB::bind_method(D_METHOD("get_debug_bounding_boxes_color"), &SpineSprite3D::get_debug_bounding_boxes_color);
	ClassDB::bind_method(D_METHOD("set_debug_paths", "v"), &SpineSprite3D::set_debug_paths);
	ClassDB::bind_method(D_METHOD("get_debug_paths"), &SpineSprite3D::get_debug_paths);
	ClassDB::bind_method(D_METHOD("set_debug_paths_color", "v"), &SpineSprite3D::set_debug_paths_color);
	ClassDB::bind_method(D_METHOD("get_debug_paths_color"), &SpineSprite3D::get_debug_paths_color);
	ClassDB::bind_method(D_METHOD("set_debug_clipping", "v"), &SpineSprite3D::set_debug_clipping);
	ClassDB::bind_method(D_METHOD("get_debug_clipping"), &SpineSprite3D::get_debug_clipping);
	ClassDB::bind_method(D_METHOD("set_debug_clipping_color", "v"), &SpineSprite3D::set_debug_clipping_color);
	ClassDB::bind_method(D_METHOD("get_debug_clipping_color"), &SpineSprite3D::get_debug_clipping_color);

	ADD_SIGNAL(MethodInfo("animation_started", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry")));
	ADD_SIGNAL(MethodInfo("animation_interrupted", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry")));
	ADD_SIGNAL(MethodInfo("animation_ended", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry")));
	ADD_SIGNAL(MethodInfo("animation_completed", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry")));
	ADD_SIGNAL(MethodInfo("animation_disposed", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry")));
	ADD_SIGNAL(MethodInfo("animation_event", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state"), PropertyInfo(Variant::OBJECT, "track_entry"), PropertyInfo(Variant::OBJECT, "event")));
	ADD_SIGNAL(MethodInfo("before_animation_state_update", PropertyInfo(Variant::OBJECT, "spine_sprite_3d")));
	ADD_SIGNAL(MethodInfo("before_animation_state_apply", PropertyInfo(Variant::OBJECT, "spine_sprite_3d")));
	ADD_SIGNAL(MethodInfo("before_world_transforms_change", PropertyInfo(Variant::OBJECT, "spine_sprite_3d")));
	ADD_SIGNAL(MethodInfo("world_transforms_changed", PropertyInfo(Variant::OBJECT, "spine_sprite_3d")));
	ADD_SIGNAL(MethodInfo("animation_state_ready", PropertyInfo(Variant::OBJECT, "spine_sprite_3d"), PropertyInfo(Variant::OBJECT, "animation_state")));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "skeleton_data_res", PROPERTY_HINT_RESOURCE_TYPE, "SpineSkeletonDataResource"), "set_skeleton_data_res", "get_skeleton_data_res");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "update_mode", PROPERTY_HINT_ENUM, "Process,Physics,Manual"), "set_update_mode", "get_update_mode");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "time_scale", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "pixels_per_unit", PROPERTY_HINT_RANGE, "0.01,4096.0,0.01"), "set_pixels_per_unit", "get_pixels_per_unit");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "depth_offset", PROPERTY_HINT_RANGE, "0.0001,1.0,0.0001"), "set_depth_offset", "get_depth_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "render_embedded_in_parent_slot"), "set_render_embedded_in_parent_slot", "is_render_embedded_in_parent_slot");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority_bias", PROPERTY_HINT_RANGE, "-128,127,1"), "set_render_priority_bias", "get_render_priority_bias");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_priority_range", PROPERTY_HINT_RANGE, "0,127,1"), "set_render_priority_range", "get_render_priority_range");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_backend", PROPERTY_HINT_ENUM, "Reference Mesh,Stream World,Auto Stream World"), "set_render_backend", "get_render_backend");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "stream_render_order", PROPERTY_HINT_RANGE, "-524288,524287,1"), "set_stream_render_order", "get_stream_render_order");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lighting_enabled"), "set_lighting_enabled", "is_lighting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "double_sided"), "set_double_sided", "is_double_sided");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_casting_mode", PROPERTY_HINT_ENUM, "Off,On,Double Sided,Shadows Only"), "set_shadow_casting_mode", "get_shadow_casting_mode");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "shadow_alpha_cutoff", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_shadow_alpha_cutoff", "get_shadow_alpha_cutoff");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "visible_alpha_cutoff", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_visible_alpha_cutoff", "get_visible_alpha_cutoff");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "legacy_position_conversion_enabled"), "set_legacy_position_conversion_enabled", "is_legacy_position_conversion_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "diagnostics_enabled"), "set_diagnostics_enabled", "is_diagnostics_enabled");

	ADD_GROUP("Generated Material Debug", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "generated_material_mode", PROPERTY_HINT_ENUM, "StandardMaterial3D,StandardMaterial3D Safe"), "set_generated_material_mode", "get_generated_material_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generated_normal_map_enabled"), "set_generated_normal_map_enabled", "is_generated_normal_map_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generated_normal_map_preview"), "set_generated_normal_map_preview", "is_generated_normal_map_preview");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generated_normal_map_flip_y"), "set_generated_normal_map_flip_y", "is_generated_normal_map_flip_y");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "generated_normal_map_override", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_generated_normal_map_override", "get_generated_normal_map_override");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_normal_scale", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_generated_normal_scale", "get_generated_normal_scale");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_normal_max_slope", PROPERTY_HINT_RANGE, "0.05,1.0,0.01"), "set_generated_normal_max_slope", "get_generated_normal_max_slope");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_normal_dark_suppression", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_generated_normal_dark_suppression", "get_generated_normal_dark_suppression");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_standard_specular", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_generated_standard_specular", "get_generated_standard_specular");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_standard_roughness", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_generated_standard_roughness", "get_generated_standard_roughness");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_standard_metallic", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_generated_standard_metallic", "get_generated_standard_metallic");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_shader_light_scale", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_generated_shader_light_scale", "get_generated_shader_light_scale");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "generated_shader_ambient", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_generated_shader_ambient", "get_generated_shader_ambient");

	ADD_GROUP("Materials", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_normal_material", "get_normal_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "additive_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_additive_material", "get_additive_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiply_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_multiply_material", "get_multiply_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "screen_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_screen_material", "get_screen_material");

	ADD_GROUP("Debug", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "root"), "set_debug_root", "get_debug_root");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "root_color"), "set_debug_root_color", "get_debug_root_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bones"), "set_debug_bones", "get_debug_bones");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bones_color"), "set_debug_bones_color", "get_debug_bones_color");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "bones_thickness"), "set_debug_bones_thickness", "get_debug_bones_thickness");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "regions"), "set_debug_regions", "get_debug_regions");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "regions_color"), "set_debug_regions_color", "get_debug_regions_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meshes"), "set_debug_meshes", "get_debug_meshes");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "meshes_color"), "set_debug_meshes_color", "get_debug_meshes_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bounding_boxes"), "set_debug_bounding_boxes", "get_debug_bounding_boxes");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bounding_boxes_color"), "set_debug_bounding_boxes_color", "get_debug_bounding_boxes_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paths"), "set_debug_paths", "get_debug_paths");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "paths_color"), "set_debug_paths_color", "get_debug_paths_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clipping"), "set_debug_clipping", "get_debug_clipping");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "clipping_color"), "set_debug_clipping_color", "get_debug_clipping_color");

	ADD_GROUP("Preview", "");
}

SpineSprite3D::SpineSprite3D() :
		update_mode(SpineConstant::UpdateMode_Process),
		time_scale(1.0f),
		pixels_per_unit(100.0f),
		depth_offset(0.0005f),
		render_embedded_in_parent_slot(true),
		render_priority_bias(-48),
		render_priority_range(24),
		render_backend(RenderBackend_AutoStreamWorld),
		stream_render_order(0),
		lighting_enabled(false),
		double_sided(false),
		shadow_casting_mode(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF),
		shadow_alpha_cutoff(0.3f),
		visible_alpha_cutoff(0.0f),
		generated_material_mode(GeneratedMaterialMode_StandardMaterial3D),
		generated_normal_map_enabled(true),
		generated_normal_map_preview(false),
		generated_normal_map_flip_y(true),
		generated_normal_scale(0.65f),
		generated_normal_max_slope(0.55f),
		generated_normal_dark_suppression(0.45f),
		generated_standard_specular(0.25f),
		generated_standard_roughness(0.65f),
		generated_standard_metallic(0.0f),
		generated_shader_light_scale(1.0f),
		generated_shader_ambient(0.0f),
		debug_root(false),
		debug_root_color(Color(1, 1, 1, 0.5)),
		debug_bones(false),
		debug_bones_color(Color(1, 1, 0, 0.5)),
		debug_bones_thickness(5),
		debug_regions(false),
		debug_regions_color(Color(0, 0, 1, 0.5)),
		debug_meshes(false),
		debug_meshes_color(Color(0, 0, 1, 0.5)),
		debug_bounding_boxes(false),
		debug_bounding_boxes_color(Color(0, 1, 0, 0.5)),
		debug_paths(false),
		debug_paths_color(Color::hex(0xff7f0077)),
		debug_clipping(false),
		debug_clipping_color(Color(0.8, 0, 0, 0.8)),
		preview_skin("Default"),
		preview_animation("-- Empty --"),
		preview_frame(false),
		preview_time(0),
		legacy_position_conversion_enabled(true),
		diagnostics_enabled(false),
		diagnostics_counter(0),
		last_generated_slot_count(0),
		last_generated_run_count(0),
		animation_state_ready_emitted(false),
		sprite_signals_connected(false),
		last_animation_state_object(nullptr),
		sprite_2d(nullptr),
		mesh_instance_3d(nullptr),
		shadow_mesh_instance_3d(nullptr),
		debug_mesh_instance_3d(nullptr),
		skeleton_clipper(nullptr) {
#if VERSION_MAJOR > 3
	internal_spine_objects_invalidated_callable = Callable(this, SNAME("on_internal_spine_objects_invalidated"));
	animation_started_callable = Callable(this, SNAME("on_sprite_animation_started"));
	animation_interrupted_callable = Callable(this, SNAME("on_sprite_animation_interrupted"));
	animation_ended_callable = Callable(this, SNAME("on_sprite_animation_ended"));
	animation_completed_callable = Callable(this, SNAME("on_sprite_animation_completed"));
	animation_disposed_callable = Callable(this, SNAME("on_sprite_animation_disposed"));
	animation_event_callable = Callable(this, SNAME("on_sprite_animation_event"));
	before_animation_state_update_callable = Callable(this, SNAME("on_sprite_before_animation_state_update"));
	before_animation_state_apply_callable = Callable(this, SNAME("on_sprite_before_animation_state_apply"));
	before_world_transforms_change_callable = Callable(this, SNAME("on_sprite_before_world_transforms_change"));
	world_transforms_changed_callable = Callable(this, SNAME("on_sprite_world_transforms_changed"));
#endif
}

SpineSprite3D::~SpineSprite3D() {
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	if (skeleton_clipper) {
		delete skeleton_clipper;
		skeleton_clipper = nullptr;
	}
}

void SpineSprite3D::log_checkpoint(const String &message) const {
#ifdef SPINE_GODOT_EXTENSION
	if (diagnostics_enabled) UtilityFunctions::print(String("[SpineSprite3D] ") + message);
#endif
}

static void add_spine_sprite_3d_internal_child(Node *parent, Node *child) {
	if (!parent || !child) return;
#if VERSION_MAJOR > 3
	parent->add_child(child, false, Node::INTERNAL_MODE_BACK);
#else
	parent->add_child(child);
#endif
}

void SpineSprite3D::ensure_runtime_nodes() {
	if (!sprite_2d) {
		sprite_2d = Object::cast_to<SpineSprite>(get_node_or_null(NodePath("SpineSprite3DLogic")));
	}
	if (!sprite_2d) {
		sprite_2d = memnew(SpineSprite);
		sprite_2d->set_name("SpineSprite3DLogic");
		add_spine_sprite_3d_internal_child(this, sprite_2d);
	}
	sprite_2d->set_update_mode(SpineConstant::UpdateMode_Manual);
	sprite_2d->set_update_when_invisible(true);
	sprite_2d->set_mesh_rendering_enabled(false);
	sprite_2d->set_visible(false);
	sync_debug_settings_to_sprite();
	connect_sprite_signals();

	if (!mesh_instance_3d) {
		mesh_instance_3d = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("SpineSprite3DMesh")));
	}
	if (!mesh_instance_3d) {
		mesh_instance_3d = memnew(MeshInstance3D);
		mesh_instance_3d->set_name("SpineSprite3DMesh");
		add_spine_sprite_3d_internal_child(this, mesh_instance_3d);
	}
	if (!runtime_mesh.is_valid()) runtime_mesh.instantiate();
	mesh_instance_3d->set_mesh(runtime_mesh);
	mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mesh_instance_3d->set_extra_cull_margin(10.0f);
	mesh_instance_3d->set_sorting_use_aabb_center(false);
	const float sorting_jitter = (((float)((uint64_t)get_instance_id() & 0xff)) / 255.0f - 0.5f) * 0.00002f;
	mesh_instance_3d->set_sorting_offset(sorting_jitter);

	if (!shadow_mesh_instance_3d) {
		shadow_mesh_instance_3d = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("SpineSprite3DShadowMesh")));
	}
	if (!shadow_mesh_instance_3d) {
		shadow_mesh_instance_3d = memnew(MeshInstance3D);
		shadow_mesh_instance_3d->set_name("SpineSprite3DShadowMesh");
		add_spine_sprite_3d_internal_child(this, shadow_mesh_instance_3d);
	}
	if (!shadow_runtime_mesh.is_valid()) shadow_runtime_mesh.instantiate();
	shadow_mesh_instance_3d->set_mesh(shadow_runtime_mesh);
	shadow_mesh_instance_3d->set_extra_cull_margin(10.0f);
	shadow_mesh_instance_3d->set_sorting_use_aabb_center(false);
	shadow_mesh_instance_3d->set_sorting_offset(sorting_jitter);
	apply_shadow_mesh_state();

	if (!debug_mesh_instance_3d) {
		debug_mesh_instance_3d = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("SpineSprite3DDebugMesh")));
	}
	if (!debug_mesh_instance_3d) {
		debug_mesh_instance_3d = memnew(MeshInstance3D);
		debug_mesh_instance_3d->set_name("SpineSprite3DDebugMesh");
		add_spine_sprite_3d_internal_child(this, debug_mesh_instance_3d);
	}
	if (!debug_runtime_mesh.is_valid()) debug_runtime_mesh.instantiate();
	if (!debug_material.is_valid()) {
		debug_material.instantiate();
		debug_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		debug_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		debug_material->set_render_priority(127);
		debug_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		debug_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
		debug_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		debug_material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);
	}
	debug_mesh_instance_3d->set_mesh(debug_runtime_mesh);
	debug_mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	debug_mesh_instance_3d->set_extra_cull_margin(10.0f);
	debug_mesh_instance_3d->set_sorting_use_aabb_center(false);
	debug_mesh_instance_3d->set_sorting_offset(sorting_jitter + 0.00001f);
	debug_mesh_instance_3d->set_visible(false);

	if (!skeleton_clipper) skeleton_clipper = new spine::SkeletonClipping();
	set_notify_transform(true);
	set_notify_local_transform(true);
	log_checkpoint("runtime nodes created");
}

void SpineSprite3D::apply_shadow_mesh_state() {
	if (!shadow_mesh_instance_3d) return;
	const bool streaming_to_world = should_stream_to_world();
	if (mesh_instance_3d) mesh_instance_3d->set_visible(!streaming_to_world && shadow_casting_mode != GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
	if (streaming_to_world) {
		shadow_mesh_instance_3d->set_visible(false);
		shadow_mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		return;
	}
	if (shadow_casting_mode == GeometryInstance3D::SHADOW_CASTING_SETTING_OFF) {
		shadow_mesh_instance_3d->set_visible(false);
		shadow_mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		return;
	}
	shadow_mesh_instance_3d->set_visible(true);
	shadow_mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY);
}

void SpineSprite3D::connect_sprite_signals() {
	if (!sprite_2d) return;
	if (sprite_signals_connected) return;
#if VERSION_MAJOR > 3
	if (!sprite_2d->is_connected(SNAME("_internal_spine_objects_invalidated"), internal_spine_objects_invalidated_callable))
		sprite_2d->connect(SNAME("_internal_spine_objects_invalidated"), internal_spine_objects_invalidated_callable);
	if (!sprite_2d->is_connected(SNAME("animation_started"), animation_started_callable))
		sprite_2d->connect(SNAME("animation_started"), animation_started_callable);
	if (!sprite_2d->is_connected(SNAME("animation_interrupted"), animation_interrupted_callable))
		sprite_2d->connect(SNAME("animation_interrupted"), animation_interrupted_callable);
	if (!sprite_2d->is_connected(SNAME("animation_ended"), animation_ended_callable))
		sprite_2d->connect(SNAME("animation_ended"), animation_ended_callable);
	if (!sprite_2d->is_connected(SNAME("animation_completed"), animation_completed_callable))
		sprite_2d->connect(SNAME("animation_completed"), animation_completed_callable);
	if (!sprite_2d->is_connected(SNAME("animation_disposed"), animation_disposed_callable))
		sprite_2d->connect(SNAME("animation_disposed"), animation_disposed_callable);
	if (!sprite_2d->is_connected(SNAME("animation_event"), animation_event_callable))
		sprite_2d->connect(SNAME("animation_event"), animation_event_callable);
	if (!sprite_2d->is_connected(SNAME("before_animation_state_update"), before_animation_state_update_callable))
		sprite_2d->connect(SNAME("before_animation_state_update"), before_animation_state_update_callable);
	if (!sprite_2d->is_connected(SNAME("before_animation_state_apply"), before_animation_state_apply_callable))
		sprite_2d->connect(SNAME("before_animation_state_apply"), before_animation_state_apply_callable);
	if (!sprite_2d->is_connected(SNAME("before_world_transforms_change"), before_world_transforms_change_callable))
		sprite_2d->connect(SNAME("before_world_transforms_change"), before_world_transforms_change_callable);
	if (!sprite_2d->is_connected(SNAME("world_transforms_changed"), world_transforms_changed_callable))
		sprite_2d->connect(SNAME("world_transforms_changed"), world_transforms_changed_callable);
#else
	if (!sprite_2d->is_connected(SNAME("_internal_spine_objects_invalidated"), this, SNAME("on_internal_spine_objects_invalidated")))
		sprite_2d->connect(SNAME("_internal_spine_objects_invalidated"), this, SNAME("on_internal_spine_objects_invalidated"));
	if (!sprite_2d->is_connected(SNAME("animation_started"), this, SNAME("on_sprite_animation_started")))
		sprite_2d->connect(SNAME("animation_started"), this, SNAME("on_sprite_animation_started"));
	if (!sprite_2d->is_connected(SNAME("animation_interrupted"), this, SNAME("on_sprite_animation_interrupted")))
		sprite_2d->connect(SNAME("animation_interrupted"), this, SNAME("on_sprite_animation_interrupted"));
	if (!sprite_2d->is_connected(SNAME("animation_ended"), this, SNAME("on_sprite_animation_ended")))
		sprite_2d->connect(SNAME("animation_ended"), this, SNAME("on_sprite_animation_ended"));
	if (!sprite_2d->is_connected(SNAME("animation_completed"), this, SNAME("on_sprite_animation_completed")))
		sprite_2d->connect(SNAME("animation_completed"), this, SNAME("on_sprite_animation_completed"));
	if (!sprite_2d->is_connected(SNAME("animation_disposed"), this, SNAME("on_sprite_animation_disposed")))
		sprite_2d->connect(SNAME("animation_disposed"), this, SNAME("on_sprite_animation_disposed"));
	if (!sprite_2d->is_connected(SNAME("animation_event"), this, SNAME("on_sprite_animation_event")))
		sprite_2d->connect(SNAME("animation_event"), this, SNAME("on_sprite_animation_event"));
	if (!sprite_2d->is_connected(SNAME("before_animation_state_update"), this, SNAME("on_sprite_before_animation_state_update")))
		sprite_2d->connect(SNAME("before_animation_state_update"), this, SNAME("on_sprite_before_animation_state_update"));
	if (!sprite_2d->is_connected(SNAME("before_animation_state_apply"), this, SNAME("on_sprite_before_animation_state_apply")))
		sprite_2d->connect(SNAME("before_animation_state_apply"), this, SNAME("on_sprite_before_animation_state_apply"));
	if (!sprite_2d->is_connected(SNAME("before_world_transforms_change"), this, SNAME("on_sprite_before_world_transforms_change")))
		sprite_2d->connect(SNAME("before_world_transforms_change"), this, SNAME("on_sprite_before_world_transforms_change"));
	if (!sprite_2d->is_connected(SNAME("world_transforms_changed"), this, SNAME("on_sprite_world_transforms_changed")))
		sprite_2d->connect(SNAME("world_transforms_changed"), this, SNAME("on_sprite_world_transforms_changed"));
#endif
	sprite_signals_connected = true;
}

void SpineSprite3D::disconnect_sprite_signals() {
	if (!sprite_2d) return;
	if (!sprite_signals_connected) return;
	sprite_signals_connected = false;
#if VERSION_MAJOR > 3
	if (sprite_2d->is_connected(SNAME("_internal_spine_objects_invalidated"), internal_spine_objects_invalidated_callable))
		sprite_2d->disconnect(SNAME("_internal_spine_objects_invalidated"), internal_spine_objects_invalidated_callable);
	if (sprite_2d->is_connected(SNAME("animation_started"), animation_started_callable))
		sprite_2d->disconnect(SNAME("animation_started"), animation_started_callable);
	if (sprite_2d->is_connected(SNAME("animation_interrupted"), animation_interrupted_callable))
		sprite_2d->disconnect(SNAME("animation_interrupted"), animation_interrupted_callable);
	if (sprite_2d->is_connected(SNAME("animation_ended"), animation_ended_callable))
		sprite_2d->disconnect(SNAME("animation_ended"), animation_ended_callable);
	if (sprite_2d->is_connected(SNAME("animation_completed"), animation_completed_callable))
		sprite_2d->disconnect(SNAME("animation_completed"), animation_completed_callable);
	if (sprite_2d->is_connected(SNAME("animation_disposed"), animation_disposed_callable))
		sprite_2d->disconnect(SNAME("animation_disposed"), animation_disposed_callable);
	if (sprite_2d->is_connected(SNAME("animation_event"), animation_event_callable))
		sprite_2d->disconnect(SNAME("animation_event"), animation_event_callable);
	if (sprite_2d->is_connected(SNAME("before_animation_state_update"), before_animation_state_update_callable))
		sprite_2d->disconnect(SNAME("before_animation_state_update"), before_animation_state_update_callable);
	if (sprite_2d->is_connected(SNAME("before_animation_state_apply"), before_animation_state_apply_callable))
		sprite_2d->disconnect(SNAME("before_animation_state_apply"), before_animation_state_apply_callable);
	if (sprite_2d->is_connected(SNAME("before_world_transforms_change"), before_world_transforms_change_callable))
		sprite_2d->disconnect(SNAME("before_world_transforms_change"), before_world_transforms_change_callable);
	if (sprite_2d->is_connected(SNAME("world_transforms_changed"), world_transforms_changed_callable))
		sprite_2d->disconnect(SNAME("world_transforms_changed"), world_transforms_changed_callable);
#else
	if (sprite_2d->is_connected(SNAME("_internal_spine_objects_invalidated"), this, SNAME("on_internal_spine_objects_invalidated")))
		sprite_2d->disconnect(SNAME("_internal_spine_objects_invalidated"), this, SNAME("on_internal_spine_objects_invalidated"));
	if (sprite_2d->is_connected(SNAME("animation_started"), this, SNAME("on_sprite_animation_started")))
		sprite_2d->disconnect(SNAME("animation_started"), this, SNAME("on_sprite_animation_started"));
	if (sprite_2d->is_connected(SNAME("animation_interrupted"), this, SNAME("on_sprite_animation_interrupted")))
		sprite_2d->disconnect(SNAME("animation_interrupted"), this, SNAME("on_sprite_animation_interrupted"));
	if (sprite_2d->is_connected(SNAME("animation_ended"), this, SNAME("on_sprite_animation_ended")))
		sprite_2d->disconnect(SNAME("animation_ended"), this, SNAME("on_sprite_animation_ended"));
	if (sprite_2d->is_connected(SNAME("animation_completed"), this, SNAME("on_sprite_animation_completed")))
		sprite_2d->disconnect(SNAME("animation_completed"), this, SNAME("on_sprite_animation_completed"));
	if (sprite_2d->is_connected(SNAME("animation_disposed"), this, SNAME("on_sprite_animation_disposed")))
		sprite_2d->disconnect(SNAME("animation_disposed"), this, SNAME("on_sprite_animation_disposed"));
	if (sprite_2d->is_connected(SNAME("animation_event"), this, SNAME("on_sprite_animation_event")))
		sprite_2d->disconnect(SNAME("animation_event"), this, SNAME("on_sprite_animation_event"));
	if (sprite_2d->is_connected(SNAME("before_animation_state_update"), this, SNAME("on_sprite_before_animation_state_update")))
		sprite_2d->disconnect(SNAME("before_animation_state_update"), this, SNAME("on_sprite_before_animation_state_update"));
	if (sprite_2d->is_connected(SNAME("before_animation_state_apply"), this, SNAME("on_sprite_before_animation_state_apply")))
		sprite_2d->disconnect(SNAME("before_animation_state_apply"), this, SNAME("on_sprite_before_animation_state_apply"));
	if (sprite_2d->is_connected(SNAME("before_world_transforms_change"), this, SNAME("on_sprite_before_world_transforms_change")))
		sprite_2d->disconnect(SNAME("before_world_transforms_change"), this, SNAME("on_sprite_before_world_transforms_change"));
	if (sprite_2d->is_connected(SNAME("world_transforms_changed"), this, SNAME("on_sprite_world_transforms_changed")))
		sprite_2d->disconnect(SNAME("world_transforms_changed"), this, SNAME("on_sprite_world_transforms_changed"));
#endif
}

void SpineSprite3D::emit_animation_state_ready_deferred() {
	if (!is_inside_tree()) return;
	if (animation_state.is_valid()) {
		emit_signal(SNAME("animation_state_ready"), this, animation_state);
	}
}

Transform2D SpineSprite3D::get_raw_bone_global_transform(spine::Bone *bone) {
	if (!bone) return Transform2D();
	Transform2D transform;
#ifdef SPINE_RUNTIME_43
	transform.rotate(spine::MathUtil::Deg_Rad * bone->getAppliedPose().getWorldRotationX());
	transform.scale(Vector2(bone->getAppliedPose().getWorldScaleX(), bone->getAppliedPose().getWorldScaleY()));
	transform.set_origin(Vector2(bone->getAppliedPose().getWorldX(), bone->getAppliedPose().getWorldY()));
#else
	transform.rotate(spine::MathUtil::Deg_Rad * bone->getWorldRotationX());
	transform.scale(Vector2(bone->getWorldScaleX(), bone->getWorldScaleY()));
	transform.set_origin(Vector2(bone->getWorldX(), bone->getWorldY()));
#endif
	return transform;
}

void SpineSprite3D::set_raw_bone_global_transform(spine::Bone *bone, const Transform2D &transform) {
	if (!bone) return;

	Vector2 position = transform.get_origin();
	float rotation = spine::MathUtil::Rad_Deg * transform.get_rotation();
	Vector2 scale = transform.get_scale();
	Vector2 local_position = position;
#ifdef SPINE_RUNTIME_43
	float local_rotation = bone->getAppliedPose().worldToLocalRotation(rotation) - 180.0f;
	spine::Bone *parent = bone->getParent();
	if (parent) {
		parent->getAppliedPose().worldToLocal(local_position.x, local_position.y, local_position.x, local_position.y);
	}

	bone->getPose().setX(local_position.x);
	bone->getPose().setY(local_position.y);
	bone->getPose().setRotation(local_rotation);
	bone->getPose().setScaleX(scale.x);
	bone->getPose().setScaleY(scale.y);
#else
	float local_rotation = bone->worldToLocalRotation(rotation) - 180.0f;
	spine::Bone *parent = bone->getParent();
	if (parent) {
		parent->worldToLocal(local_position.x, local_position.y, local_position.x, local_position.y);
	}

	bone->setX(local_position.x);
	bone->setY(local_position.y);
	bone->setRotation(local_rotation);
	bone->setScaleX(scale.x);
	bone->setScaleY(scale.y);
#endif
}

void SpineSprite3D::set_bone_override(const String &bone_name, const Transform2D &transform) {
	if (EMPTY(bone_name)) return;
	for (int i = 0; i < bone_overrides.size(); ++i) {
		if (bone_overrides[i].bone_name == bone_name) {
			bone_overrides.write[i].transform = transform;
			return;
		}
	}
	BoneOverrideState state;
	state.bone_name = bone_name;
	state.transform = transform;
	bone_overrides.push_back(state);
}

void SpineSprite3D::apply_bone_overrides() {
	if (!skeleton.is_valid()) return;
	spine::Skeleton *skeleton_object = skeleton->get_spine_object();
	if (!skeleton_object) return;

	bool any_applied = false;
	for (int i = 0; i < bone_overrides.size(); ++i) {
		const BoneOverrideState &state = bone_overrides[i];
		spine::Bone *bone = skeleton_object->findBone(SPINE_STRING_TMP(state.bone_name));
		if (!bone) continue;
		set_raw_bone_global_transform(bone, state.transform);
		any_applied = true;
	}

	if (any_applied) {
		skeleton->update_world_transform(SpineConstant::Physics_Update);
	}
}

void SpineSprite3D::clear_track_state(int track_id) {
	for (int i = 0; i < restorable_track_states.size(); ++i) {
		if (restorable_track_states[i].track_id == track_id) {
			restorable_track_states.remove_at(i);
			return;
		}
	}
}

void SpineSprite3D::clear_all_track_states() {
	restorable_track_states.clear();
}

void SpineSprite3D::remember_track_state(Ref<SpineTrackEntry> track_entry) {
	if (!track_entry.is_valid()) return;
	auto animation = track_entry->get_animation();
	if (!animation.is_valid()) return;
	RestorableTrackState state;
	state.valid = true;
	state.animation_name = animation->get_name();
	state.loop = track_entry->get_loop();
	state.track_id = track_entry->get_track_index();
	state.track_time = track_entry->get_track_time();
	clear_track_state(state.track_id);
	restorable_track_states.push_back(state);
}

void SpineSprite3D::restore_track_states_if_needed() {
	if (!animation_state.is_valid()) return;
	auto current_state_object = animation_state->get_spine_object();
	if (!current_state_object || current_state_object == last_animation_state_object) return;
	last_animation_state_object = current_state_object;
	for (int i = 0; i < restorable_track_states.size(); ++i) {
		const RestorableTrackState &state = restorable_track_states[i];
		if (!state.valid || EMPTY(state.animation_name)) continue;
		Ref<SpineTrackEntry> restored_entry = animation_state->set_animation(state.animation_name, state.loop, state.track_id);
		if (restored_entry.is_valid()) {
			restored_entry->set_track_time(state.track_time);
		}
	}
}

void SpineSprite3D::on_internal_spine_objects_invalidated() {
	if (animation_state.is_valid()) {
		for (int i = 0; i < restorable_track_states.size(); ++i) {
			RestorableTrackState &state = restorable_track_states.write[i];
			if (!state.valid) continue;
			Ref<SpineTrackEntry> current_entry = animation_state->get_current(state.track_id);
			if (current_entry.is_valid()) {
				state.track_time = current_entry->get_track_time();
			}
		}
	}
	skeleton.unref();
	animation_state.unref();
	animation_state_ready_emitted = false;
	last_animation_state_object = nullptr;
	log_checkpoint("internal spine objects invalidated");
}

void SpineSprite3D::sanitize_legacy_2d_transform_if_needed() {
	Vector3 scale = get_scale();
	if (!Math::is_zero_approx(scale.z)) return;

	Basis basis = get_basis();
	basis.set_column(2, Vector3(0, 0, 1));
	if (Math::is_zero_approx(basis.determinant())) {
		basis = Basis();
	}
	set_basis(basis);

	Vector3 position = get_position();
	if (legacy_position_conversion_enabled && (Math::abs(position.x) > 32.0f || Math::abs(position.y) > 32.0f || Math::abs(position.z) > 32.0f)) {
		float ppu = MAX(0.0001f, pixels_per_unit);
		position.x /= ppu;
		position.y = -position.y / ppu;
		position.z /= ppu;
		set_position(position);
		log_checkpoint(String("legacy 2D transform converted to 3D units, new position=") + String::num(position.x) + "," + String::num(position.y) + "," + String::num(position.z));
	} else {
		log_checkpoint("legacy 2D transform detected (zero z-scale) and basis repaired without auto position conversion");
	}
}

bool SpineSprite3D::sanitize_local_scale_if_needed() {
	Vector3 scale = get_scale();
	bool changed = false;
	if (Math::is_zero_approx(scale.x)) {
		scale.x = scale.x < 0.0f ? -1.0f : 1.0f;
		changed = true;
	}
	if (Math::is_zero_approx(scale.y)) {
		scale.y = scale.y < 0.0f ? -1.0f : 1.0f;
		changed = true;
	}
	if (Math::is_zero_approx(scale.z)) {
		scale.z = scale.z < 0.0f ? -1.0f : 1.0f;
		changed = true;
	}
	if (changed) {
		set_scale(scale);
		log_checkpoint(String("local scale sanitized to avoid singular Basis: ") + String::num(scale.x) + "," + String::num(scale.y) + "," + String::num(scale.z));
	}
	return changed;
}

void SpineSprite3D::_notification(int what) {
	switch (what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_PARENTED:
		case NOTIFICATION_UNPARENTED:
			if (render_backend == RenderBackend_AutoStreamWorld) {
				clear_runtime_mesh_surfaces();
				rebuild_runtime_mesh();
			}
			break;
		case NOTIFICATION_READY:
			log_checkpoint("NOTIFICATION_READY");
			if (get_parent() && !Object::cast_to<Node3D>(get_parent())) {
				log_checkpoint("parent is not Node3D -> forcing top_level + unit scale");
				set_as_top_level(true);
				set_scale(Vector3(1, 1, 1));
			}
			sanitize_legacy_2d_transform_if_needed();
			sanitize_local_scale_if_needed();
			ensure_runtime_nodes();
			on_skeleton_data_changed();
			set_process(update_mode == SpineConstant::UpdateMode_Process);
			set_process_internal(true);
			set_physics_process(update_mode == SpineConstant::UpdateMode_Physics);
			set_physics_process_internal(false);
			break;
		case NOTIFICATION_EXIT_TREE:
			disconnect_sprite_signals();
			if (render_backend == RenderBackend_AutoStreamWorld) {
				clear_runtime_mesh_surfaces();
			}
			break;
		case NOTIFICATION_PROCESS:
			sanitize_local_scale_if_needed();
			if (update_mode == SpineConstant::UpdateMode_Process) update_skeleton(get_process_delta_time());
			break;
		case NOTIFICATION_PHYSICS_PROCESS:
			sanitize_local_scale_if_needed();
			if (update_mode == SpineConstant::UpdateMode_Physics) update_skeleton(get_physics_process_delta_time());
			break;
	}
}

void SpineSprite3D::set_skeleton_data_res(const Ref<SpineSkeletonDataResource> &resource) {
#if VERSION_MAJOR > 3
	Callable skeleton_data_changed_callable(this, SNAME("on_skeleton_data_changed"));
	if (skeleton_data_res.is_valid() && skeleton_data_res->is_connected(SNAME("skeleton_data_changed"), skeleton_data_changed_callable)) {
		skeleton_data_res->disconnect(SNAME("skeleton_data_changed"), skeleton_data_changed_callable);
	}
#else
	if (skeleton_data_res.is_valid() && skeleton_data_res->is_connected(SNAME("skeleton_data_changed"), this, SNAME("on_skeleton_data_changed"))) {
		skeleton_data_res->disconnect(SNAME("skeleton_data_changed"), this, SNAME("on_skeleton_data_changed"));
	}
#endif
	skeleton_data_res = resource;
#if VERSION_MAJOR > 3
	if (skeleton_data_res.is_valid() && !skeleton_data_res->is_connected(SNAME("skeleton_data_changed"), skeleton_data_changed_callable)) {
		skeleton_data_res->connect(SNAME("skeleton_data_changed"), skeleton_data_changed_callable);
	}
#else
	if (skeleton_data_res.is_valid() && !skeleton_data_res->is_connected(SNAME("skeleton_data_changed"), this, SNAME("on_skeleton_data_changed"))) {
		skeleton_data_res->connect(SNAME("skeleton_data_changed"), this, SNAME("on_skeleton_data_changed"));
	}
#endif
	on_skeleton_data_changed();
}

Ref<SpineSkeletonDataResource> SpineSprite3D::get_skeleton_data_res() {
	return skeleton_data_res;
}

void SpineSprite3D::sync_from_sprite() {
	if (!sprite_2d) return;
	skeleton = sprite_2d->get_skeleton();
	animation_state = sprite_2d->get_animation_state();
	if (animation_state.is_valid() && !animation_state_ready_emitted) {
		animation_state_ready_emitted = true;
		call_deferred(SNAME("emit_animation_state_ready_deferred"));
	}

	if (!animation_state.is_valid()) return;
	restore_track_states_if_needed();
	for (int i = 0; i < pending_animation_commands.size(); ++i) {
		const PendingAnimationCommand &command = pending_animation_commands[i];
		switch (command.type) {
			case PendingAnimationCommand::Type_SetAnimation:
				animation_state->set_animation(command.animation_name, command.loop, command.track_id);
				break;
			case PendingAnimationCommand::Type_AddAnimation:
				animation_state->add_animation(command.animation_name, command.delay, command.loop, command.track_id);
				break;
			case PendingAnimationCommand::Type_SetEmptyAnimation:
				animation_state->set_empty_animation(command.track_id, command.mix_duration);
				break;
			case PendingAnimationCommand::Type_AddEmptyAnimation:
				animation_state->add_empty_animation(command.track_id, command.mix_duration, command.delay);
				break;
			case PendingAnimationCommand::Type_SetEmptyAnimations:
				animation_state->set_empty_animations(command.mix_duration);
				break;
			case PendingAnimationCommand::Type_ClearTracks:
				animation_state->clear_tracks();
				break;
			case PendingAnimationCommand::Type_ClearTrack:
				animation_state->clear_track(command.track_id);
				break;
		}
	}
	pending_animation_commands.clear();
}

void SpineSprite3D::sync_debug_settings_to_sprite() {
	if (!sprite_2d) return;
	sprite_2d->set_debug_root(debug_root);
	sprite_2d->set_debug_root_color(debug_root_color);
	sprite_2d->set_debug_bones(debug_bones);
	sprite_2d->set_debug_bones_color(debug_bones_color);
	sprite_2d->set_debug_bones_thickness(debug_bones_thickness);
	sprite_2d->set_debug_regions(debug_regions);
	sprite_2d->set_debug_regions_color(debug_regions_color);
	sprite_2d->set_debug_meshes(debug_meshes);
	sprite_2d->set_debug_meshes_color(debug_meshes_color);
	sprite_2d->set_debug_bounding_boxes(debug_bounding_boxes);
	sprite_2d->set_debug_bounding_boxes_color(debug_bounding_boxes_color);
	sprite_2d->set_debug_paths(debug_paths);
	sprite_2d->set_debug_paths_color(debug_paths_color);
	sprite_2d->set_debug_clipping(debug_clipping);
	sprite_2d->set_debug_clipping_color(debug_clipping_color);
}

void SpineSprite3D::on_skeleton_data_changed() {
	log_checkpoint("on_skeleton_data_changed: begin");
	ensure_runtime_nodes();
	if (!sprite_2d) return;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();

	sprite_2d->set_skeleton_data_res(skeleton_data_res);
	sprite_2d->set_time_scale(time_scale);
	sprite_2d->set_normal_material(normal_material);
	sprite_2d->set_additive_material(additive_material);
	sprite_2d->set_multiply_material(multiply_material);
	sprite_2d->set_screen_material(screen_material);
	sprite_2d->set_normal_map_enabled(generated_normal_map_enabled);
	sprite_2d->set_normal_map_override(generated_normal_map_override);
	sync_debug_settings_to_sprite();
	log_checkpoint("on_skeleton_data_changed: sprite configured");

	sync_from_sprite();
	log_checkpoint(String("resource_valid=") + String(skeleton_data_res.is_valid() ? "true" : "false") + String(", loaded=") + String((skeleton_data_res.is_valid() && skeleton_data_res->is_skeleton_data_loaded()) ? "true" : "false") + String(", skeleton=") + String(skeleton.is_valid() ? "ok" : "null") + String(", anim=") + String(animation_state.is_valid() ? "ok" : "null"));
	log_checkpoint("on_skeleton_data_changed: update preview");
	update_preview_animation();
	log_checkpoint("on_skeleton_data_changed: rebuild mesh");
	rebuild_runtime_mesh();
	log_checkpoint("on_skeleton_data_changed: done");
	NOTIFY_PROPERTY_LIST_CHANGED();
}

Ref<Material> SpineSprite3D::resolve_visible_slot_material(spine::BlendMode blend_mode, const Ref<Texture> &texture, const Ref<Texture> &normal_map, int render_priority, bool two_color_tint, const Ref<Material> &slot_override) const {
	const int clamped_priority = CLAMP(render_priority, -128, 127);
	const Ref<Texture> material_normal_map = normal_map;
	Ref<Texture2D> material_texture_2d = texture;
	Ref<Texture2D> material_normal_texture_2d = material_normal_map;

	const uint64_t generated_cache_key = make_spine_sprite_3d_material_cache_key(0x535052334447454eULL, blend_mode, texture, material_normal_map, Ref<Material>(), clamped_priority, lighting_enabled, double_sided, generated_normal_map_preview, two_color_tint);

	if (generated_normal_map_preview) {
		auto preview_it = generated_material_cache.find(generated_cache_key);
		if (preview_it != generated_material_cache.end()) {
			return preview_it->second;
		}
		Ref<Material> preview_material = make_normal_map_preview_material(material_texture_2d, material_normal_texture_2d, clamped_priority, visible_alpha_cutoff, double_sided);
		generated_material_cache.emplace(generated_cache_key, preview_material);
		return preview_material;
	}

	Ref<Material> custom_material = slot_override;
	if (!custom_material.is_valid()) {
		switch (blend_mode) {
			case spine::BlendMode_Normal: custom_material = normal_material; break;
			case spine::BlendMode_Additive: custom_material = additive_material; break;
			case spine::BlendMode_Multiply: custom_material = multiply_material; break;
			case spine::BlendMode_Screen: custom_material = screen_material; break;
		}
	}

	if (custom_material.is_valid()) {
		const uint64_t cache_key = make_spine_sprite_3d_material_cache_key(0x5350523344435553ULL, blend_mode, texture, material_normal_map, custom_material, clamped_priority, lighting_enabled, double_sided, generated_normal_map_preview, two_color_tint);
		auto custom_it = custom_material_priority_cache.find(cache_key);
		if (custom_it != custom_material_priority_cache.end()) {
			return custom_it->second;
		}

		Ref<ShaderMaterial> source_shader_material = custom_material;
		if (source_shader_material.is_valid()) {
			Ref<ShaderMaterial> runtime_shader_material = source_shader_material->duplicate(true);
			if (!runtime_shader_material.is_valid()) {
				runtime_shader_material.instantiate();
				runtime_shader_material->set_shader(source_shader_material->get_shader());
			}
			Ref<Shader> source_shader = source_shader_material->get_shader();
			if (source_shader.is_valid()) {
				const String source_code = source_shader->get_code();
				const String runtime_code = make_spine_sprite_3d_custom_shader_code(source_code, blend_mode, lighting_enabled, double_sided);
				if (runtime_code != source_code) {
					Ref<Shader> runtime_shader = memnew(Shader);
					runtime_shader->set_code(runtime_code);
					runtime_shader_material->set_shader(runtime_shader);
				}
			}
			runtime_shader_material->set_render_priority(clamped_priority);
			set_spine_sprite_3d_shader_material_parameters(runtime_shader_material, material_texture_2d, material_normal_texture_2d, generated_normal_map_enabled && material_normal_texture_2d.is_valid(), generated_normal_scale, generated_normal_max_slope, generated_normal_dark_suppression, generated_shader_light_scale, generated_shader_ambient, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, visible_alpha_cutoff);
			custom_material_priority_cache.emplace(cache_key, runtime_shader_material);
			return runtime_shader_material;
		}

		Ref<Material> duplicated = custom_material->duplicate();
		if (!duplicated.is_valid()) {
			return custom_material;
		}

		duplicated->set("render_priority", clamped_priority);

		Ref<BaseMaterial3D> base_material = duplicated;
		apply_spine_sprite_3d_base_material_contract(base_material, blend_mode, texture, material_normal_map, lighting_enabled, generated_normal_map_enabled, generated_normal_scale, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient);

		custom_material_priority_cache.emplace(cache_key, duplicated);
		return duplicated;
	}

	const uint64_t cache_key = generated_cache_key;
	auto it = generated_material_cache.find(cache_key);
	if (it != generated_material_cache.end()) {
		return it->second;
	}

	if (two_color_tint) {
		Ref<Material> material = make_two_color_material(blend_mode, material_texture_2d, material_normal_texture_2d, clamped_priority, lighting_enabled, double_sided, generated_normal_map_enabled, generated_normal_scale, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient, visible_alpha_cutoff);
		generated_material_cache.emplace(cache_key, material);
		return material;
	}

	if (double_sided) {
		Ref<Material> material = make_single_color_material(blend_mode, material_texture_2d, material_normal_texture_2d, clamped_priority, lighting_enabled, true, generated_normal_map_enabled, generated_normal_scale, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient, visible_alpha_cutoff);
		generated_material_cache.emplace(cache_key, material);
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
	switch (blend_mode) {
		case spine::BlendMode_Additive: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD); break;
		case spine::BlendMode_Multiply: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MUL); break;
		case spine::BlendMode_Screen: material->set_blend_mode(BaseMaterial3D::BLEND_MODE_ADD); break;
		case spine::BlendMode_Normal:
		default:
			material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);
			break;
	}
	generated_material_cache.emplace(cache_key, material);
	return material;
}

SpineSlotNode3D *SpineSprite3D::find_slot_node_for_index(int slot_index) const {
	for (int i = 0; i < get_child_count(); ++i) {
		SpineSlotNode3D *slot_node = cast_to<SpineSlotNode3D>(get_child(i));
		if (slot_node && slot_node->get_slot_index() == slot_index) {
			return slot_node;
		}
	}
	return nullptr;
}

Ref<Material> SpineSprite3D::resolve_shadow_slot_material(const Ref<Texture> &texture) const {
	const uint64_t texture_id = texture.is_valid() ? texture->get_instance_id() : 0;
	const uint64_t cutoff_key = (uint64_t)CLAMP((int)Math::round(shadow_alpha_cutoff * 1000.0f), 0, 1000);
	const uint64_t mode_key = (uint64_t)shadow_casting_mode;
	const uint64_t cache_key = (texture_id << 20) ^ (cutoff_key << 2) ^ mode_key;
	auto it = generated_shadow_material_cache.find(cache_key);
	if (it != generated_shadow_material_cache.end()) {
		return it->second;
	}

	Ref<StandardMaterial3D> material = memnew(StandardMaterial3D);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	const bool double_sided_shadows = shadow_casting_mode == GeometryInstance3D::SHADOW_CASTING_SETTING_DOUBLE_SIDED;
	material->set_cull_mode((double_sided || double_sided_shadows) ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
	material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_OPAQUE_ONLY);
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, false);
	material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
	material->set("alpha_scissor_threshold", shadow_alpha_cutoff);
	material->set_blend_mode(BaseMaterial3D::BLEND_MODE_MIX);

	generated_shadow_material_cache.emplace(cache_key, material);
	return material;
}

void SpineSprite3D::clear_runtime_mesh_surfaces() {
	if (!runtime_mesh.is_valid()) return;
	runtime_mesh->clear_surfaces();
	if (shadow_runtime_mesh.is_valid()) shadow_runtime_mesh->clear_surfaces();
	if (debug_runtime_mesh.is_valid()) debug_runtime_mesh->clear_surfaces();
	if (mesh_instance_3d) mesh_instance_3d->set_visible(!should_stream_to_world());
	if (shadow_mesh_instance_3d) shadow_mesh_instance_3d->set_visible(false);
	if (debug_mesh_instance_3d) debug_mesh_instance_3d->set_visible(false);
}

bool SpineSprite3D::build_render_runs(std::vector<RenderRun> &runs, bool stream_space, SpineRenderWorld3D *render_world, spine::Skeleton *&skeleton_object, Vector3 &slot_stack_axis_local, float &effective_depth_scale, int &slot_count) {
	runs.clear();
	std::vector<RenderRun> back_runs;
	const bool build_static_double_sided_stack = double_sided;
	const bool build_back_geometry = false;
	last_generated_slot_count = 0;
	last_generated_run_count = 0;
	skeleton_object = nullptr;
	slot_stack_axis_local = Vector3(0.0f, 0.0f, 1.0f);
	effective_depth_scale = 1.0f;
	slot_count = 0;

	if (stream_space && !render_world) return false;
	if (!skeleton.is_valid() || !skeleton_clipper) return false;
	skeleton_object = skeleton->get_spine_object();
	if (!skeleton_object) return false;

	const float ppu = MAX(0.0001f, pixels_per_unit);
	slot_stack_axis_local = build_static_double_sided_stack ? Vector3(0.0f, 0.0f, 1.0f) : get_slot_stack_axis_local();
	const Vector3 slot_surface_normal = Vector3(0.0f, 0.0f, 1.0f);
	slot_count = (int)skeleton_object->getSlots().size();
	effective_depth_scale = get_effective_depth_scale_for_axis(slot_stack_axis_local);
	float local_depth_step = depth_offset / MAX(effective_depth_scale, 0.0001f);
	bool embedded_in_slot = false;
	bool embedded_priority_band = false;
	int embedded_host_draw_order_index = -1;
	int embedded_host_slot_count = 0;
	bool isolated_in_parent_sprite = false;
	const float embedded_band_fraction = 0.9f;
	SpineSlotNode3D *parent_slot_node = find_ancestor_slot_node();
	SpineSprite3D *parent_sprite_host = nullptr;
	int isolated_priority_bias = render_priority_bias;
	int isolated_priority_range = render_priority_range;
	auto compute_priority_from_band = [](int slot_order, int slot_count, int bias, int range) {
		int min_p = CLAMP(bias - range, -128, 127);
		int max_p = CLAMP(bias + range, -128, 127);
		if (min_p > max_p) {
			SWAP(min_p, max_p);
		}
		if (slot_count <= 1) {
			return CLAMP(bias, -128, 127);
		}
		const float t = (float)slot_order / (float)(slot_count - 1);
		return min_p + (int)Math::round(t * (float)(max_p - min_p));
	};

	if (!stream_space && render_embedded_in_parent_slot && parent_slot_node && slot_count > 0) {
		SpineSprite3D *host_sprite = nullptr;
		Node *host_ancestor = parent_slot_node->get_parent();
		while (host_ancestor) {
			host_sprite = Object::cast_to<SpineSprite3D>(host_ancestor);
			if (host_sprite) break;
			host_ancestor = host_ancestor->get_parent();
		}
		if (host_sprite) {
			const int host_slot_index = parent_slot_node->get_slot_index();
			const int host_draw_order_index = host_sprite->find_draw_order_index_for_slot_index(host_slot_index);
			Ref<SpineSkeleton> host_skeleton_ref = host_sprite->get_skeleton();
			spine::Skeleton *host_skeleton_object = host_skeleton_ref.is_valid() ? host_skeleton_ref->get_spine_object() : nullptr;
			int host_slot_count = 0;
			if (host_skeleton_object) {
#ifdef SPINE_RUNTIME_43
				host_slot_count = (int)host_skeleton_object->getDrawOrder().getAppliedPose().size();
#else
				host_slot_count = (int)host_skeleton_object->getDrawOrder().size();
#endif
			}
			if (host_draw_order_index >= 0 && host_slot_count > 0) {
				embedded_in_slot = true;
				embedded_priority_band = true;
				embedded_host_draw_order_index = host_draw_order_index;
				embedded_host_slot_count = host_slot_count;
				const float host_step = MAX(0.0001f, host_sprite->get_depth_offset());
				const float slice_span = host_step * embedded_band_fraction;
				local_depth_step = (slot_count > 1) ? (slice_span / MAX(effective_depth_scale, 0.0001f) / (float)(slot_count - 1)) : 0.0f;
			}
		}
	}

	if (!stream_space) {
		Node *ancestor = get_parent();
		while (ancestor) {
			parent_sprite_host = Object::cast_to<SpineSprite3D>(ancestor);
			if (parent_sprite_host) break;
			ancestor = ancestor->get_parent();
		}

		if (!embedded_priority_band && parent_sprite_host && is_inside_tree() && parent_sprite_host->is_inside_tree()) {
			int sibling_nested_sprite_index = 0;
			Vector<Node *> stack;
			for (int i = parent_sprite_host->get_child_count() - 1; i >= 0; --i) {
				stack.push_back(parent_sprite_host->get_child(i));
			}

			while (!stack.is_empty()) {
				Node *node = stack[stack.size() - 1];
				stack.resize(stack.size() - 1);

				SpineSprite3D *nested_sprite = Object::cast_to<SpineSprite3D>(node);
				if (nested_sprite) {
					if (nested_sprite == this) break;
					sibling_nested_sprite_index++;
					continue;
				}

				for (int i = node->get_child_count() - 1; i >= 0; --i) {
					stack.push_back(node->get_child(i));
				}
			}

			const Transform3D host_global_transform = parent_sprite_host->get_global_transform();
			Vector3 host_plane_normal =
#if VERSION_MAJOR > 3
					host_global_transform.basis.get_column(2);
#else
					host_global_transform.basis.get_axis(2);
#endif
			if (host_plane_normal.length_squared() < CMP_EPSILON2) {
				host_plane_normal = Vector3(0.0f, 0.0f, 1.0f);
			} else {
				host_plane_normal.normalize();
			}

			const float signed_host_distance = host_plane_normal.dot(get_global_transform().origin - host_global_transform.origin);
			const int direction = signed_host_distance < -0.0001f ? -1 : 1;
			const int nested_range = MAX(render_priority_range, embedded_in_slot ? 20 : 8);
			const int nested_stride = MAX(6, nested_range * 2 + 4);
			const int host_margin = MAX(parent_sprite_host->render_priority_range, 8) + nested_range + 4;
			const int nested_offset = host_margin + sibling_nested_sprite_index * nested_stride;
			isolated_priority_range = nested_range;
			isolated_priority_bias = CLAMP(parent_sprite_host->render_priority_bias + direction * nested_offset, -128, 127);
			isolated_in_parent_sprite = true;
		}

	}

	Transform3D to_stream_local;
	Vector3 run_normal = slot_surface_normal;
	if (stream_space) {
		to_stream_local = render_world->get_global_transform().affine_inverse() * get_global_transform();
		run_normal = to_stream_local.basis.inverse().transposed().xform(slot_surface_normal);
		if (run_normal.length_squared() < CMP_EPSILON2) {
			run_normal = slot_surface_normal;
		} else {
			run_normal.normalize();
		}
	}

	static spine::Vector<unsigned short> quad_indices = []() {
		spine::Vector<unsigned short> arr;
		arr.setSize(6, 0);
		arr[0] = 0; arr[1] = 1; arr[2] = 2; arr[3] = 2; arr[4] = 3; arr[5] = 0;
		return arr;
	}();

	auto same_ref = [](const Ref<Material> &a, const Ref<Material> &b) {
		return a.ptr() == b.ptr();
	};
	auto append_slot_to_run = [&](RenderRun &run, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedFloat32Array &stack_depths, const PackedInt32Array &indices) {
		const int vertex_offset = run.vertices.size();
		for (int i = 0; i < vertices.size(); ++i) run.vertices.push_back(vertices[i]);
		for (int i = 0; i < uvs.size(); ++i) run.uvs.push_back(uvs[i]);
		for (int i = 0; i < normals.size(); ++i) run.normals.push_back(normals[i]);
		for (int i = 0; i < tangents.size(); ++i) run.tangents.push_back(tangents[i]);
		for (int i = 0; i < colors.size(); ++i) run.colors.push_back(colors[i]);
		for (int i = 0; i < dark_colors.size(); ++i) run.dark_colors.push_back(dark_colors[i]);
		for (int i = 0; i < stack_depths.size(); ++i) run.stack_depths.push_back(stack_depths[i]);
		for (int i = 0; i < indices.size(); ++i) run.indices.push_back(indices[i] + vertex_offset);
	};
	auto append_geometry_to_runs = [&](std::vector<RenderRun> &target_runs, int run_slot_order, int run_render_priority, const Ref<Material> &material, bool casts_shadow, const Ref<Material> &shadow_material, const PackedVector3Array &vertices, const PackedVector2Array &uvs, const PackedVector3Array &normals, const PackedFloat32Array &tangents, const PackedColorArray &colors, const PackedFloat32Array &dark_colors, const PackedFloat32Array &stack_depths, const PackedInt32Array &indices) {
		const bool visible = material.is_valid();
		const bool run_casts_shadow = casts_shadow && shadow_material.is_valid();
		bool can_append = false;
		if (!target_runs.empty()) {
			RenderRun &last_run = target_runs[target_runs.size() - 1];
			can_append = last_run.last_slot_order + 1 == run_slot_order &&
					last_run.render_priority == run_render_priority &&
					last_run.visible == visible &&
					last_run.casts_shadow == run_casts_shadow &&
					same_ref(last_run.material, material) &&
					same_ref(last_run.shadow_material, shadow_material);
		}

		if (!can_append) {
			RenderRun run;
			run.first_slot_order = run_slot_order;
			run.last_slot_order = run_slot_order;
			run.render_priority = run_render_priority;
			run.visible = visible;
			run.casts_shadow = run_casts_shadow;
			run.material = material;
			run.shadow_material = shadow_material;
			append_slot_to_run(run, vertices, uvs, normals, tangents, colors, dark_colors, stack_depths, indices);
			target_runs.push_back(run);
		} else {
			RenderRun &run = target_runs[target_runs.size() - 1];
			run.last_slot_order = run_slot_order;
			append_slot_to_run(run, vertices, uvs, normals, tangents, colors, dark_colors, stack_depths, indices);
		}
	};

	spine::Vector<float> world_vertices;
#ifdef SPINE_RUNTIME_43
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder().getAppliedPose();
#else
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder();
#endif
	for (int slot_order = 0; slot_order < slot_count; ++slot_order) {
		spine::Slot *slot = draw_order[slot_order];
#ifdef SPINE_RUNTIME_43
		spine::Attachment *attachment = slot->getAppliedPose().getAttachment();
#else
		spine::Attachment *attachment = slot->getAttachment();
#endif
		if (!attachment || !slot->getBone().isActive()) {
			skeleton_clipper->clipEnd(*slot);
			continue;
		}

		SpineRendererObject *renderer_object = nullptr;
		spine::Vector<float> *vertices = &world_vertices;
		spine::Vector<float> *uvs = nullptr;
		spine::Vector<unsigned short> *indices = nullptr;
		spine::Color skeleton_color = skeleton_object->getColor();
#ifdef SPINE_RUNTIME_43
		spine::Color slot_color = slot->getAppliedPose().getColor();
		const bool slot_has_dark_color = slot->getAppliedPose().hasDarkColor();
		spine::Color slot_dark_color = slot->getAppliedPose().getDarkColor();
#else
		spine::Color slot_color = slot->getColor();
		const bool slot_has_dark_color = slot->hasDarkColor();
		spine::Color slot_dark_color = slot->getDarkColor();
#endif
		spine::Color tint(skeleton_color.r * slot_color.r, skeleton_color.g * slot_color.g, skeleton_color.b * slot_color.b, skeleton_color.a * slot_color.a);
		const bool two_color_tint = slot_has_dark_color;

		if (attachment->getRTTI().isExactly(spine::RegionAttachment::rtti)) {
			auto *region = (spine::RegionAttachment *)attachment;
			vertices->setSize(8, 0);
#ifdef SPINE_RUNTIME_43
			auto &sequence = region->getSequence();
			int sequence_index = sequence.resolveIndex(slot->getAppliedPose());
			region->computeWorldVertices(*slot, sequence.getOffsets(sequence_index).buffer(), vertices->buffer(), 0);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)sequence.getRegion(sequence_index))->getPage()->texture;
			uvs = &sequence.getUVs(sequence_index);
#elif defined(SPINE_RUNTIME_41_PLUS)
			region->computeWorldVertices(*slot, *vertices, 0);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)region->getRegion())->page->texture;
			uvs = &region->getUVs();
#else
			region->computeWorldVertices(slot->getBone(), *vertices, 0);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)region->getRendererObject())->page->getRendererObject();
			uvs = &region->getUVs();
#endif
			indices = &quad_indices;
			auto attachment_color = region->getColor();
			tint.r *= attachment_color.r; tint.g *= attachment_color.g; tint.b *= attachment_color.b; tint.a *= attachment_color.a;
		} else if (attachment->getRTTI().isExactly(spine::MeshAttachment::rtti)) {
			auto *mesh = (spine::MeshAttachment *)attachment;
			vertices->setSize(mesh->getWorldVerticesLength(), 0);
#ifdef SPINE_RUNTIME_43
			auto &sequence = mesh->getSequence();
			int sequence_index = sequence.resolveIndex(slot->getAppliedPose());
			mesh->computeWorldVertices(*skeleton_object, *slot, 0, mesh->getWorldVerticesLength(), vertices->buffer(), 0, 2);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)sequence.getRegion(sequence_index))->getPage()->texture;
			uvs = &sequence.getUVs(sequence_index);
#elif defined(SPINE_RUNTIME_41_PLUS)
			mesh->computeWorldVertices(*slot, *vertices);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)mesh->getRegion())->page->texture;
			uvs = &mesh->getUVs();
#else
			mesh->computeWorldVertices(*slot, *vertices);
			renderer_object = (SpineRendererObject *)((spine::AtlasRegion *)mesh->getRendererObject())->page->getRendererObject();
			uvs = &mesh->getUVs();
#endif
			indices = &mesh->getTriangles();
			auto attachment_color = mesh->getColor();
			tint.r *= attachment_color.r; tint.g *= attachment_color.g; tint.b *= attachment_color.b; tint.a *= attachment_color.a;
		} else if (attachment->getRTTI().isExactly(spine::ClippingAttachment::rtti)) {
#ifdef SPINE_RUNTIME_43
			skeleton_clipper->clipStart(*skeleton_object, *slot, (spine::ClippingAttachment *)attachment);
#else
			skeleton_clipper->clipStart(*slot, (spine::ClippingAttachment *)attachment);
#endif
			continue;
		} else {
			skeleton_clipper->clipEnd(*slot);
			continue;
		}

		if (skeleton_clipper->isClipping()) {
			skeleton_clipper->clipTriangles(*vertices, *indices, *uvs, 2);
			if (skeleton_clipper->getClippedTriangles().size() == 0) {
				skeleton_clipper->clipEnd(*slot);
				continue;
			}
			vertices = &skeleton_clipper->getClippedVertices();
			uvs = &skeleton_clipper->getClippedUVs();
			indices = &skeleton_clipper->getClippedTriangles();
		}
		if (!renderer_object || indices->size() == 0) {
			skeleton_clipper->clipEnd(*slot);
			continue;
		}

		PackedVector3Array godot_vertices;
		PackedVector3Array godot_back_vertices;
		PackedVector2Array godot_uvs;
		PackedVector3Array godot_normals;
		PackedVector3Array godot_back_normals;
		PackedFloat32Array godot_tangents;
		PackedFloat32Array godot_back_tangents;
		PackedColorArray godot_colors;
		PackedFloat32Array godot_dark_colors;
		PackedFloat32Array godot_back_dark_colors;
		PackedFloat32Array godot_stack_depths;
		PackedFloat32Array godot_back_stack_depths;
		PackedInt32Array godot_indices;
		PackedInt32Array godot_back_indices;
		const int vertex_count = (int)(vertices->size() / 2);
		godot_vertices.resize(vertex_count);
		if (build_back_geometry) godot_back_vertices.resize(vertex_count);
		godot_uvs.resize(vertex_count);
		godot_normals.resize(vertex_count);
		if (build_back_geometry) godot_back_normals.resize(vertex_count);
		godot_colors.resize(vertex_count);
		if (build_static_double_sided_stack) godot_stack_depths.resize(vertex_count * 4);
		if (two_color_tint) {
			godot_dark_colors.resize(vertex_count * 4);
			if (build_back_geometry) godot_back_dark_colors.resize(vertex_count * 4);
		}

		float stack_depth = 0.0f;
		if (!stream_space && embedded_in_slot) {
			if (slot_count > 1) {
				const float total_span = local_depth_step * (float)(slot_count - 1);
				stack_depth = (float)slot_order * local_depth_step - total_span * 0.5f;
			}
		} else {
			stack_depth = (float)slot_order * depth_offset / MAX(effective_depth_scale, 0.0001f);
		}
		const Vector3 slot_depth_offset = build_static_double_sided_stack ? Vector3() : slot_stack_axis_local * stack_depth;
		const Vector3 back_slot_depth_offset = -slot_stack_axis_local * stack_depth;
		const Vector3 back_run_normal = -run_normal;

		int render_priority = 0;
		int back_slot_order = slot_count - 1 - slot_order;
		int back_render_priority = 0;
		if (!stream_space) {
			if (embedded_priority_band && parent_sprite_host) {
				const float t = slot_count <= 1 ? 0.5f : (float)slot_order / (float)(slot_count - 1);
				render_priority = parent_sprite_host->compute_slot_insert_band_render_priority(embedded_host_draw_order_index, embedded_host_slot_count, t);
				const float back_t = slot_count <= 1 ? 0.5f : (float)back_slot_order / (float)(slot_count - 1);
				back_render_priority = parent_sprite_host->compute_slot_insert_band_render_priority(embedded_host_draw_order_index, embedded_host_slot_count, back_t);
			} else {
				render_priority = isolated_in_parent_sprite ? compute_priority_from_band(slot_order, slot_count, isolated_priority_bias, isolated_priority_range) : compute_slot_render_priority(slot_order, slot_count);
				back_render_priority = isolated_in_parent_sprite ? compute_priority_from_band(back_slot_order, slot_count, isolated_priority_bias, isolated_priority_range) : compute_slot_render_priority(back_slot_order, slot_count);
			}
		}

		const spine::BlendMode slot_blend_mode = slot->getData().getBlendMode();
		Engine *engine = Engine::get_singleton();
		const bool editor_opaque_preview_alpha = engine && engine->is_editor_hint() && (slot_blend_mode == spine::BlendMode_Normal || slot_blend_mode == spine::BlendMode_Multiply);
		for (int i = 0; i < vertex_count; ++i) {
			const float x = vertices->buffer()[i * 2 + 0] / ppu;
			const float y = -vertices->buffer()[i * 2 + 1] / ppu;
			const Vector3 local_vertex = Vector3(x, y, 0.0f) + slot_depth_offset;
			godot_vertices.set(i, stream_space ? to_stream_local.xform(local_vertex) : local_vertex);
			const float u = uvs->buffer()[i * 2 + 0];
			const float v = uvs->buffer()[i * 2 + 1];
			godot_uvs.set(i, Vector2(u, v));
			godot_normals.set(i, run_normal);
			godot_colors.set(i, Color(tint.r, tint.g, tint.b, editor_opaque_preview_alpha ? 1.0f : tint.a));
			if (build_static_double_sided_stack) {
				godot_stack_depths.set(i * 4 + 0, stack_depth);
				godot_stack_depths.set(i * 4 + 1, 0.0f);
				godot_stack_depths.set(i * 4 + 2, 0.0f);
				godot_stack_depths.set(i * 4 + 3, 0.0f);
			}
			if (build_back_geometry) {
				const Vector3 back_local_vertex = Vector3(x, y, 0.0f) + back_slot_depth_offset;
				godot_back_vertices.set(i, stream_space ? to_stream_local.xform(back_local_vertex) : back_local_vertex);
				godot_back_normals.set(i, back_run_normal);
			}
			if (two_color_tint) {
				godot_dark_colors.set(i * 4 + 0, slot_dark_color.r);
				godot_dark_colors.set(i * 4 + 1, slot_dark_color.g);
				godot_dark_colors.set(i * 4 + 2, slot_dark_color.b);
				godot_dark_colors.set(i * 4 + 3, 1.0f);
				if (build_back_geometry) {
					godot_back_dark_colors.set(i * 4 + 0, slot_dark_color.r);
					godot_back_dark_colors.set(i * 4 + 1, slot_dark_color.g);
					godot_back_dark_colors.set(i * 4 + 2, slot_dark_color.b);
					godot_back_dark_colors.set(i * 4 + 3, 1.0f);
				}
			}
		}
		if (build_back_geometry) {
			build_oriented_spine_sprite_3d_indices(*indices, godot_vertices, run_normal, godot_indices);
		} else {
			godot_indices.resize((int)indices->size());
			for (int i = 0; i < (int)indices->size(); ++i) godot_indices.set(i, indices->buffer()[i]);
		}
		if (build_back_geometry) {
			build_oriented_spine_sprite_3d_indices(*indices, godot_back_vertices, back_run_normal, godot_back_indices);
		}
		build_spine_sprite_3d_tangents(godot_vertices, godot_uvs, godot_indices, run_normal, generated_normal_map_flip_y, godot_tangents);
		if (build_back_geometry) {
			build_spine_sprite_3d_tangents(godot_back_vertices, godot_uvs, godot_back_indices, back_run_normal, generated_normal_map_flip_y, godot_back_tangents);
		}

		const int slot_data_index = slot->getData().getIndex();
		SpineSlotNode3D *slot_node = find_slot_node_for_index(slot_data_index);

		Ref<Material> slot_override;
		if (slot_node) {
			switch (slot_blend_mode) {
				case spine::BlendMode_Normal: slot_override = slot_node->get_normal_material(); break;
				case spine::BlendMode_Additive: slot_override = slot_node->get_additive_material(); break;
				case spine::BlendMode_Multiply: slot_override = slot_node->get_multiply_material(); break;
				case spine::BlendMode_Screen: slot_override = slot_node->get_screen_material(); break;
			}
		}
		if (stream_space && !slot_override.is_valid()) {
			switch (slot_blend_mode) {
				case spine::BlendMode_Normal: slot_override = normal_material; break;
				case spine::BlendMode_Additive: slot_override = additive_material; break;
				case spine::BlendMode_Multiply: slot_override = multiply_material; break;
				case spine::BlendMode_Screen: slot_override = screen_material; break;
			}
		}

		Ref<Texture> effective_normal_map = generated_normal_map_override.is_valid() ? generated_normal_map_override : renderer_object->normal_map;
		const bool should_render_visible = shadow_casting_mode != GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY;
		Ref<Material> material;
		if (should_render_visible) {
			material = stream_space ? render_world->resolve_visible_material((int)slot_blend_mode, renderer_object->texture, effective_normal_map, slot_override, lighting_enabled, double_sided, generated_normal_map_enabled, generated_normal_map_preview, two_color_tint, generated_normal_scale, generated_normal_max_slope, generated_normal_dark_suppression, generated_standard_specular, generated_standard_roughness, generated_standard_metallic, generated_shader_light_scale, generated_shader_ambient, visible_alpha_cutoff) : resolve_visible_slot_material(slot_blend_mode, renderer_object->texture, effective_normal_map, render_priority, two_color_tint, slot_override);
		}
		Ref<Material> back_material = material;
		if (build_back_geometry && should_render_visible && !stream_space) {
			back_material = resolve_visible_slot_material(slot_blend_mode, renderer_object->texture, effective_normal_map, back_render_priority, two_color_tint, slot_override);
		}
		const bool should_cast_shadow = shadow_casting_mode != GeometryInstance3D::SHADOW_CASTING_SETTING_OFF && (slot_blend_mode == spine::BlendMode_Normal || slot_blend_mode == spine::BlendMode_Multiply);
		Ref<Material> shadow_material = should_cast_shadow ? (stream_space ? render_world->resolve_shadow_material(renderer_object->texture, shadow_alpha_cutoff, (int)shadow_casting_mode, double_sided) : resolve_shadow_slot_material(renderer_object->texture)) : Ref<Material>();

		append_geometry_to_runs(runs, slot_order, render_priority, material, should_cast_shadow, shadow_material, godot_vertices, godot_uvs, godot_normals, godot_tangents, godot_colors, godot_dark_colors, godot_stack_depths, godot_indices);
		if (build_back_geometry) {
			append_geometry_to_runs(back_runs, back_slot_order, back_render_priority, back_material, should_cast_shadow, shadow_material, godot_back_vertices, godot_uvs, godot_back_normals, godot_back_tangents, godot_colors, godot_back_dark_colors, godot_back_stack_depths, godot_back_indices);
		}
		last_generated_slot_count++;
		skeleton_clipper->clipEnd(*slot);
	}
	skeleton_clipper->clipEnd();
	if (build_back_geometry) {
		runs.insert(runs.end(), back_runs.begin(), back_runs.end());
	}
	last_generated_run_count = (int)runs.size();
	return true;
}

void SpineSprite3D::rebuild_runtime_mesh() {
	if (!runtime_mesh.is_valid()) return;
	if (!shadow_runtime_mesh.is_valid()) return;
	clear_runtime_mesh_surfaces();
	if (should_stream_to_world()) return;

	std::vector<RenderRun> runs;
	spine::Skeleton *skeleton_object = nullptr;
	Vector3 slot_stack_axis_local;
	float effective_depth_scale = 1.0f;
	int slot_count = 0;
	if (!build_render_runs(runs, false, nullptr, skeleton_object, slot_stack_axis_local, effective_depth_scale, slot_count)) return;

	for (const RenderRun &run : runs) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = run.vertices;
		arrays[Mesh::ARRAY_TEX_UV] = run.uvs;
		arrays[Mesh::ARRAY_NORMAL] = run.normals;
		arrays[Mesh::ARRAY_TANGENT] = run.tangents;
		arrays[Mesh::ARRAY_COLOR] = run.colors;
		const bool has_dark_colors = run.dark_colors.size() == run.vertices.size() * 4;
		const bool has_stack_depths = run.stack_depths.size() == run.vertices.size() * 4;
		if (has_dark_colors) arrays[Mesh::ARRAY_CUSTOM0] = run.dark_colors;
		if (has_stack_depths) arrays[Mesh::ARRAY_CUSTOM1] = run.stack_depths;
		arrays[Mesh::ARRAY_INDEX] = run.indices;
		if (run.visible) {
			runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), make_spine_sprite_3d_custom_array_flags(has_dark_colors, has_stack_depths));
			runtime_mesh->surface_set_material(runtime_mesh->get_surface_count() - 1, run.material);
		}
		if (run.casts_shadow) {
			shadow_runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), make_spine_sprite_3d_custom_array_flags(has_dark_colors, has_stack_depths));
			shadow_runtime_mesh->surface_set_material(shadow_runtime_mesh->get_surface_count() - 1, run.shadow_material);
		}
	}
	apply_shadow_mesh_state();
	rebuild_debug_mesh(skeleton_object, slot_stack_axis_local, effective_depth_scale, slot_count);
}

void SpineSprite3D::submit_render_parts(SpineRenderWorld3D *render_world, int collector_object_index) {
	if (!render_world) return;
	if (!is_inside_tree()) return;

	std::vector<RenderRun> runs;
	spine::Skeleton *skeleton_object = nullptr;
	Vector3 slot_stack_axis_local;
	float effective_depth_scale = 1.0f;
	int slot_count = 0;
	if (!build_render_runs(runs, true, render_world, skeleton_object, slot_stack_axis_local, effective_depth_scale, slot_count)) return;

	for (const RenderRun &run : runs) {
		render_world->submit_run(stream_render_order, collector_object_index, run.first_slot_order, run.last_slot_order, run.material, run.casts_shadow, run.shadow_material, run.vertices, run.uvs, run.normals, run.tangents, run.colors, run.dark_colors, run.stack_depths, run.indices);
	}
}

void SpineSprite3D::rebuild_debug_mesh(spine::Skeleton *skeleton_object, const Vector3 &slot_stack_axis_local, float effective_depth_scale, int slot_count) {
	if (!debug_runtime_mesh.is_valid() || !debug_mesh_instance_3d || !skeleton_object) return;

	const bool any_debug_enabled = debug_root || debug_bones || debug_regions || debug_meshes || debug_bounding_boxes || debug_paths || debug_clipping;
	const bool debug_allowed = Engine::get_singleton()->is_editor_hint() || (is_inside_tree() && get_tree() && get_tree()->is_debugging_collisions_hint());
	if (!any_debug_enabled || !debug_allowed) {
		debug_mesh_instance_3d->set_visible(false);
		return;
	}

	const float ppu = MAX(0.0001f, pixels_per_unit);
	const float front_depth = ((float)MAX(slot_count, 1) + 1.0f) * depth_offset / MAX(effective_depth_scale, 0.0001f);
	const Vector3 debug_depth_offset = slot_stack_axis_local * front_depth;
	SpineSprite3DDebugSurfaceBuilder builder;

	static spine::Vector<unsigned short> quad_indices = []() {
		spine::Vector<unsigned short> arr;
		arr.setSize(6, 0);
		arr[0] = 0; arr[1] = 1; arr[2] = 2; arr[3] = 2; arr[4] = 3; arr[5] = 0;
		return arr;
	}();

	spine::Vector<float> world_vertices;
#ifdef SPINE_RUNTIME_43
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder().getAppliedPose();
#else
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder();
#endif
	for (int slot_order = 0; slot_order < (int)draw_order.size(); ++slot_order) {
		spine::Slot *slot = draw_order[slot_order];
		if (!slot || !slot->getBone().isActive()) continue;

#ifdef SPINE_RUNTIME_43
		spine::Attachment *attachment = slot->getAppliedPose().getAttachment();
#else
		spine::Attachment *attachment = slot->getAttachment();
#endif
		if (!attachment) continue;

		const float stack_depth = (float)slot_order * depth_offset / MAX(effective_depth_scale, 0.0001f);
		const Vector3 slot_depth_offset = debug_depth_offset + slot_stack_axis_local * stack_depth;

		if (debug_regions && attachment->getRTTI().isExactly(spine::RegionAttachment::rtti)) {
			auto *region = (spine::RegionAttachment *)attachment;
			world_vertices.setSize(8, 0);
#ifdef SPINE_RUNTIME_43
			auto &sequence = region->getSequence();
			int sequence_index = sequence.resolveIndex(slot->getAppliedPose());
			region->computeWorldVertices(*slot, sequence.getOffsets(sequence_index).buffer(), world_vertices.buffer(), 0);
#elif defined(SPINE_RUNTIME_41_PLUS)
			region->computeWorldVertices(*slot, world_vertices, 0);
#else
			region->computeWorldVertices(slot->getBone(), world_vertices, 0);
#endif
			add_debug_mesh_edges(builder, quad_indices, world_vertices, ppu, slot_depth_offset, debug_regions_color);
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, 4, ppu, slot_depth_offset), debug_regions_color, true);
			continue;
		}

		if (debug_meshes && attachment->getRTTI().isExactly(spine::MeshAttachment::rtti)) {
			auto *mesh = (spine::MeshAttachment *)attachment;
			world_vertices.setSize(mesh->getWorldVerticesLength(), 0);
#ifdef SPINE_RUNTIME_43
			mesh->computeWorldVertices(*skeleton_object, *slot, 0, mesh->getWorldVerticesLength(), world_vertices.buffer(), 0, 2);
#else
			mesh->computeWorldVertices(*slot, world_vertices);
#endif
			add_debug_mesh_edges(builder, mesh->getTriangles(), world_vertices, ppu, slot_depth_offset, debug_meshes_color);
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, mesh->getHullLength(), ppu, slot_depth_offset), debug_meshes_color, true);
			continue;
		}

		if (debug_bounding_boxes && attachment->getRTTI().isExactly(spine::BoundingBoxAttachment::rtti)) {
			auto *bounding_box = (spine::BoundingBoxAttachment *)attachment;
			world_vertices.setSize(bounding_box->getWorldVerticesLength(), 0);
#ifdef SPINE_RUNTIME_43
			bounding_box->computeWorldVertices(*skeleton_object, *slot, 0, bounding_box->getWorldVerticesLength(), world_vertices.buffer(), 0, 2);
#else
			bounding_box->computeWorldVertices(*slot, world_vertices);
#endif
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, (int)world_vertices.size() / 2, ppu, slot_depth_offset), debug_bounding_boxes_color, true);
			continue;
		}

		if (debug_paths && attachment->getRTTI().isExactly(spine::PathAttachment::rtti)) {
			auto *path = (spine::PathAttachment *)attachment;
			world_vertices.setSize(path->getWorldVerticesLength(), 0);
#ifdef SPINE_RUNTIME_43
			path->computeWorldVertices(*skeleton_object, *slot, 0, path->getWorldVerticesLength(), world_vertices.buffer(), 0, 2);
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, (int)world_vertices.size() / 2, ppu, slot_depth_offset), debug_paths_color, path->getClosed());
#else
			path->computeWorldVertices(*slot, world_vertices);
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, (int)world_vertices.size() / 2, ppu, slot_depth_offset), debug_paths_color, path->isClosed());
#endif
			continue;
		}

		if (debug_clipping && attachment->getRTTI().isExactly(spine::ClippingAttachment::rtti)) {
			auto *clipping = (spine::ClippingAttachment *)attachment;
			world_vertices.setSize(clipping->getWorldVerticesLength(), 0);
#ifdef SPINE_RUNTIME_43
			clipping->computeWorldVertices(*skeleton_object, *slot, 0, clipping->getWorldVerticesLength(), world_vertices.buffer(), 0, 2);
#else
			clipping->computeWorldVertices(*slot, world_vertices);
#endif
			builder.add_line_loop(make_debug_points_from_world_vertices(world_vertices, (int)world_vertices.size() / 2, ppu, slot_depth_offset), debug_clipping_color, true);
			continue;
		}
	}

	if (debug_root) add_debug_bone_shape(builder, skeleton_object->getRootBone(), debug_bones_thickness, ppu, debug_depth_offset, debug_root_color);
	if (debug_bones) {
		spine::Vector<spine::Bone *> &bones = skeleton_object->getBones();
		for (int i = 0; i < (int)bones.size(); ++i) {
			if (!bones[i] || !bones[i]->isActive()) continue;
			add_debug_bone_shape(builder, bones[i], debug_bones_thickness, ppu, debug_depth_offset, debug_bones_color);
		}
	}

	if (builder.line_vertices.is_empty() && builder.triangle_vertices.is_empty()) {
		debug_mesh_instance_3d->set_visible(false);
		return;
	}

	if (!builder.triangle_vertices.is_empty()) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = builder.triangle_vertices;
		arrays[Mesh::ARRAY_COLOR] = builder.triangle_colors;
		debug_runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		debug_runtime_mesh->surface_set_material(debug_runtime_mesh->get_surface_count() - 1, debug_material);
	}

	if (!builder.line_vertices.is_empty()) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = builder.line_vertices;
		arrays[Mesh::ARRAY_COLOR] = builder.line_colors;
		debug_runtime_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
		debug_runtime_mesh->surface_set_material(debug_runtime_mesh->get_surface_count() - 1, debug_material);
	}

	debug_mesh_instance_3d->set_visible(debug_runtime_mesh->get_surface_count() > 0);
}

SpineSlotNode3D *SpineSprite3D::find_ancestor_slot_node() const {
	Node *node = get_parent();
	while (node) {
		SpineSlotNode3D *slot_node = Object::cast_to<SpineSlotNode3D>(node);
		if (slot_node) {
			return slot_node;
		}
		node = node->get_parent();
	}
	return nullptr;
}

SpineRenderWorld3D *SpineSprite3D::find_stream_world() const {
	Node *node = get_parent();
	while (node) {
		SpineRenderWorld3D *render_world = Object::cast_to<SpineRenderWorld3D>(node);
		if (render_world) {
			return render_world;
		}
		node = node->get_parent();
	}
	return nullptr;
}

bool SpineSprite3D::should_stream_to_world() const {
	if (render_backend == RenderBackend_StreamWorld) return true;
	if (render_backend == RenderBackend_AutoStreamWorld) return find_stream_world() != nullptr;
	return false;
}

float SpineSprite3D::get_effective_depth_scale() const {
	const Transform3D global_transform = is_inside_tree() ? get_global_transform() : get_transform();
	float depth_scale =
#if VERSION_MAJOR > 3
			global_transform.basis.get_column(2).length();
#else
			global_transform.basis.get_axis(2).length();
#endif
	if (depth_scale < 0.0001f) {
		Vector3 scale = get_scale();
		depth_scale = Math::abs(scale.z);
	}
	return MAX(depth_scale, 0.0001f);
}

Vector3 SpineSprite3D::get_slot_stack_axis_local() const {
	return Vector3(0.0f, 0.0f, 1.0f);
}

float SpineSprite3D::get_effective_depth_scale_for_axis(const Vector3 &axis_local) const {
	Vector3 clamped_axis = axis_local;
	if (clamped_axis.length_squared() < CMP_EPSILON2) {
		clamped_axis = Vector3(0.0f, 0.0f, 1.0f);
	}

	const Transform3D transform = is_inside_tree() ? get_global_transform() : get_transform();
	Vector3 axis_world = transform.basis.xform(clamped_axis);
	float depth_scale = axis_world.length();
	if (depth_scale < 0.0001f) {
		depth_scale = get_effective_depth_scale();
	}
	return MAX(depth_scale, 0.0001f);
}

bool SpineSprite3D::get_effective_reverse_slot_stack() const {
	return false;
}

int SpineSprite3D::get_visual_slot_order(int slot_order, int slot_count) const {
	if (slot_count <= 0) return 0;
	return CLAMP(slot_order, 0, slot_count - 1);
}

int SpineSprite3D::find_draw_order_index_for_slot_index(int slot_index) const {
	if (!skeleton.is_valid()) return -1;
	spine::Skeleton *skeleton_object = skeleton->get_spine_object();
	if (!skeleton_object) return -1;

#ifdef SPINE_RUNTIME_43
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder().getAppliedPose();
#else
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder();
#endif
	for (int i = 0, n = (int)draw_order.size(); i < n; ++i) {
		if (draw_order[i] && draw_order[i]->getData().getIndex() == slot_index) {
			return i;
		}
	}
	return -1;
}

Vector3 SpineSprite3D::get_slot_insert_offset(int draw_order_index, int slot_count) const {
	if (slot_count <= 0) return Vector3();
	const int visual_slot_order = get_visual_slot_order(draw_order_index, slot_count);
	const Vector3 slot_stack_axis_local = double_sided ? Vector3(0.0f, 0.0f, 1.0f) : get_slot_stack_axis_local();
	const float local_depth = ((float)visual_slot_order + 0.5f) * get_depth_offset() / get_effective_depth_scale_for_axis(slot_stack_axis_local);
	return slot_stack_axis_local * local_depth;
}

float SpineSprite3D::get_slot_insert_z(int draw_order_index, int slot_count) const {
	return get_slot_insert_offset(draw_order_index, slot_count).z;
}

int SpineSprite3D::get_slot_insert_render_priority(int draw_order_index, int slot_count) const {
	if (slot_count <= 0) return 0;

	const int visual_slot_order = get_visual_slot_order(draw_order_index, slot_count);
	const int priority_a = compute_slot_render_priority(visual_slot_order, slot_count);
	if (visual_slot_order + 1 < slot_count) {
		const int priority_b = compute_slot_render_priority(visual_slot_order + 1, slot_count);
		return (priority_a + priority_b) / 2;
	}

	return CLAMP(priority_a + 1, -128, 127);
}

int SpineSprite3D::compute_slot_insert_band_render_priority(int draw_order_index, int slot_count, float t) const {
	if (slot_count <= 0) return 0;

	const float clamped_t = CLAMP(t, 0.0f, 1.0f);
	const int visual_slot_order = get_visual_slot_order(draw_order_index, slot_count);
	const int priority_a = compute_slot_render_priority(visual_slot_order, slot_count);
	const int priority_b = visual_slot_order + 1 < slot_count
			? compute_slot_render_priority(visual_slot_order + 1, slot_count)
			: CLAMP(priority_a + 1, -128, 127);

	return CLAMP(
			priority_a + (int)Math::round((float)(priority_b - priority_a) * clamped_t),
			-128,
			127);
}

int SpineSprite3D::compute_slot_render_priority(int slot_order, int slot_count) const {
	int min_p = CLAMP(render_priority_bias - render_priority_range, -128, 127);
	int max_p = CLAMP(render_priority_bias + render_priority_range, -128, 127);
	if (min_p > max_p) {
		SWAP(min_p, max_p);
	}

	if (slot_count <= 1) {
		return CLAMP(render_priority_bias, -128, 127);
	}

	const float t = (float)slot_order / (float)(slot_count - 1);
	return min_p + (int)Math::round(t * (float)(max_p - min_p));
}

Ref<SpineSkeleton> SpineSprite3D::get_skeleton() { return skeleton; }
Ref<SpineAnimationState> SpineSprite3D::get_animation_state() {
	ensure_runtime_nodes();
	if (!sprite_2d) return nullptr;
	sync_from_sprite();
	if (!animation_state.is_valid() && skeleton_data_res.is_valid() && skeleton_data_res->is_skeleton_data_loaded()) {
		on_skeleton_data_changed();
		sync_from_sprite();
	}
	return animation_state;
}

Ref<SpineTrackEntry> SpineSprite3D::set_animation(const String &animation_name, bool loop, int track_id) {
	sync_from_sprite();
	clear_track_state(track_id);
	RestorableTrackState state{true, animation_name, loop, track_id, 0.0f};
	restorable_track_states.push_back(state);
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_SetAnimation;
		command.animation_name = animation_name;
		command.loop = loop;
		command.track_id = track_id;
		command.delay = 0;
		command.mix_duration = 0;
		pending_animation_commands.push_back(command);
		return nullptr;
	}
	auto entry = animation_state->set_animation(animation_name, loop, track_id);
	remember_track_state(entry);
	return entry;
}

Ref<SpineTrackEntry> SpineSprite3D::add_animation(const String &animation_name, float delay, bool loop, int track_id) {
	sync_from_sprite();
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_AddAnimation;
		command.animation_name = animation_name;
		command.loop = loop;
		command.track_id = track_id;
		command.delay = delay;
		command.mix_duration = 0;
		pending_animation_commands.push_back(command);
		return nullptr;
	}
	auto entry = animation_state->add_animation(animation_name, delay, loop, track_id);
	remember_track_state(entry);
	return entry;
}

Ref<SpineTrackEntry> SpineSprite3D::set_empty_animation(int track_id, float mix_duration) {
	sync_from_sprite();
	clear_track_state(track_id);
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_SetEmptyAnimation;
		command.animation_name = "";
		command.loop = false;
		command.track_id = track_id;
		command.delay = 0;
		command.mix_duration = mix_duration;
		pending_animation_commands.push_back(command);
		return nullptr;
	}
	return animation_state->set_empty_animation(track_id, mix_duration);
}

Ref<SpineTrackEntry> SpineSprite3D::add_empty_animation(int track_id, float mix_duration, float delay) {
	sync_from_sprite();
	clear_track_state(track_id);
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_AddEmptyAnimation;
		command.animation_name = "";
		command.loop = false;
		command.track_id = track_id;
		command.delay = delay;
		command.mix_duration = mix_duration;
		pending_animation_commands.push_back(command);
		return nullptr;
	}
	return animation_state->add_empty_animation(track_id, mix_duration, delay);
}

void SpineSprite3D::set_empty_animations(float mix_duration) {
	sync_from_sprite();
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_SetEmptyAnimations;
		command.animation_name = "";
		command.loop = false;
		command.track_id = 0;
		command.delay = 0;
		command.mix_duration = mix_duration;
		pending_animation_commands.push_back(command);
		return;
	}
	animation_state->set_empty_animations(mix_duration);
}

void SpineSprite3D::clear_tracks() {
	sync_from_sprite();
	clear_all_track_states();
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_ClearTracks;
		command.animation_name = "";
		command.loop = false;
		command.track_id = 0;
		command.delay = 0;
		command.mix_duration = 0;
		pending_animation_commands.push_back(command);
		return;
	}
	animation_state->clear_tracks();
}

void SpineSprite3D::clear_track(int track_id) {
	sync_from_sprite();
	clear_track_state(track_id);
	if (!animation_state.is_valid()) {
		PendingAnimationCommand command;
		command.type = PendingAnimationCommand::Type_ClearTrack;
		command.animation_name = "";
		command.loop = false;
		command.track_id = track_id;
		command.delay = 0;
		command.mix_duration = 0;
		pending_animation_commands.push_back(command);
		return;
	}
	animation_state->clear_track(track_id);
}

void SpineSprite3D::update_skeleton(float delta) {
	if (!sprite_2d) { log_checkpoint("update_skeleton: sprite_2d is null"); return; }
	sync_from_sprite();
	sprite_2d->update_skeleton(delta);
	sync_from_sprite();
	rebuild_runtime_mesh();
}

void SpineSprite3D::update_preview_animation() {
	if (!Engine::get_singleton()->is_editor_hint() || !sprite_2d) return;
	if (!skeleton.is_valid() || !animation_state.is_valid()) return;

	if (EMPTY(preview_skin) || preview_skin == "Default") skeleton->set_skin(nullptr);
	else skeleton->set_skin_by_name(preview_skin);

	skeleton->set_to_setup_pose();
	if (EMPTY(preview_animation) || preview_animation == "-- Empty --" || preview_animation == "<empty>") {
		animation_state->set_empty_animation(0, 0);
		return;
	}
	auto track_entry = animation_state->set_animation(preview_animation, true, 0);
	if (track_entry.is_valid()) {
		track_entry->set_mix_duration(0);
		if (preview_frame) {
			track_entry->set_time_scale(0);
			track_entry->set_track_time(preview_time);
		}
	}
	sprite_2d->update_skeleton(0);
	rebuild_runtime_mesh();
}

void SpineSprite3D::_get_property_list(List<PropertyInfo> *list) const {
	if (!skeleton_data_res.is_valid() || !skeleton_data_res->is_skeleton_data_loaded()) return;
	PackedStringArray animation_names;
	PackedStringArray skin_names;
	skeleton_data_res->get_animation_names(animation_names);
	skeleton_data_res->get_skin_names(skin_names);
	animation_names.insert(0, "-- Empty --");

	PropertyInfo preview_skin_property;
	preview_skin_property.name = "preview_skin";
	preview_skin_property.type = Variant::STRING;
	preview_skin_property.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
	preview_skin_property.hint_string = String(",").join(skin_names);
	preview_skin_property.hint = PROPERTY_HINT_ENUM;
	list->push_back(preview_skin_property);

	PropertyInfo preview_anim_property;
	preview_anim_property.name = "preview_animation";
	preview_anim_property.type = Variant::STRING;
	preview_anim_property.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
	preview_anim_property.hint_string = String(",").join(animation_names);
	preview_anim_property.hint = PROPERTY_HINT_ENUM;
	list->push_back(preview_anim_property);

	list->push_back(PropertyInfo(Variant::BOOL, "preview_frame", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE));
	float animation_duration = 0;
	if (!EMPTY(preview_animation) && preview_animation != "-- Empty --" && preview_animation != "<empty>") {
		auto animation = skeleton_data_res->find_animation(preview_animation);
		if (animation.is_valid()) animation_duration = animation->get_duration();
	}
	String preview_hint = String("0.0,") + String::num(animation_duration) + String(",0.01");
	list->push_back(PropertyInfo(Variant::FLOAT, "preview_time", PROPERTY_HINT_RANGE, preview_hint, PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE));
}

bool SpineSprite3D::_get(const StringName &property, Variant &value) const {
	if (property == StringName("preview_skin")) { value = preview_skin; return true; }
	if (property == StringName("preview_animation")) { value = preview_animation; return true; }
	if (property == StringName("preview_frame")) { value = preview_frame; return true; }
	if (property == StringName("preview_time")) { value = preview_time; return true; }
	return false;
}

bool SpineSprite3D::_set(const StringName &property, const Variant &value) {
	if (property == StringName("preview_skin")) { preview_skin = value; update_preview_animation(); rebuild_runtime_mesh(); NOTIFY_PROPERTY_LIST_CHANGED(); return true; }
	if (property == StringName("preview_animation")) { preview_animation = value; update_preview_animation(); rebuild_runtime_mesh(); NOTIFY_PROPERTY_LIST_CHANGED(); return true; }
	if (property == StringName("preview_frame")) { preview_frame = value; update_preview_animation(); rebuild_runtime_mesh(); return true; }
	if (property == StringName("preview_time")) { preview_time = value; update_preview_animation(); rebuild_runtime_mesh(); return true; }
	return false;
}

void SpineSprite3D::on_sprite_animation_started(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry) {
	remember_track_state(entry);
	emit_signal(SNAME("animation_started"), this, state, entry);
}

void SpineSprite3D::on_sprite_animation_interrupted(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry) {
	emit_signal(SNAME("animation_interrupted"), this, state, entry);
}

void SpineSprite3D::on_sprite_animation_ended(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry) {
	emit_signal(SNAME("animation_ended"), this, state, entry);
}

void SpineSprite3D::on_sprite_animation_completed(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry) {
	emit_signal(SNAME("animation_completed"), this, state, entry);
}

void SpineSprite3D::on_sprite_animation_disposed(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry) {
	if (entry.is_valid()) clear_track_state(entry->get_track_index());
	emit_signal(SNAME("animation_disposed"), this, state, entry);
}

void SpineSprite3D::on_sprite_animation_event(SpineSprite *, Ref<SpineAnimationState> state, Ref<SpineTrackEntry> entry, Ref<SpineEvent> event) {
	emit_signal(SNAME("animation_event"), this, state, entry, event);
}

void SpineSprite3D::on_sprite_before_animation_state_update(SpineSprite *) {
	emit_signal(SNAME("before_animation_state_update"), this);
}

void SpineSprite3D::on_sprite_before_animation_state_apply(SpineSprite *) {
	emit_signal(SNAME("before_animation_state_apply"), this);
}

void SpineSprite3D::on_sprite_before_world_transforms_change(SpineSprite *) {
	if (!is_inside_tree()) return;
	emit_signal(SNAME("before_world_transforms_change"), this);
}

void SpineSprite3D::on_sprite_world_transforms_changed(SpineSprite *) {
	if (!is_inside_tree()) return;
	apply_bone_overrides();
	emit_signal(SNAME("world_transforms_changed"), this);
}

void SpineSprite3D::callback(spine::AnimationState *state, spine::EventType type, spine::TrackEntry *entry, spine::Event *event) {
	if (!sprite_2d) return;
	Ref<SpineTrackEntry> entry_ref = Ref<SpineTrackEntry>(memnew(SpineTrackEntry));
	entry_ref->set_spine_object(sprite_2d, entry);

	Ref<SpineEvent> event_ref(nullptr);
	if (event) {
		event_ref = Ref<SpineEvent>(memnew(SpineEvent));
		event_ref->set_spine_object(sprite_2d, event);
	}

	switch (type) {
		case spine::EventType_Start: emit_signal(SNAME("animation_started"), this, animation_state, entry_ref); break;
		case spine::EventType_Interrupt: emit_signal(SNAME("animation_interrupted"), this, animation_state, entry_ref); break;
		case spine::EventType_End: emit_signal(SNAME("animation_ended"), this, animation_state, entry_ref); break;
		case spine::EventType_Complete: emit_signal(SNAME("animation_completed"), this, animation_state, entry_ref); break;
		case spine::EventType_Dispose: emit_signal(SNAME("animation_disposed"), this, animation_state, entry_ref); break;
		case spine::EventType_Event: emit_signal(SNAME("animation_event"), this, animation_state, entry_ref, event_ref); break;
	}
}

SpineConstant::UpdateMode SpineSprite3D::get_update_mode() { return update_mode; }
void SpineSprite3D::set_update_mode(SpineConstant::UpdateMode mode) {
	update_mode = mode;
	set_process(update_mode == SpineConstant::UpdateMode_Process);
	set_process_internal(true);
	set_physics_process(update_mode == SpineConstant::UpdateMode_Physics);
	set_physics_process_internal(false);
	if (sprite_2d) sprite_2d->set_update_mode(SpineConstant::UpdateMode_Manual);
}

void SpineSprite3D::set_time_scale(float v) { time_scale = v; if (sprite_2d) sprite_2d->set_time_scale(v); }
float SpineSprite3D::get_time_scale() { return time_scale; }
void SpineSprite3D::set_pixels_per_unit(float v) { pixels_per_unit = MAX(0.0001f, v); rebuild_runtime_mesh(); }
float SpineSprite3D::get_pixels_per_unit() const { return pixels_per_unit; }
void SpineSprite3D::set_depth_offset(float v) { depth_offset = MAX(0.0001f, v); rebuild_runtime_mesh(); }
float SpineSprite3D::get_depth_offset() const { return depth_offset; }
void SpineSprite3D::set_render_embedded_in_parent_slot(bool v) {
	if (render_embedded_in_parent_slot == v) return;
	render_embedded_in_parent_slot = v;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_render_priority_bias(int v) {
	const int clamped = CLAMP(v, -128, 127);
	if (render_priority_bias == clamped) return;
	render_priority_bias = clamped;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
int SpineSprite3D::get_render_priority_bias() const { return render_priority_bias; }
void SpineSprite3D::set_render_priority_range(int v) {
	const int clamped = CLAMP(v, 0, 127);
	if (render_priority_range == clamped) return;
	render_priority_range = clamped;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
int SpineSprite3D::get_render_priority_range() const { return render_priority_range; }
void SpineSprite3D::set_render_backend(int v) {
	const int clamped = CLAMP(v, (int)RenderBackend_ReferenceMesh, (int)RenderBackend_AutoStreamWorld);
	if (render_backend == clamped) return;
	render_backend = clamped;
	if (mesh_instance_3d) mesh_instance_3d->set_visible(!should_stream_to_world());
	if (shadow_mesh_instance_3d) shadow_mesh_instance_3d->set_visible(!should_stream_to_world());
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_stream_render_order(int v) {
	stream_render_order = CLAMP(v, -524288, 524287);
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_lighting_enabled(bool v) {
	if (lighting_enabled == v) return;
	lighting_enabled = v;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_double_sided(bool v) {
	if (double_sided == v) return;
	double_sided = v;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_shadow_casting_mode(int v) {
	const GeometryInstance3D::ShadowCastingSetting new_mode = (GeometryInstance3D::ShadowCastingSetting)CLAMP(v, 0, 3);
	if (shadow_casting_mode == new_mode) return;
	shadow_casting_mode = new_mode;
	if (mesh_instance_3d) mesh_instance_3d->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	apply_shadow_mesh_state();
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_shadow_alpha_cutoff(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(shadow_alpha_cutoff, clamped)) return;
	shadow_alpha_cutoff = clamped;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}
void SpineSprite3D::set_visible_alpha_cutoff(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(visible_alpha_cutoff, clamped)) return;
	visible_alpha_cutoff = clamped;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_material_mode(int v) {
	const int clamped = CLAMP(v, (int)GeneratedMaterialMode_StandardMaterial3D, (int)GeneratedMaterialMode_FilteredShader);
	if (generated_material_mode == clamped) return;
	generated_material_mode = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_map_enabled(bool v) {
	if (generated_normal_map_enabled == v) return;
	generated_normal_map_enabled = v;
	if (sprite_2d) sprite_2d->set_normal_map_enabled(v);
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_map_preview(bool v) {
	if (generated_normal_map_preview == v) return;
	generated_normal_map_preview = v;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_map_flip_y(bool v) {
	if (generated_normal_map_flip_y == v) return;
	generated_normal_map_flip_y = v;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_map_override(Ref<Texture> texture) {
	if (generated_normal_map_override == texture) return;
	generated_normal_map_override = texture;
	if (sprite_2d) sprite_2d->set_normal_map_override(texture);
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_scale(float v) {
	const float clamped = CLAMP(v, 0.0f, 2.0f);
	if (Math::is_equal_approx(generated_normal_scale, clamped)) return;
	generated_normal_scale = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_max_slope(float v) {
	const float clamped = CLAMP(v, 0.05f, 1.0f);
	if (Math::is_equal_approx(generated_normal_max_slope, clamped)) return;
	generated_normal_max_slope = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_normal_dark_suppression(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(generated_normal_dark_suppression, clamped)) return;
	generated_normal_dark_suppression = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_standard_specular(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(generated_standard_specular, clamped)) return;
	generated_standard_specular = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_standard_roughness(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(generated_standard_roughness, clamped)) return;
	generated_standard_roughness = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_standard_metallic(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(generated_standard_metallic, clamped)) return;
	generated_standard_metallic = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_shader_light_scale(float v) {
	const float clamped = CLAMP(v, 0.0f, 4.0f);
	if (Math::is_equal_approx(generated_shader_light_scale, clamped)) return;
	generated_shader_light_scale = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

void SpineSprite3D::set_generated_shader_ambient(float v) {
	const float clamped = CLAMP(v, 0.0f, 1.0f);
	if (Math::is_equal_approx(generated_shader_ambient, clamped)) return;
	generated_shader_ambient = clamped;
	generated_material_cache.clear();
	custom_material_priority_cache.clear();
	rebuild_runtime_mesh();
}

Ref<SpineSkin> SpineSprite3D::new_skin(const String &name) {
	ensure_runtime_nodes();
	Ref<SpineSkin> skin = memnew(SpineSkin);
	skin->init(name, sprite_2d);
	return skin;
}

Transform2D SpineSprite3D::get_global_bone_transform(const String &bone_name) {
	sync_from_sprite();
	if (!skeleton.is_valid()) return Transform2D();
	spine::Skeleton *skeleton_object = skeleton->get_spine_object();
	if (!skeleton_object) return Transform2D();
	spine::Bone *bone = skeleton_object->findBone(SPINE_STRING_TMP(bone_name));
	if (!bone) return Transform2D();
	return get_raw_bone_global_transform(bone);
}

void SpineSprite3D::set_global_bone_transform(const String &bone_name, Transform2D transform) {
	sync_from_sprite();
	set_bone_override(bone_name, transform);
	if (!skeleton.is_valid()) return;
	spine::Skeleton *skeleton_object = skeleton->get_spine_object();
	if (!skeleton_object) return;
	spine::Bone *bone = skeleton_object->findBone(SPINE_STRING_TMP(bone_name));
	if (!bone) return;
	set_raw_bone_global_transform(bone, transform);
	skeleton->update_world_transform(SpineConstant::Physics_Update);
	sync_from_sprite();
	rebuild_runtime_mesh();
}

Ref<Material> SpineSprite3D::get_normal_material() { return normal_material; }
void SpineSprite3D::set_normal_material(Ref<Material> material) {
	normal_material = material;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	if (sprite_2d) sprite_2d->set_normal_material(material);
	rebuild_runtime_mesh();
}
Ref<Material> SpineSprite3D::get_additive_material() { return additive_material; }
void SpineSprite3D::set_additive_material(Ref<Material> material) {
	additive_material = material;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	if (sprite_2d) sprite_2d->set_additive_material(material);
	rebuild_runtime_mesh();
}
Ref<Material> SpineSprite3D::get_multiply_material() { return multiply_material; }
void SpineSprite3D::set_multiply_material(Ref<Material> material) {
	multiply_material = material;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	if (sprite_2d) sprite_2d->set_multiply_material(material);
	rebuild_runtime_mesh();
}
Ref<Material> SpineSprite3D::get_screen_material() { return screen_material; }
void SpineSprite3D::set_screen_material(Ref<Material> material) {
	screen_material = material;
	generated_material_cache.clear();
	generated_shadow_material_cache.clear();
	custom_material_priority_cache.clear();
	if (sprite_2d) sprite_2d->set_screen_material(material);
	rebuild_runtime_mesh();
}
