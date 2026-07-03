using Godot;

public partial class PlayCutscene : Node2D
{
	AnimationPlayer player;
	Node2D spineboy;
	float speed = 400;
	float velocityX = 0;
		
	public override void _Ready()
	{
		player = GetNode<AnimationPlayer>("AnimationPlayer");
		player.Play("cutscene");
		spineboy = GetNode<Node2D>("Spineboy");
	}
	
	public override void _Process(double delta)
	{
		if (player.IsPlaying()) return;
		
		if (Input.IsActionJustPressed("ui_left"))
		{
			SpineBridge.SetAnimation(spineboy, "run", true, 0);
			SpineBridge.SetScaleX(spineboy, -1);
			velocityX = -1;
		}

		if (Input.IsActionJustReleased("ui_left"))
		{
			SpineBridge.SetAnimation(spineboy, "idle", true, 0);
			velocityX = 0;
		}

		if (Input.IsActionJustPressed("ui_right"))
		{
			SpineBridge.SetAnimation(spineboy, "run", true, 0);
			SpineBridge.SetScaleX(spineboy, 1);
			velocityX = 1;
		}

		if (Input.IsActionJustReleased("ui_right"))
		{
			SpineBridge.SetAnimation(spineboy, "idle", true, 0);
			velocityX = 0;
		}

		var newPosition = spineboy.Position;
		newPosition.X += velocityX * speed * (float)delta;
		spineboy.Position = newPosition;
	}
}
