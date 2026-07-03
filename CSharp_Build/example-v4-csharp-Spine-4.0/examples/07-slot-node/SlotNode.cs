using Godot;

public partial class SlotNode : Node2D
{
	public override void _Ready()
	{
		var spineboy = GetNode<Node2D>("Spineboy");
		var raptor = GetNode<Node2D>("Spineboy/GunSlot/Raptor");
		var tinySpineboy = GetNode<Node2D>("Spineboy/FrontFistSlot/TinySpineboy");

		var entry = SpineBridge.SetAnimation(spineboy, "run", true, 0);
		SpineBridge.SetTimeScale(entry, 0.1f);
		SpineBridge.SetAnimation(raptor, "walk", true, 0);
		SpineBridge.SetAnimation(tinySpineboy, "walk", true, 0);
	}
}
