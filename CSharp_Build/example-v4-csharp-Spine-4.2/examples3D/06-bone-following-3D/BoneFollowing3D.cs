using Godot;

public partial class BoneFollowing3D : Node3D
{
	public override void _Ready()
	{
		SpineBridge.SetAnimation(GetNode<Node3D>("Spineboy"), "walk", true, 0);
	}
}
