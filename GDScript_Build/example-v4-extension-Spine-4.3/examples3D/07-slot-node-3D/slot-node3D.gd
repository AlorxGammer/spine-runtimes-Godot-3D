extends Node3D

@onready var spineboy: SpineSprite3D = $Spineboy
@onready var raptor: SpineSprite3D = $Spineboy/GunSlot/Raptor
@onready var tiny_spineboy: SpineSprite3D = $Spineboy/FrontFistSlot/TinySpineboy

func _ready():
	call_deferred("_start_animations")

func _start_animations():
	if not is_inside_tree():
		return
	var entry = spineboy.set_animation("run", true, 0)
	if entry:
		entry.set_time_scale(0.1)
	raptor.set_animation("walk", true, 0)
	tiny_spineboy.set_animation("walk", true, 0)
