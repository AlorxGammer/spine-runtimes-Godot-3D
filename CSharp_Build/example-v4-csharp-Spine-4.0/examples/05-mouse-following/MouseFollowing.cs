using Godot;

public partial class MouseFollowing : Node2D
{
	private Node2D spineboy;
	private Node2D crosshairBone;
	
	public override void _Ready()
	{
		spineboy = GetNode<Node2D>("Spineboy");
		crosshairBone = spineboy.GetNode<Node2D>("CrosshairBone");
		SpineBridge.SetAnimation(spineboy, "walk", true, 0);
		SpineBridge.SetAnimation(spineboy, "aim", true, 1);
	}
	
	public override void _Process(double delta)
	{
		crosshairBone.GlobalPosition = GetViewport().GetMousePosition();
	}
}
