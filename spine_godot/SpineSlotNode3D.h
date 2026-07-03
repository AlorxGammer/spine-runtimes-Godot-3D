/****************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *****************************************************************************/

#pragma once

#include "SpineCommon.h"

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/object.hpp>
#else
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#endif

class SpineSprite3D;

class SpineSlotNode3D : public Node3D {
	GDCLASS(SpineSlotNode3D, Node3D)

public:
	enum TransformInheritanceMode {
		TRANSFORM_INHERITANCE_FULL_BONE = 0,
		TRANSFORM_INHERITANCE_TRANSLATION_AND_ROTATION = 1,
		TRANSFORM_INHERITANCE_TRANSLATION_ONLY = 2,
		TRANSFORM_INHERITANCE_TRANSLATION_ROTATION_AND_SCALE = 3,
	};

protected:
	static void _bind_methods();
	void _notification(int what);
	void _get_property_list(List<PropertyInfo> *list) const;
	bool _get(const StringName &property, Variant &value) const;
	bool _set(const StringName &property, const Variant &value);

private:
	String slot_name;
	int slot_index;
	SpineSprite3D *bound_sprite;
	ObjectID bound_sprite_id;

	Ref<Material> normal_material;
	Ref<Material> additive_material;
	Ref<Material> multiply_material;
	Ref<Material> screen_material;
	Vector3 position_offset;
	Vector3 rotation_offset_degrees;
	float overlay_z;
	TransformInheritanceMode transform_inheritance_mode;

	void update_sprite_binding();
	void disconnect_bound_sprite();
	SpineSprite3D *get_bound_sprite() const;
	SpineSprite3D *find_parent_sprite() const;

public:
	SpineSlotNode3D();

	void on_world_transforms_changed(const Variant &spine_sprite);
	void update_transform(SpineSprite3D *sprite);

	void set_slot_name(const String &_slot_name);
	String get_slot_name() const;
	int get_slot_index() const { return slot_index; }

	Ref<Material> get_normal_material();
	void set_normal_material(Ref<Material> material);

	Ref<Material> get_additive_material();
	void set_additive_material(Ref<Material> material);

	Ref<Material> get_multiply_material();
	void set_multiply_material(Ref<Material> material);

	Ref<Material> get_screen_material();
	void set_screen_material(Ref<Material> material);

	void set_position_offset(const Vector3 &v);
	Vector3 get_position_offset() const;

	void set_rotation_offset_degrees(const Vector3 &v);
	Vector3 get_rotation_offset_degrees() const;

	void set_overlay_z(float v);
	float get_overlay_z() const;

	void set_transform_inheritance_mode(int mode);
	int get_transform_inheritance_mode() const;
};
