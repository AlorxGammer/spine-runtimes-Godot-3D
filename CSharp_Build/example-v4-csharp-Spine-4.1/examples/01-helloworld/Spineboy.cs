using Godot;

public partial class Spineboy : Node2D {
	public override void _Ready() {
		SpineBridge.SetAnimation(this, "run", true, 0);
	}
}
