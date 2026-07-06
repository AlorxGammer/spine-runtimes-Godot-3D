extends Node3D

func _ready():
	# Load the skeleton file
	var skeleton_path = ProjectSettings.globalize_path(
		"res://assets/Coin/coin-pro.skel"
	)
	var atlas_path = ProjectSettings.globalize_path(
		"res://assets/Coin/coin.atlas"
	)

	var skeleton_file_res = SpineSkeletonFileResource.new()
	skeleton_file_res.load_from_file(skeleton_path)

	# Load the atlas file
	var atlas_res = SpineAtlasResource.new()
	atlas_res.load_from_atlas_file(atlas_path)

	# Create shared skeleton data
	var skeleton_data_res = SpineSkeletonDataResource.new()
	skeleton_data_res.skeleton_file_res = skeleton_file_res
	skeleton_data_res.atlas_res = atlas_res

	# Create the 3D Spine sprite
	var sprite = SpineSprite3D.new()

	# Keep Spine's native generated materials and slot blend modes.
	# Do not override normal/additive/multiply/screen materials here.
	sprite.lighting_enabled = false
	sprite.generated_normal_map_enabled = false
	sprite.visible_alpha_cutoff = 0.0
	sprite.skeleton_data_res = skeleton_data_res

	# Add it before applying global transforms.
	add_child(sprite)

	# Put it in front of the active camera.
	var cam := get_viewport().get_camera_3d()
	var distance := 2.0

	if cam:
		sprite.global_transform = Transform3D(
			cam.global_transform.basis,
			cam.global_position - cam.global_transform.basis.z * distance
		)
	else:
		sprite.position = Vector3(0.0, 0.0, -distance)

	# Start the Spine animation.
	sprite.set_animation("animation", true, 0)
