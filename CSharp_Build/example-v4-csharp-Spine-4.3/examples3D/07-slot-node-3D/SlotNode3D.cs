using Godot;

public partial class SlotNode3D : Node3D
{
	public override async void _Ready()
	{
		await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);

		var spineboy = GetNode<Node3D>("Spineboy");
		var raptor = GetNode<Node3D>("Spineboy/GunSlot/Raptor");
		var tinySpineboy = GetNode<Node3D>("Spineboy/FrontFistSlot/TinySpineboy");

		var entry = SpineBridge.SetAnimation(spineboy, "run", true, 0);
		SpineBridge.SetTimeScale(entry, 0.1f);
		SpineBridge.SetAnimation(raptor, "walk", true, 0);
		SpineBridge.SetAnimation(tinySpineboy, "walk", true, 0);
	}
}
