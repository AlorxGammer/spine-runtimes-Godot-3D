using Godot;

public partial class Lighting : Node2D
{	
	public override void _Ready()
	{
		SpineBridge.SetAnimation(GetNode<Node2D>("SpineSprite"), "walk", true, 0);
	}
}
