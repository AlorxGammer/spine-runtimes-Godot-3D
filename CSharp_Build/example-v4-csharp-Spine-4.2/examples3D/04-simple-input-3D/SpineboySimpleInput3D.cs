using Godot;

public partial class SpineboySimpleInput3D : Node3D
{
	public override void _Ready()
	{
		SpineBridge.SetAnimation(this, "idle", true, 0);
	}

	public override void _Process(double delta)
	{
		if (Input.IsActionJustPressed("ui_left"))
		{
			SpineBridge.SetAnimation(this, "run", true, 0);
			SpineBridge.SetScaleX(this, -1.0f);
		}

		if (Input.IsActionJustReleased("ui_left"))
			SpineBridge.SetAnimation(this, "idle", true, 0);

		if (Input.IsActionJustPressed("ui_right"))
		{
			SpineBridge.SetAnimation(this, "run", true, 0);
			SpineBridge.SetScaleX(this, 1.0f);
		}

		if (Input.IsActionJustReleased("ui_right"))
			SpineBridge.SetAnimation(this, "idle", true, 0);
	}
}
