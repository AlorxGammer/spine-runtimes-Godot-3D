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
#include "SpineBoneNode3D.h"

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/math.hpp>
#else
#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#endif

SpineBoneNode3D::SpineBoneNode3D() :
		bone_mode(SpineConstant::BoneMode_Follow),
		enabled(true),
		bound_sprite(nullptr),
		applying_bone_transform(false) {
	set_notify_transform(true);
	set_notify_local_transform(true);
}

void SpineBoneNode3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bone_mode"), &SpineBoneNode3D::set_bone_mode);
	ClassDB::bind_method(D_METHOD("get_bone_mode"), &SpineBoneNode3D::get_bone_mode);
	ClassDB::bind_method(D_METHOD("set_enabled"), &SpineBoneNode3D::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &SpineBoneNode3D::get_enabled);
	ClassDB::bind_method(D_METHOD("_on_world_transforms_changed", "spine_sprite_3d"), &SpineBoneNode3D::on_world_transforms_changed);
	ClassDB::bind_method(D_METHOD("find_bone"), &SpineBoneNode3D::find_bone);
	ClassDB::bind_method(D_METHOD("find_sprite"), &SpineBoneNode3D::find_parent_sprite);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone_mode", PROPERTY_HINT_ENUM, "Follow,Drive"), "set_bone_mode", "get_bone_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
}

void SpineBoneNode3D::_notification(int what) {
	switch (what) {
		case NOTIFICATION_ENTER_TREE: {
			update_sprite_binding();
			if (bound_sprite) {
				update_transform(bound_sprite);
			}
			NOTIFY_PROPERTY_LIST_CHANGED();
			break;
		}
		case NOTIFICATION_PARENTED: {
			NOTIFY_PROPERTY_LIST_CHANGED();
			break;
		}
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_UNPARENTED: {
			update_sprite_binding();
			break;
		}
		case NOTIFICATION_TRANSFORM_CHANGED:
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (!enabled || applying_bone_transform || bone_mode != SpineConstant::BoneMode_Drive) break;
			if (!is_inside_tree()) break;
			if (!bound_sprite) break;
			drive_bone_from_node(bound_sprite);
			break;
		}
		default:
			break;
	}
}

void SpineBoneNode3D::_get_property_list(List<PropertyInfo> *list) const {
#ifdef SPINE_GODOT_EXTENSION
	PackedStringArray bone_names;
#else
	Vector<String> bone_names;
#endif
	SpineSprite3D *sprite = find_parent_sprite();
	if (sprite && sprite->get_skeleton_data_res().is_valid()) sprite->get_skeleton_data_res()->get_bone_names(bone_names);
	else bone_names.push_back(bone_name);

	PropertyInfo bone_name_property;
	bone_name_property.name = "bone_name";
	bone_name_property.type = Variant::STRING;
	bone_name_property.hint_string = String(",").join(bone_names);
	bone_name_property.hint = PROPERTY_HINT_ENUM;
	bone_name_property.usage = PROPERTY_USAGE_DEFAULT;
	list->push_back(bone_name_property);
}

bool SpineBoneNode3D::_get(const StringName &property, Variant &value) const {
	if (property == StringName("bone_name")) {
		value = bone_name;
		return true;
	}
	return false;
}

bool SpineBoneNode3D::_set(const StringName &property, const Variant &value) {
	if (property == StringName("bone_name")) {
		bone_name = value;
		init_transform(find_parent_sprite());
		return true;
	}
	return false;
}

void SpineBoneNode3D::update_sprite_binding() {
	SpineSprite3D *sprite = is_inside_tree() ? find_parent_sprite() : nullptr;
	if (bound_sprite == sprite) return;

	if (bound_sprite) {
#if VERSION_MAJOR > 3
		Callable world_transforms_changed_callable(this, SNAME("_on_world_transforms_changed"));
		if (bound_sprite->is_connected(SNAME("world_transforms_changed"), world_transforms_changed_callable)) {
			bound_sprite->disconnect(SNAME("world_transforms_changed"), world_transforms_changed_callable);
		}
#else
		if (bound_sprite->is_connected(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"))) {
			bound_sprite->disconnect(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"));
		}
#endif
	}

	bound_sprite = sprite;
	if (!bound_sprite) return;

#if VERSION_MAJOR > 3
	Callable world_transforms_changed_callable(this, SNAME("_on_world_transforms_changed"));
	if (!bound_sprite->is_connected(SNAME("world_transforms_changed"), world_transforms_changed_callable)) {
		bound_sprite->connect(SNAME("world_transforms_changed"), world_transforms_changed_callable);
	}
#else
	if (!bound_sprite->is_connected(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"))) {
		bound_sprite->connect(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"));
	}
#endif
}

void SpineBoneNode3D::on_world_transforms_changed(const Variant &spine_sprite) {
	SpineSprite3D *sprite = cast_to<SpineSprite3D>(spine_sprite.operator Object *());
	update_transform(sprite);
}

void SpineBoneNode3D::update_transform(SpineSprite3D *sprite) {
	if (!is_inside_tree()) return;
	if (!enabled || !sprite) return;
	Ref<SpineBone> bone = find_bone();
	if (!bone.is_valid()) return;

	if (bone_mode == SpineConstant::BoneMode_Drive) {
		drive_bone_from_node(sprite);
	} else {
		follow_bone_with_node(sprite);
	}
}

void SpineBoneNode3D::init_transform(SpineSprite3D *sprite) {
	if (!is_inside_tree()) return;
	if (!sprite) return;
	if (bone_mode == SpineConstant::BoneMode_Drive) return;
	if (!sprite->get_skeleton().is_valid()) return;

	sprite->get_skeleton()->set_to_setup_pose();
	sprite->get_skeleton()->update_world_transform(SpineConstant::Physics_Update);
	follow_bone_with_node(sprite);
}

Transform3D SpineBoneNode3D::bone_transform_to_3d(const Transform2D &bone_transform, float pixels_per_unit) {
	const float ppu = MAX(0.0001f, pixels_per_unit);
	const float rotation = bone_transform.get_rotation();
	const Vector2 scale = bone_transform.get_scale();
	const Vector2 origin = bone_transform.get_origin();

	Basis rotation_basis(Vector3(0.0f, 0.0f, 1.0f), -rotation);
	Basis basis(
			rotation_basis.get_column(0) * scale.x,
			rotation_basis.get_column(1) * scale.y,
			rotation_basis.get_column(2));

	return Transform3D(basis, Vector3(origin.x / ppu, -origin.y / ppu, 0.0f));
}

Transform2D SpineBoneNode3D::transform_3d_to_bone(const Transform3D &transform, float pixels_per_unit) {
	const float ppu = MAX(0.0001f, pixels_per_unit);
	const Vector3 x_axis_3d = transform.basis.get_column(0);
	const Vector3 y_axis_3d = transform.basis.get_column(1);
	const Vector2 x_axis(x_axis_3d.x, -x_axis_3d.y);
	const Vector2 y_axis(y_axis_3d.x, -y_axis_3d.y);
	const Vector2 origin(transform.origin.x * ppu, -transform.origin.y * ppu);

	Transform2D bone_transform;
	bone_transform.rotate(Math::atan2(x_axis.y, x_axis.x));
	bone_transform.scale(Vector2(x_axis.length(), y_axis.length()));
	bone_transform.set_origin(origin);
	return bone_transform;
}

void SpineBoneNode3D::drive_bone_from_node(SpineSprite3D *sprite) {
	if (!is_inside_tree()) return;
	if (!sprite) return;
	const float ppu = sprite->get_pixels_per_unit();
	const Transform3D sprite_global = sprite->get_global_transform();
	const Transform3D node_in_sprite_space = sprite_global.affine_inverse() * get_global_transform();
	sprite->set_global_bone_transform(bone_name, transform_3d_to_bone(node_in_sprite_space, ppu));
}

void SpineBoneNode3D::follow_bone_with_node(SpineSprite3D *sprite) {
	if (!is_inside_tree()) return;
	if (!sprite) return;
	const Transform2D bone_transform = sprite->get_global_bone_transform(bone_name);
	const Transform3D node_in_sprite_space = bone_transform_to_3d(bone_transform, sprite->get_pixels_per_unit());

	applying_bone_transform = true;
	set_global_transform(sprite->get_global_transform() * node_in_sprite_space);
	applying_bone_transform = false;
}

SpineSprite3D *SpineBoneNode3D::find_parent_sprite() const {
	Node *parent = get_parent();
	while (parent) {
		SpineSprite3D *sprite = cast_to<SpineSprite3D>(parent);
		if (sprite) return sprite;
		parent = parent->get_parent();
	}
	return nullptr;
}

Ref<SpineBone> SpineBoneNode3D::find_bone() const {
	SpineSprite3D *sprite = find_parent_sprite();
	if (!sprite) return nullptr;
	if (!sprite->get_skeleton().is_valid()) return nullptr;
	return sprite->get_skeleton()->find_bone(bone_name);
}

SpineConstant::BoneMode SpineBoneNode3D::get_bone_mode() {
	return bone_mode;
}

void SpineBoneNode3D::set_bone_mode(SpineConstant::BoneMode _bone_mode) {
	if (bone_mode == _bone_mode) return;
	bone_mode = _bone_mode;
	init_transform(find_parent_sprite());
}

void SpineBoneNode3D::set_enabled(bool _enabled) {
	enabled = _enabled;
	if (enabled && bound_sprite) {
		update_transform(bound_sprite);
	}
}

bool SpineBoneNode3D::get_enabled() {
	return enabled;
}
