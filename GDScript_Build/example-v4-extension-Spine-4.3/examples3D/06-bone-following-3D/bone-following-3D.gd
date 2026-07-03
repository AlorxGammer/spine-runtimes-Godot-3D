extends Node3D

func _ready():
	var spineboy: SpineSprite3D = $Spineboy
	spineboy.get_animation_state().set_animation("walk", true, 0)	
