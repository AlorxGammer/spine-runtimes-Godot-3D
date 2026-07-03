using Godot;

public partial class BoneFollowing : Node2D
{
	public override void _Ready()
	{
		SpineBridge.SetAnimation(GetNode<Node2D>("Spineboy"), "walk", true, 0);
	}
}
