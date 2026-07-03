extends SpineSprite3D

func _ready():
	await get_tree().process_frame

	var data = get_skeleton().get_data()
	var custom_skin = new_skin("custom-skin")
	var skin_base = data.find_skin("skin-base")
	custom_skin.add_skin(skin_base)
	custom_skin.add_skin(data.find_skin("nose/short"))
	custom_skin.add_skin(data.find_skin("eyelids/girly"))
	custom_skin.add_skin(data.find_skin("eyes/violet"))
	custom_skin.add_skin(data.find_skin("hair/brown"))
	custom_skin.add_skin(data.find_skin("clothes/hoodie-orange"))
	custom_skin.add_skin(data.find_skin("legs/pants-jeans"))
	custom_skin.add_skin(data.find_skin("accessories/bag"))
	custom_skin.add_skin(data.find_skin("accessories/hat-red-yellow"))
	get_skeleton().set_skin(custom_skin)
	get_skeleton().set_slots_to_setup_pose()

	for el in custom_skin.get_attachments():
		var entry: SpineSkinEntry = el
		print(str(entry.get_slot_index()) + " " + entry.get_name())

	get_animation_state().set_animation("dance", true, 0)
