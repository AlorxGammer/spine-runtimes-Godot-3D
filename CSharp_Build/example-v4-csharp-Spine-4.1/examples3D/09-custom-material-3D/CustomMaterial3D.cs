using Godot;

public partial class CustomMaterial3D : Node3D
{
	public override void _Ready()
	{
		SpineBridge.SetAnimation(this, "walk", true, 0);
	}
}
