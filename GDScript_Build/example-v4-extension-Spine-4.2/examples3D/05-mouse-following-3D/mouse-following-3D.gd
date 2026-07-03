extends Node3D

@onready var spineboy: SpineSprite3D = $Spineboy
@onready var crosshair_bone: SpineBoneNode3D = $Spineboy/CrosshairBone
@onready var camera: Camera3D = $Camera3D

func _ready():
	camera.current = true
	spineboy.get_animation_state().set_animation("walk", true, 0)
	spineboy.get_animation_state().set_animation("aim", true, 1)

func _process(_delta):
	if camera == null:
		return

	var hit = screen_to_spine_plane(camera, get_viewport().get_mouse_position())
	if hit == null:
		return

	crosshair_bone.global_position = hit

func screen_to_spine_plane(view_camera: Camera3D, screen_pos: Vector2):
	var ray_origin = view_camera.project_ray_origin(screen_pos)
	var ray_dir = view_camera.project_ray_normal(screen_pos)
	var plane_normal: Vector3 = spineboy.global_transform.basis * Vector3(0, 0, 1)
	plane_normal = plane_normal.normalized()

	var plane := Plane(plane_normal, spineboy.global_transform.origin)
	return plane.intersects_ray(ray_origin, ray_dir)
