/****************************************************************************
 * Spine Runtimes License Agreement
 * Last updated April 5, 2025. Replaces all prior versions.
 *
 * Copyright (c) 2013-2025, Esoteric Software LLC
 *****************************************************************************/

#pragma once

#include "SpineCommon.h"
#include "SpineSkeleton.h"
#include "SpineSprite3D.h"

#ifdef SPINE_GODOT_EXTENSION
#include <godot_cpp/classes/node3d.hpp>
#else
#include "scene/3d/node_3d.h"
#endif

class SpineBoneNode3D : public Node3D {
	GDCLASS(SpineBoneNode3D, Node3D)

protected:
	String bone_name;
	SpineConstant::BoneMode bone_mode;
	bool enabled;
	SpineSprite3D *bound_sprite;
	bool applying_bone_transform;

	static void _bind_methods();
	void _notification(int what);
	void _get_property_list(List<PropertyInfo> *list) const;
	bool _get(const StringName &property, Variant &value) const;
	bool _set(const StringName &property, const Variant &value);

	void update_sprite_binding();
	void on_world_transforms_changed(const Variant &spine_sprite);
	void update_transform(SpineSprite3D *sprite);
	void init_transform(SpineSprite3D *sprite);
	void drive_bone_from_node(SpineSprite3D *sprite);
	void follow_bone_with_node(SpineSprite3D *sprite);

	static Transform3D bone_transform_to_3d(const Transform2D &bone_transform, float pixels_per_unit);
	static Transform2D transform_3d_to_bone(const Transform3D &transform, float pixels_per_unit);

public:
	static String get_class_icon_path() { return "res://spine_godot/icons/SpineBoneNode3D.svg"; }

	SpineBoneNode3D();

	SpineConstant::BoneMode get_bone_mode();
	void set_bone_mode(SpineConstant::BoneMode bone_mode);

	void set_enabled(bool _enabled);
	bool get_enabled();

	SpineSprite3D *find_parent_sprite() const;
	Ref<SpineBone> find_bone() const;
};
