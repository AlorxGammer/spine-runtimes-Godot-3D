using Godot;

public partial class PlayCutscene3D : Node3D
{
	private Node3D spineboy;
	private AnimationPlayer animationPlayer;

	public override void _Ready()
	{
		spineboy = GetNode<Node3D>("Spineboy");
		animationPlayer = GetNodeOrNull<AnimationPlayer>("AnimationPlayer");
		SpineBridge.SetAnimation(spineboy, "walk", true, 0);
		if (animationPlayer != null)
		{
			EnsureCutsceneAnimation();
			animationPlayer.Play("cutscene");
		}
	}

	public override void _Input(InputEvent @event)
	{
		if (!@event.IsActionPressed("ui_accept"))
			return;

		SpineBridge.SetAnimation(spineboy, "jump", false, 0);
		SpineBridge.AddAnimation(spineboy, "walk", 0.0f, true, 0);
	}

	private void EnsureCutsceneAnimation()
	{
		if (animationPlayer.HasAnimation("cutscene"))
			return;

		var animation = new Animation { ResourceName = "cutscene", Length = 2.0f, LoopMode = Animation.LoopModeEnum.Linear };
		var track = animation.AddTrack(Animation.TrackType.Position3D);
		animation.TrackSetPath(track, new NodePath("Spineboy"));
		animation.TrackInsertKey(track, 0.0f, Vector3.Zero);
		animation.TrackInsertKey(track, 1.0f, new Vector3(0.18f, 0.0f, 0.0f));
		animation.TrackInsertKey(track, 2.0f, Vector3.Zero);

		var library = new AnimationLibrary();
		library.AddAnimation("cutscene", animation);
		animationPlayer.AddAnimationLibrary("", library);
	}
}
