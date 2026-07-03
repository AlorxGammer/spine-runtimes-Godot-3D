extends Node3D

@export var move_speed := 1.5
@export var hover_height := 0.3
@export var ground_probe_height := 3.0
@export var ground_probe_depth := 8.0
@export var ground_follow_speed := 8.0
@export var hip_follow_slack := 0.2
@export var loop_motion := true
@export var use_initial_x_as_start := true
@export var reset_x := 5.35
@export var start_x := -5.0

@onready var spineboy: SpineSprite3D = $Spineboy
@onready var center_bone: SpineBoneNode3D = $Spineboy/HoverboardCenterBone
@onready var center_ray: RayCast3D = $Spineboy/HoverboardCenterBone/CenterRay
@onready var target_bone: SpineBoneNode3D = $Spineboy/HoverboardTargetBone
@onready var target_ray: RayCast3D = $Spineboy/HoverboardTargetBone/TargetRay
@onready var hip_bone: SpineBoneNode3D = $Spineboy/HipBone

const CENTER_BONE_NAME := "hoverboard-controller"
const TARGET_BONE_NAME := "board-ik"
const HIP_BONE_NAME := "hip"

var center_hip_distance := 0.0

func _ready() -> void:
	center_bone.enabled = false
	target_bone.enabled = false
	hip_bone.enabled = false
	_setup_world_probe(center_ray)
	_setup_world_probe(target_ray)
	if use_initial_x_as_start:
		start_x = spineboy.global_position.x
	spineboy.set_animation("hoverboard", true, 0)
	spineboy.update_skeleton(0.0)
	center_hip_distance = hip_bone.global_position.y - center_bone.global_position.y

func _physics_process(delta: float) -> void:
	_hover_bone_to_ground(target_bone, target_ray, TARGET_BONE_NAME, delta)
	_hover_bone_to_ground(center_bone, center_ray, CENTER_BONE_NAME, delta)
	_stabilize_hip()
	_move_spineboy(delta)

func _setup_world_probe(ray: RayCast3D) -> void:
	ray.top_level = true
	ray.enabled = true
	ray.target_position = Vector3(0.0, -ground_probe_height - ground_probe_depth, 0.0)

func _hover_bone_to_ground(bone: SpineBoneNode3D, ray: RayCast3D, bone_name: String, delta: float) -> void:
	var probe_origin := bone.global_position
	probe_origin.y += ground_probe_height
	ray.global_transform = Transform3D(Basis(), probe_origin)
	ray.force_raycast_update()
	if not ray.is_colliding():
		return

	var bone_position := bone.global_position
	var target_y := ray.get_collision_point().y + hover_height
	bone_position.y = move_toward(bone_position.y, target_y, ground_follow_speed * delta)
	bone.global_position = bone_position
	_drive_spine_bone_from_marker(bone_name, bone)

func _stabilize_hip() -> void:
	var current_distance := hip_bone.global_position.y - center_bone.global_position.y
	if abs(current_distance) - abs(center_hip_distance) >= hip_follow_slack:
		return

	var hip_position := hip_bone.global_position
	hip_position.y = center_bone.global_position.y + center_hip_distance
	hip_bone.global_position = hip_position
	_drive_spine_bone_from_marker(HIP_BONE_NAME, hip_bone)

func _move_spineboy(delta: float) -> void:
	var spineboy_position := spineboy.global_position
	spineboy_position.x += delta * move_speed
	if loop_motion and spineboy_position.x > reset_x:
		spineboy_position.x = start_x
	spineboy.global_position = spineboy_position

func _drive_spine_bone_from_marker(bone_name: String, marker: Node3D) -> void:
	var marker_in_spineboy_space := spineboy.global_transform.affine_inverse() * marker.global_transform
	spineboy.set_global_bone_transform(bone_name, _transform_3d_to_spine_2d(marker_in_spineboy_space))

func _transform_3d_to_spine_2d(source_transform: Transform3D) -> Transform2D:
	var ppu := spineboy.get_pixels_per_unit()
	var x_axis_3d := source_transform.basis.x
	var y_axis_3d := source_transform.basis.y
	var x_axis := Vector2(x_axis_3d.x, -x_axis_3d.y)
	var y_axis := Vector2(y_axis_3d.x, -y_axis_3d.y)
	var origin := Vector2(source_transform.origin.x * ppu, -source_transform.origin.y * ppu)
	var bone_transform := Transform2D(atan2(x_axis.y, x_axis.x), origin)
	bone_transform.x *= x_axis.length()
	bone_transform.y *= y_axis.length()
	return bone_transform
