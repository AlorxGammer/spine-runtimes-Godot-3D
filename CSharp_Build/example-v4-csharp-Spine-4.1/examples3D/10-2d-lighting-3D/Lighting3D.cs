using Godot;

public partial class Lighting3D : Node3D
{
	public override void _Ready()
	{
		SpineBridge.SetAnimation(GetNode<Node3D>("SpineSprite"), "walk", true, 0);
	}
}
