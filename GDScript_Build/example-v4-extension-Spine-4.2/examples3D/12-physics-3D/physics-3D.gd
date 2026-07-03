extends Node3D

@onready var celestial_circus: SpineSprite3D = $"celestial-circus"
@onready var camera: Camera3D = get_viewport().get_camera_3d()

var is_mouse_over := false
var is_dragging := false

# Смещение между точкой клика и позицией объекта
var drag_offset := Vector3.ZERO

# Куда нужно поставить объект
var target_position := Vector3.ZERO

# Последняя экранная дельта мыши для Spine physics
var mouse_relative := Vector2.ZERO

# Насколько сильно screen delta влияет на Spine physics
@export var spine_physics_strength := 1.0


func _ready():
	await get_tree().process_frame

	if camera == null:
		push_error("Camera3D not found in viewport.")
		return

	var state = celestial_circus.get_animation_state()
	if state == null:
		push_error("Spine animation state is null.")
		return

	state.set_animation("wind-idle", true, 0)
	state.set_animation("eyeblink-long", true, 1)
	state.set_animation("stars", true, 2)

	target_position = celestial_circus.global_position


func _input(event):
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			if is_mouse_over:
				is_dragging = true

				var mouse_world = screen_to_world_on_object_plane(
					event.position,
					celestial_circus.global_position.z
				)

				drag_offset = celestial_circus.global_position - mouse_world
				target_position = celestial_circus.global_position
				mouse_relative = Vector2.ZERO
		else:
			is_dragging = false
			mouse_relative = Vector2.ZERO

	elif event is InputEventMouseMotion and is_dragging:
		mouse_relative = event.relative

		var mouse_world = screen_to_world_on_object_plane(
			event.position,
			celestial_circus.global_position.z
		)

		target_position = mouse_world + drag_offset


func _process(_delta):
	if not is_dragging:
		return

	# Жёсткая моментальная привязка к курсору
	celestial_circus.global_position = target_position

	# Spine physics — как в старом коде, от движения мыши
	apply_spine_physics_from_mouse_delta(mouse_relative)

	# Сбрасываем, чтобы не дёргалось повторно без нового motion event
	mouse_relative = Vector2.ZERO


func screen_to_world_on_object_plane(screen_pos: Vector2, z_plane: float) -> Vector3:
	if camera == null:
		return celestial_circus.global_position

	var ray_origin = camera.project_ray_origin(screen_pos)
	var ray_direction = camera.project_ray_normal(screen_pos)

	if absf(ray_direction.z) < 0.0001:
		return celestial_circus.global_position

	var t = (z_plane - ray_origin.z) / ray_direction.z
	return ray_origin + ray_direction * t


func apply_spine_physics_from_mouse_delta(relative: Vector2):
	if relative == Vector2.ZERO:
		return

	var skeleton = celestial_circus.get_skeleton()
	if skeleton == null:
		return

	var sx = celestial_circus.scale.x
	var sy = celestial_circus.scale.y

	if absf(sx) < 0.0001:
		sx = 1.0
	if absf(sy) < 0.0001:
		sy = 1.0

	# Тут ориентация как в твоём старом коде:
	# dx идёт как есть, dy тоже как есть.
	# Если вдруг по ощущению вертикаль "не та", можно поставить минус перед relative.y
	var dx = relative.x / sx
	var dy = relative.y / sy

	skeleton.physics_translate(
		dx * spine_physics_strength,
		dy * spine_physics_strength
	)


func _on_area_3d_mouse_entered():
	is_mouse_over = true


func _on_area_3d_mouse_exited():
	is_mouse_over = false
