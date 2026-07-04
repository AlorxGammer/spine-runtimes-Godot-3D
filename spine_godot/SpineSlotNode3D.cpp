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
#include "SpineSlotNode3D.h"

#include "SpineSprite3D.h"
#include <spine/Vector.h>

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/classes/sprite_base3d.hpp>
#else
#include "core/math/math_funcs.h"
#include "scene/3d/sprite_3d.h"
#endif

namespace {
void apply_slot_insert_render_priority_to_subtree(Node *node, int render_priority) {
	if (!node) return;

	if (Object::cast_to<SpineSprite3D>(node) || Object::cast_to<SpineSlotNode3D>(node)) {
		return;
	}

	SpriteBase3D *sprite_base_3d = Object::cast_to<SpriteBase3D>(node);
	if (sprite_base_3d) {
		sprite_base_3d->set_render_priority(render_priority);
	}

	const int child_count = node->get_child_count();
	for (int i = 0; i < child_count; ++i) {
		Node *child = Object::cast_to<Node>(node->get_child(i));
		apply_slot_insert_render_priority_to_subtree(child, render_priority);
	}
}
}

void SpineSlotNode3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_world_transforms_changed", "spine_sprite_3d"), &SpineSlotNode3D::on_world_transforms_changed);

	ClassDB::bind_method(D_METHOD("set_slot_name", "slot_name"), &SpineSlotNode3D::set_slot_name);
	ClassDB::bind_method(D_METHOD("get_slot_name"), &SpineSlotNode3D::get_slot_name);
	ClassDB::bind_method(D_METHOD("get_slot_index"), &SpineSlotNode3D::get_slot_index);

	ClassDB::bind_method(D_METHOD("set_normal_material", "material"), &SpineSlotNode3D::set_normal_material);
	ClassDB::bind_method(D_METHOD("get_normal_material"), &SpineSlotNode3D::get_normal_material);
	ClassDB::bind_method(D_METHOD("set_additive_material", "material"), &SpineSlotNode3D::set_additive_material);
	ClassDB::bind_method(D_METHOD("get_additive_material"), &SpineSlotNode3D::get_additive_material);
	ClassDB::bind_method(D_METHOD("set_multiply_material", "material"), &SpineSlotNode3D::set_multiply_material);
	ClassDB::bind_method(D_METHOD("get_multiply_material"), &SpineSlotNode3D::get_multiply_material);
	ClassDB::bind_method(D_METHOD("set_screen_material", "material"), &SpineSlotNode3D::set_screen_material);
	ClassDB::bind_method(D_METHOD("get_screen_material"), &SpineSlotNode3D::get_screen_material);
	ClassDB::bind_method(D_METHOD("set_position_offset", "v"), &SpineSlotNode3D::set_position_offset);
	ClassDB::bind_method(D_METHOD("get_position_offset"), &SpineSlotNode3D::get_position_offset);
	ClassDB::bind_method(D_METHOD("set_rotation_offset_degrees", "v"), &SpineSlotNode3D::set_rotation_offset_degrees);
	ClassDB::bind_method(D_METHOD("get_rotation_offset_degrees"), &SpineSlotNode3D::get_rotation_offset_degrees);
	ClassDB::bind_method(D_METHOD("set_overlay_z", "v"), &SpineSlotNode3D::set_overlay_z);
	ClassDB::bind_method(D_METHOD("get_overlay_z"), &SpineSlotNode3D::get_overlay_z);
	ClassDB::bind_method(D_METHOD("set_transform_inheritance_mode", "mode"), &SpineSlotNode3D::set_transform_inheritance_mode);
	ClassDB::bind_method(D_METHOD("get_transform_inheritance_mode"), &SpineSlotNode3D::get_transform_inheritance_mode);

	ADD_GROUP("Materials", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_normal_material", "get_normal_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "additive_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_additive_material", "get_additive_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiply_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_multiply_material", "get_multiply_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "screen_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_screen_material", "get_screen_material");
	ADD_GROUP("Offsets", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position_offset"), "set_position_offset", "get_position_offset");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "rotation_offset_degrees"), "set_rotation_offset_degrees", "get_rotation_offset_degrees");
	ADD_PROPERTY(PropertyInfo(VARIANT_FLOAT, "overlay_z", PROPERTY_HINT_RANGE, "-1.0,1.0,0.0001"), "set_overlay_z", "get_overlay_z");
	ADD_GROUP("Transform", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transform_inheritance_mode", PROPERTY_HINT_ENUM, "Full Bone,Translation and Rotation,Translation Only,Translation Rotation and Scale (2D Compatible)"), "set_transform_inheritance_mode", "get_transform_inheritance_mode");
}

SpineSlotNode3D::SpineSlotNode3D() :
		slot_index(-1),
		bound_sprite(nullptr),
		bound_sprite_id(ObjectID()),
		position_offset(Vector3(0, 0, 0)),
		rotation_offset_degrees(Vector3(0, 0, 0)),
		overlay_z(0.0f),
		transform_inheritance_mode(TRANSFORM_INHERITANCE_TRANSLATION_ROTATION_AND_SCALE) {
}

void SpineSlotNode3D::_notification(int what) {
	switch (what) {
		case NOTIFICATION_ENTER_TREE: {
			update_sprite_binding();
			SpineSprite3D *sprite = get_bound_sprite();
			if (sprite) {
				update_transform(sprite);
			}
			NOTIFY_PROPERTY_LIST_CHANGED();
			break;
		}
		case NOTIFICATION_PARENTED:
			NOTIFY_PROPERTY_LIST_CHANGED();
			break;
		case NOTIFICATION_EXIT_TREE:
		case NOTIFICATION_UNPARENTED:
		case NOTIFICATION_PREDELETE: {
			disconnect_bound_sprite();
			break;
		}
		case NOTIFICATION_CHILD_ORDER_CHANGED: {
			SpineSprite3D *sprite = get_bound_sprite();
			if (sprite && is_inside_tree()) {
				update_transform(sprite);
			}
			break;
		}

		default:
			break;
	}
}

SpineSprite3D *SpineSlotNode3D::get_bound_sprite() const {
	if (bound_sprite_id.is_null()) return nullptr;
	SpineSprite3D *sprite = Object::cast_to<SpineSprite3D>(ObjectDB::get_instance(bound_sprite_id));
	return sprite;
}

void SpineSlotNode3D::disconnect_bound_sprite() {
	SpineSprite3D *sprite = get_bound_sprite();
	if (!sprite) {
		bound_sprite = nullptr;
		bound_sprite_id = ObjectID();
		return;
	}
#if VERSION_MAJOR > 3
	Callable world_transforms_changed_callable(this, SNAME("_on_world_transforms_changed"));
	if (sprite->is_connected(SNAME("world_transforms_changed"), world_transforms_changed_callable)) {
		sprite->disconnect(SNAME("world_transforms_changed"), world_transforms_changed_callable);
	}
#else
	if (sprite->is_connected(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"))) {
		sprite->disconnect(SNAME("world_transforms_changed"), this, SNAME("_on_world_transforms_changed"));
	}
#endif
	bound_sprite = nullptr;
	bound_sprite_id = ObjectID();
}

void SpineSlotNode3D::update_sprite_binding() {
	SpineSprite3D *sprite = is_inside_tree() ? find_parent_sprite() : nullptr;
	SpineSprite3D *current_sprite = get_bound_sprite();
	if (current_sprite == sprite && bound_sprite == sprite) return;

	disconnect_bound_sprite();
	bound_sprite = sprite;
	bound_sprite_id = sprite ? ObjectID(sprite->get_instance_id()) : ObjectID();
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

SpineSprite3D *SpineSlotNode3D::find_parent_sprite() const {
	Node *parent = get_parent();
	while (parent) {
		SpineSprite3D *sprite = Object::cast_to<SpineSprite3D>(parent);
		if (sprite) return sprite;
		parent = parent->get_parent();
	}
	return nullptr;
}

void SpineSlotNode3D::_get_property_list(List<PropertyInfo> *list) const {
#ifdef SPINE_GODOT_EXTENSION
	PackedStringArray slot_names;
#else
	Vector<String> slot_names;
#endif

	SpineSprite3D *sprite = find_parent_sprite();
	if (sprite && sprite->get_skeleton_data_res().is_valid()) {
		sprite->get_skeleton_data_res()->get_slot_names(slot_names);
	} else {
		slot_names.push_back(slot_name);
	}

	PropertyInfo slot_name_property;
	slot_name_property.name = "slot_name";
	slot_name_property.type = Variant::STRING;
	slot_name_property.hint = PROPERTY_HINT_ENUM;
	slot_name_property.hint_string = String(",").join(slot_names);
	slot_name_property.usage = PROPERTY_USAGE_DEFAULT;
	list->push_back(slot_name_property);
}

bool SpineSlotNode3D::_get(const StringName &property, Variant &value) const {
	if (property == StringName("slot_name")) {
		value = slot_name;
		return true;
	}
	return false;
}

bool SpineSlotNode3D::_set(const StringName &property, const Variant &value) {
	if (property == StringName("slot_name")) {
		set_slot_name(value);
		return true;
	}
	return false;
}

void SpineSlotNode3D::on_world_transforms_changed(const Variant &spine_sprite) {
	if (!is_inside_tree()) {
		return;
	}
	SpineSprite3D *sprite = cast_to<SpineSprite3D>(spine_sprite.operator Object *());
	if (sprite != get_bound_sprite()) {
		update_sprite_binding();
		sprite = get_bound_sprite();
	}
	update_transform(sprite);
}

void SpineSlotNode3D::update_transform(SpineSprite3D *sprite) {
	if (!is_inside_tree()) return;
	if (!sprite) return;

	Ref<SpineSkeleton> skeleton_ref = sprite->get_skeleton();
	if (!skeleton_ref.is_valid()) return;

	spine::Skeleton *skeleton_object = skeleton_ref->get_spine_object();
	if (!skeleton_object) return;

	auto slot_ref = skeleton_ref->find_slot(slot_name);
	if (!slot_ref.is_valid()) {
		slot_index = -1;
		return;
	}

	slot_index = slot_ref->get_data()->get_index();
	if (slot_index < 0 || slot_index >= (int)skeleton_object->getSlots().size()) {
		slot_index = -1;
		return;
	}

	spine::Slot *raw_slot = skeleton_object->getSlots()[slot_index];
	if (!raw_slot) return;
	spine::Bone *raw_bone = &raw_slot->getBone();
	if (!raw_bone) return;

	int draw_order_index = 0;
#ifdef SPINE_RUNTIME_43
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder().getAppliedPose();
#else
	spine::Vector<spine::Slot *> &draw_order = skeleton_object->getDrawOrder();
#endif
	for (int i = 0, n = (int)draw_order.size(); i < n; ++i) {
		if (draw_order[i] == raw_slot) {
			draw_order_index = i;
			break;
		}
	}
	const float ppu = MAX(0.0001f, sprite->get_pixels_per_unit());
	const int slot_count = (int)draw_order.size();
	const Vector3 insert_offset = sprite->get_slot_insert_offset(draw_order_index, (int)draw_order.size());
	const int insert_render_priority = sprite->get_slot_insert_render_priority(draw_order_index, slot_count);
#ifdef SPINE_RUNTIME_43
	const float world_x = raw_bone->getAppliedPose().getWorldX();
	const float world_y = raw_bone->getAppliedPose().getWorldY();
	const float a = raw_bone->getAppliedPose().getA();
	const float b = raw_bone->getAppliedPose().getB();
	const float c = raw_bone->getAppliedPose().getC();
	const float d = raw_bone->getAppliedPose().getD();
#else
	const float world_x = raw_bone->getWorldX();
	const float world_y = raw_bone->getWorldY();
	const float a = raw_bone->getA();
	const float b = raw_bone->getB();
	const float c = raw_bone->getC();
	const float d = raw_bone->getD();
#endif

	Basis bone_basis;
	switch (transform_inheritance_mode) {
		case TRANSFORM_INHERITANCE_FULL_BONE: {
			bone_basis = Basis(
					Vector3(a, -c, 0.0f),
					Vector3(b, -d, 0.0f),
					Vector3(0.0f, 0.0f, 1.0f));
			break;
		}
		case TRANSFORM_INHERITANCE_TRANSLATION_ROTATION_AND_SCALE: {
			Vector3 x_axis(a, -c, 0.0f);
			if (x_axis.length_squared() < CMP_EPSILON2) {
				x_axis = Vector3(1.0f, 0.0f, 0.0f);
			} else {
				x_axis.normalize();
			}
			Vector3 y_axis(-x_axis.y, x_axis.x, 0.0f);
#ifdef SPINE_RUNTIME_43
			const float scale_x = raw_bone->getAppliedPose().getWorldScaleX();
			const float scale_y = raw_bone->getAppliedPose().getWorldScaleY();
#else
			const float scale_x = raw_bone->getWorldScaleX();
			const float scale_y = raw_bone->getWorldScaleY();
#endif
			bone_basis = Basis(
					x_axis * scale_x,
					y_axis * scale_y,
					Vector3(0.0f, 0.0f, 1.0f));
			break;
		}
		case TRANSFORM_INHERITANCE_TRANSLATION_ONLY: {
			bone_basis = Basis();
			break;
		}
		case TRANSFORM_INHERITANCE_TRANSLATION_AND_ROTATION:
		default: {
			Vector3 x_axis(a, -c, 0.0f);
			if (x_axis.length_squared() < CMP_EPSILON2) {
				x_axis = Vector3(1.0f, 0.0f, 0.0f);
			} else {
				x_axis.normalize();
			}
			const float angle = Math::atan2(x_axis.y, x_axis.x);
			bone_basis = Basis(Vector3(0.0f, 0.0f, 1.0f), angle);
			break;
		}
	}

	Basis rx(Vector3(1, 0, 0), Math::deg_to_rad(rotation_offset_degrees.x));
	Basis ry(Vector3(0, 1, 0), Math::deg_to_rad(rotation_offset_degrees.y));
	Basis rz(Vector3(0, 0, 1), Math::deg_to_rad(rotation_offset_degrees.z));
	Basis offset_basis = rz * ry * rx;

	Transform3D anchor_transform(
			bone_basis,
			Vector3(world_x / ppu, -world_y / ppu, 0.0f) + insert_offset);
	Transform3D offset_transform(offset_basis, position_offset);

	Transform3D final_transform = anchor_transform * offset_transform;
	final_transform.origin.z += overlay_z;

	set_transform(final_transform);

	const int child_count = get_child_count();
	for (int i = 0; i < child_count; ++i) {
		Node *child = Object::cast_to<Node>(get_child(i));
		apply_slot_insert_render_priority_to_subtree(child, insert_render_priority);
	}
}

void SpineSlotNode3D::set_slot_name(const String &_slot_name) {
	slot_name = _slot_name;
	SpineSprite3D *sprite = get_bound_sprite();
	update_transform(sprite ? sprite : find_parent_sprite());
}

String SpineSlotNode3D::get_slot_name() const {
	return slot_name;
}

Ref<Material> SpineSlotNode3D::get_normal_material() {
	return normal_material;
}

void SpineSlotNode3D::set_normal_material(Ref<Material> material) {
	normal_material = material;
}

Ref<Material> SpineSlotNode3D::get_additive_material() {
	return additive_material;
}

void SpineSlotNode3D::set_additive_material(Ref<Material> material) {
	additive_material = material;
}

Ref<Material> SpineSlotNode3D::get_multiply_material() {
	return multiply_material;
}

void SpineSlotNode3D::set_multiply_material(Ref<Material> material) {
	multiply_material = material;
}

Ref<Material> SpineSlotNode3D::get_screen_material() {
	return screen_material;
}

void SpineSlotNode3D::set_screen_material(Ref<Material> material) {
	screen_material = material;
}

void SpineSlotNode3D::set_position_offset(const Vector3 &v) {
	position_offset = v;
	SpineSprite3D *sprite = get_bound_sprite();
	update_transform(sprite ? sprite : find_parent_sprite());
}

Vector3 SpineSlotNode3D::get_position_offset() const {
	return position_offset;
}

void SpineSlotNode3D::set_rotation_offset_degrees(const Vector3 &v) {
	rotation_offset_degrees = v;
	SpineSprite3D *sprite = get_bound_sprite();
	update_transform(sprite ? sprite : find_parent_sprite());
}

Vector3 SpineSlotNode3D::get_rotation_offset_degrees() const {
	return rotation_offset_degrees;
}

void SpineSlotNode3D::set_overlay_z(float v) {
	overlay_z = v;
	SpineSprite3D *sprite = get_bound_sprite();
	update_transform(sprite ? sprite : find_parent_sprite());
}

float SpineSlotNode3D::get_overlay_z() const {
	return overlay_z;
}

void SpineSlotNode3D::set_transform_inheritance_mode(int mode) {
	const TransformInheritanceMode clamped_mode = (TransformInheritanceMode)CLAMP(mode, (int)TRANSFORM_INHERITANCE_FULL_BONE, (int)TRANSFORM_INHERITANCE_TRANSLATION_ROTATION_AND_SCALE);
	if (transform_inheritance_mode == clamped_mode) return;
	transform_inheritance_mode = clamped_mode;
	SpineSprite3D *sprite = get_bound_sprite();
	update_transform(sprite ? sprite : find_parent_sprite());
}

int SpineSlotNode3D::get_transform_inheritance_mode() const {
	return (int)transform_inheritance_mode;
}
