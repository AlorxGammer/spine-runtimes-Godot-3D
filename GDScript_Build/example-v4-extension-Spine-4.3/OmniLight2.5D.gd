@tool
class_name OmniLight25D
extends Node3D

# Hybrid light rig for flat 3D sprites such as SpineSprite3D.
# Real OmniLight3D keeps volume, while glow quads add the softer 2D-style punch.

enum Preset {
	SOFT_CANVAS,
	BALANCED_25D,
	RIM_PUNCH,
}

const DEFAULT_GLOW_TEXTURE := preload("res://assets/raptor/light-sprite.png")
const KEY_LIGHT_NAME := "__key_light_25d"
const FILL_LIGHT_NAME := "__fill_light_25d"
const RIM_LIGHT_NAME := "__rim_light_25d"
const CORE_GLOW_NAME := "__core_glow_25d"
const HALO_GLOW_NAME := "__halo_glow_25d"

@export var preset: Preset = Preset.BALANCED_25D

@export_group("Follow")
@export var follow_target := false
@export var follow_target_path: NodePath
@export var follow_offset := Vector3(0.0, 0.45, 0.0)
@export var inherit_target_rotation := false

@export_group("Colors")
@export var key_color := Color(1.0, 0.15, 0.08, 1.0)
@export var fill_color := Color(0.58, 0.16, 0.11, 1.0)
@export var rim_color := Color(1.0, 0.38, 0.18, 1.0)

@export_group("Key Light")
@export var use_node_position_as_key_source := true
@export_range(0.0, 8.0, 0.01) var key_energy := 2.35
@export_range(0.5, 16.0, 0.05) var key_range := 4.2
@export var key_offset := Vector3.ZERO
@export var key_shadows := true

@export_group("Fill Light")
@export_range(0.0, 8.0, 0.01) var fill_energy := 0.7
@export_range(0.5, 16.0, 0.05) var fill_range := 5.4
@export var fill_offset := Vector3(-0.85, 0.18, 0.55)

@export_group("Rim Light")
@export_range(0.0, 8.0, 0.01) var rim_energy := 1.0
@export_range(0.5, 16.0, 0.05) var rim_range := 4.8
@export var rim_offset := Vector3(-0.2, 0.52, -0.72)

@export_group("Glow")
@export var enable_sprite_glow := false
@export var glow_texture: Texture2D = DEFAULT_GLOW_TEXTURE
@export var glow_offset := Vector3(0.16, 0.92, 0.02)
@export_range(0.0, 1.0, 0.01) var glow_key_mix := 0.65
@export var core_glow_size := Vector2(0.95, 0.95)
@export var halo_glow_size := Vector2(2.0, 2.0)
@export_range(0.0, 1.0, 0.01) var core_glow_alpha := 0.26
@export_range(0.0, 1.0, 0.01) var halo_glow_alpha := 0.08

@export_group("Motion")
@export_range(0.0, 0.5, 0.01) var flicker_strength := 0.03
@export_range(0.0, 20.0, 0.1) var flicker_speed := 2.6
@export var animate_in_editor := false

var _last_preset := -1
var _key_light: OmniLight3D
var _fill_light: OmniLight3D
var _rim_light: OmniLight3D
var _core_glow: MeshInstance3D
var _halo_glow: MeshInstance3D
var _core_glow_material: StandardMaterial3D
var _halo_glow_material: StandardMaterial3D


func _ready() -> void:
	_ensure_nodes()
	_apply_preset_if_needed(true)
	_sync_follow_target()
	_sync_rig()
	set_process(true)


func _process(_delta: float) -> void:
	if Engine.is_editor_hint() and not animate_in_editor:
		_apply_preset_if_needed()
		_sync_follow_target()
		_sync_rig(1.0)
		return

	_apply_preset_if_needed()
	_sync_follow_target()
	_sync_rig(_get_flicker_multiplier())


func _apply_preset_if_needed(force := false) -> void:
	if not force and _last_preset == int(preset):
		return

	_last_preset = int(preset)

	match preset:
		Preset.SOFT_CANVAS:
			key_color = Color(1.0, 0.23, 0.14, 1.0)
			fill_color = Color(0.65, 0.2, 0.14, 1.0)
			rim_color = Color(1.0, 0.48, 0.24, 1.0)
			key_energy = 1.3
			key_range = 3.4
			key_offset = Vector3.ZERO
			key_shadows = false
			fill_energy = 0.24
			fill_range = 4.2
			fill_offset = Vector3(-0.2, 0.16, 0.22)
			rim_energy = 0.22
			rim_range = 3.6
			rim_offset = Vector3(0.1, 0.12, -0.14)
			enable_sprite_glow = false
			glow_offset = Vector3(0.14, 0.94, 0.02)
			core_glow_size = Vector2(1.1, 1.1)
			halo_glow_size = Vector2(2.4, 2.4)
			core_glow_alpha = 0.24
			halo_glow_alpha = 0.1
			flicker_strength = 0.025
			flicker_speed = 2.1

		Preset.BALANCED_25D:
			key_color = Color(1.0, 0.15, 0.08, 1.0)
			fill_color = Color(0.58, 0.16, 0.11, 1.0)
			rim_color = Color(1.0, 0.38, 0.18, 1.0)
			key_energy = 1.45
			key_range = 3.5
			key_offset = Vector3.ZERO
			key_shadows = false
			fill_energy = 0.18
			fill_range = 4.2
			fill_offset = Vector3(-0.24, 0.18, 0.24)
			rim_energy = 0.34
			rim_range = 4.0
			rim_offset = Vector3(0.12, 0.14, -0.18)
			enable_sprite_glow = false
			glow_offset = Vector3(0.16, 0.96, 0.02)
			core_glow_size = Vector2(0.95, 0.95)
			halo_glow_size = Vector2(2.0, 2.0)
			core_glow_alpha = 0.26
			halo_glow_alpha = 0.08
			flicker_strength = 0.03
			flicker_speed = 2.6

		Preset.RIM_PUNCH:
			key_color = Color(1.0, 0.12, 0.06, 1.0)
			fill_color = Color(0.42, 0.1, 0.09, 1.0)
			rim_color = Color(1.0, 0.55, 0.24, 1.0)
			key_energy = 1.72
			key_range = 3.3
			key_offset = Vector3.ZERO
			key_shadows = false
			fill_energy = 0.08
			fill_range = 4.6
			fill_offset = Vector3(-0.28, 0.14, 0.2)
			rim_energy = 0.56
			rim_range = 4.2
			rim_offset = Vector3(0.18, 0.12, -0.24)
			enable_sprite_glow = false
			glow_offset = Vector3(0.22, 1.02, 0.03)
			core_glow_size = Vector2(0.82, 0.82)
			halo_glow_size = Vector2(1.7, 1.7)
			core_glow_alpha = 0.22
			halo_glow_alpha = 0.06
			flicker_strength = 0.04
			flicker_speed = 3.2


func _sync_follow_target() -> void:
	if not follow_target:
		return

	var target: Node3D = _get_target_node()
	if target == null:
		return

	if inherit_target_rotation:
		global_transform = Transform3D(target.global_basis, target.to_global(follow_offset))
	else:
		global_position = target.global_position + follow_offset


func _sync_rig(flicker_multiplier := 1.0) -> void:
	_ensure_nodes()
	var key_position: Vector3 = Vector3.ZERO if use_node_position_as_key_source else key_offset

	_key_light.position = key_position
	_key_light.light_color = key_color
	_key_light.light_energy = key_energy * flicker_multiplier
	_key_light.omni_range = key_range
	_key_light.shadow_enabled = key_shadows

	_fill_light.position = fill_offset
	_fill_light.light_color = fill_color
	_fill_light.light_energy = fill_energy * lerpf(1.0, flicker_multiplier, 0.35)
	_fill_light.omni_range = fill_range
	_fill_light.shadow_enabled = false

	_rim_light.position = rim_offset
	_rim_light.light_color = rim_color
	_rim_light.light_energy = rim_energy * lerpf(1.0, flicker_multiplier, 0.45)
	_rim_light.omni_range = rim_range
	_rim_light.shadow_enabled = false

	if enable_sprite_glow:
		var glow_pulse: float = lerpf(1.0, flicker_multiplier, 0.45)
		var core_color: Color = key_color.lerp(Color.WHITE, 0.18)
		var halo_color: Color = fill_color.lerp(key_color, 0.45)
		var actual_glow_offset: Vector3 = _get_glow_offset()

		_sync_glow_mesh(
			_core_glow,
			_core_glow_material,
			actual_glow_offset,
			core_glow_size,
			Color(core_color.r, core_color.g, core_color.b, clampf(core_glow_alpha * glow_pulse, 0.0, 1.0))
		)
		_sync_glow_mesh(
			_halo_glow,
			_halo_glow_material,
			actual_glow_offset,
			halo_glow_size,
			Color(halo_color.r, halo_color.g, halo_color.b, clampf(halo_glow_alpha * glow_pulse, 0.0, 1.0))
		)
		_core_glow.visible = true
		_halo_glow.visible = true
	else:
		_core_glow.visible = false
		_halo_glow.visible = false


func _ensure_nodes() -> void:
	_key_light = _ensure_light(KEY_LIGHT_NAME)
	_fill_light = _ensure_light(FILL_LIGHT_NAME)
	_rim_light = _ensure_light(RIM_LIGHT_NAME)

	_core_glow = _ensure_glow_mesh(CORE_GLOW_NAME)
	_halo_glow = _ensure_glow_mesh(HALO_GLOW_NAME)

	_core_glow_material = _ensure_glow_material(_core_glow, _core_glow_material)
	_halo_glow_material = _ensure_glow_material(_halo_glow, _halo_glow_material)


func _ensure_light(node_name: String) -> OmniLight3D:
	var light := get_node_or_null(node_name) as OmniLight3D
	if light != null:
		return light

	light = OmniLight3D.new()
	light.name = node_name
	add_child(light)
	return light


func _ensure_glow_mesh(node_name: String) -> MeshInstance3D:
	var glow := get_node_or_null(node_name) as MeshInstance3D
	if glow != null:
		return glow

	glow = MeshInstance3D.new()
	glow.name = node_name
	glow.mesh = QuadMesh.new()
	add_child(glow)
	return glow


func _ensure_glow_material(glow: MeshInstance3D, material: StandardMaterial3D) -> StandardMaterial3D:
	if material == null:
		material = StandardMaterial3D.new()
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		material.no_depth_test = false

	glow.material_override = material
	return material


func _sync_glow_mesh(
	glow: MeshInstance3D,
	material: StandardMaterial3D,
	offset: Vector3,
	size: Vector2,
	color: Color
) -> void:
	var quad: QuadMesh = glow.mesh as QuadMesh
	if quad == null:
		quad = QuadMesh.new()
		glow.mesh = quad

	quad.size = size
	glow.position = offset

	material.albedo_texture = glow_texture if glow_texture != null else DEFAULT_GLOW_TEXTURE
	material.albedo_color = color


func _get_target_node() -> Node3D:
	var target: Node3D = get_node_or_null(follow_target_path) as Node3D
	if target != null:
		return target

	return get_parent() as Node3D


func _get_glow_offset() -> Vector3:
	var source_offset: Vector3 = Vector3.ZERO if use_node_position_as_key_source else key_offset
	var tracked_x: float = lerpf(glow_offset.x, source_offset.x * 0.72, glow_key_mix)
	var tracked_y: float = lerpf(glow_offset.y, maxf(glow_offset.y, source_offset.y * 0.82), glow_key_mix)
	var tracked_z: float = lerpf(glow_offset.z, minf(source_offset.z, 0.08), glow_key_mix)
	return Vector3(tracked_x, tracked_y, tracked_z)


func _get_flicker_multiplier() -> float:
	if flicker_strength <= 0.0 or flicker_speed <= 0.0:
		return 1.0

	var t: float = Time.get_ticks_msec() * 0.001 * flicker_speed
	var wave: float = sin(t * 1.13)
	wave += sin(t * 2.27 + 0.7) * 0.55
	wave += sin(t * 4.91 + 1.9) * 0.22
	return 1.0 + wave * flicker_strength * 0.5
