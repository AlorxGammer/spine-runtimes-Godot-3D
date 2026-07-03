using Godot;

public partial class AnimationStateListener3D : Node3D
{
	private AudioStreamPlayer footstepAudio;

	public override void _Ready()
	{
		footstepAudio = GetNode<AudioStreamPlayer>("FootstepAudio");
		var spineboy = GetNode<Node3D>("Spineboy");
		spineboy.Connect("animation_started", Callable.From<Variant, Variant, Variant>(OnAnimationStarted));
		spineboy.Connect("animation_interrupted", Callable.From<Variant, Variant, Variant>(OnAnimationInterrupted));
		spineboy.Connect("animation_ended", Callable.From<Variant, Variant, Variant>(OnAnimationEnded));
		spineboy.Connect("animation_completed", Callable.From<Variant, Variant, Variant>(OnAnimationCompleted));
		spineboy.Connect("animation_disposed", Callable.From<Variant, Variant, Variant>(OnAnimationDisposed));
		spineboy.Connect("animation_event", Callable.From<Variant, Variant, Variant, Variant>(OnAnimationEvent));

		SpineBridge.SetAnimation(spineboy, "jump", false, 0);
		SpineBridge.AddAnimation(spineboy, "walk", 0.0f, true, 0);
		SpineBridge.AddAnimation(spineboy, "run", 2.0f, true, 0);
	}

	private void OnAnimationStarted(Variant sprite, Variant animationState, Variant trackEntry)
	{
		GD.Print("Animation started: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)));
	}

	private void OnAnimationInterrupted(Variant sprite, Variant animationState, Variant trackEntry)
	{
		GD.Print("Animation interrupted: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)));
	}

	private void OnAnimationEnded(Variant sprite, Variant animationState, Variant trackEntry)
	{
		GD.Print("Animation ended: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)));
	}

	private void OnAnimationCompleted(Variant sprite, Variant animationState, Variant trackEntry)
	{
		GD.Print("Animation completed: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)));
	}

	private void OnAnimationDisposed(Variant sprite, Variant animationState, Variant trackEntry)
	{
		GD.Print("Animation disposed: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)));
	}

	private void OnAnimationEvent(Variant sprite, Variant animationState, Variant trackEntry, Variant eventObject)
	{
		var eventName = SpineBridge.EventName(SpineBridge.AsObject(eventObject));
		GD.Print("Animation event: " + SpineBridge.TrackAnimationName(SpineBridge.AsObject(trackEntry)) + ", " + eventName);
		if (eventName == "footstep")
			footstepAudio.Play();
	}
}
