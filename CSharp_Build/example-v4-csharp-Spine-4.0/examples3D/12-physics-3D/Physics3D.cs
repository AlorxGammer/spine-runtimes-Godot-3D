using Godot;

public partial class Physics3D : Node3D
{
	[Export] public float SpinePhysicsStrength = 1.0f;

	private Node3D celestialCircus;
	private Camera3D camera;
	private bool isMouseOver;
	private bool isDragging;
	private Vector3 dragOffset = Vector3.Zero;
	private Vector3 targetPosition = Vector3.Zero;
	private Vector2 mouseRelative = Vector2.Zero;

	public override async void _Ready()
	{
		await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);

		celestialCircus = GetNode<Node3D>("celestial-circus");
		camera = GetNodeOrNull<Camera3D>("Camera3D") ?? GetViewport().GetCamera3D();
		if (camera == null)
		{
			GD.PushError("Camera3D not found in viewport.");
			return;
		}
		camera.Current = true;

		SpineBridge.SetAnimation(celestialCircus, "wind-idle", true, 0);
		SpineBridge.SetAnimation(celestialCircus, "eyeblink-long", true, 1);
		SpineBridge.SetAnimation(celestialCircus, "stars", true, 2);
		targetPosition = celestialCircus.GlobalPosition;
	}

	public override void _Input(InputEvent @event)
	{
		if (@event is InputEventMouseButton button && button.ButtonIndex == MouseButton.Left)
		{
			if (button.Pressed)
			{
				if (!isMouseOver)
					return;

				isDragging = true;
				var mouseWorld = ScreenToWorldOnObjectPlane(button.Position, celestialCircus.GlobalPosition.Z);
				dragOffset = celestialCircus.GlobalPosition - mouseWorld;
				targetPosition = celestialCircus.GlobalPosition;
				mouseRelative = Vector2.Zero;
			}
			else
			{
				isDragging = false;
				mouseRelative = Vector2.Zero;
			}
		}
		else if (@event is InputEventMouseMotion motion && isDragging)
		{
			mouseRelative = motion.Relative;
			var mouseWorld = ScreenToWorldOnObjectPlane(motion.Position, celestialCircus.GlobalPosition.Z);
			targetPosition = mouseWorld + dragOffset;
		}
	}

	public override void _Process(double delta)
	{
		if (!isDragging)
			return;

		celestialCircus.GlobalPosition = targetPosition;
		ApplySpinePhysicsFromMouseDelta(mouseRelative);
		mouseRelative = Vector2.Zero;
	}

	private Vector3 ScreenToWorldOnObjectPlane(Vector2 screenPosition, float zPlane)
	{
		if (camera == null)
			return celestialCircus.GlobalPosition;

		var rayOrigin = camera.ProjectRayOrigin(screenPosition);
		var rayDirection = camera.ProjectRayNormal(screenPosition);
		if (Mathf.Abs(rayDirection.Z) < 0.0001f)
			return celestialCircus.GlobalPosition;

		var t = (zPlane - rayOrigin.Z) / rayDirection.Z;
		return rayOrigin + rayDirection * t;
	}

	private void ApplySpinePhysicsFromMouseDelta(Vector2 relative)
	{
		if (relative == Vector2.Zero)
			return;

		var scale = celestialCircus.Scale;
		var sx = Mathf.Abs(scale.X) < 0.0001f ? 1.0f : scale.X;
		var sy = Mathf.Abs(scale.Y) < 0.0001f ? 1.0f : scale.Y;
		SpineBridge.PhysicsTranslate(celestialCircus, relative.X / sx * SpinePhysicsStrength, relative.Y / sy * SpinePhysicsStrength);
	}

	private void _on_area_3d_mouse_entered()
	{
		isMouseOver = true;
	}

	private void _on_area_3d_mouse_exited()
	{
		isMouseOver = false;
	}
}
