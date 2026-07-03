using Godot;

public partial class MouseFollowing3D : Node3D
{
	private Node3D spineboy;
	private Node3D crosshairBone;
	private Camera3D camera;

	public override void _Ready()
	{
		spineboy = GetNode<Node3D>("Spineboy");
		crosshairBone = GetNode<Node3D>("Spineboy/CrosshairBone");
		camera = GetNode<Camera3D>("Camera3D");
		camera.Current = true;
		SpineBridge.SetAnimation(spineboy, "walk", true, 0);
		SpineBridge.SetAnimation(spineboy, "aim", true, 1);
	}

	public override void _Process(double delta)
	{
		if (camera == null)
			return;

		var hit = ScreenToSpinePlane(GetViewport().GetMousePosition());
		if (hit.HasValue)
			crosshairBone.GlobalPosition = hit.Value;
	}

	private Vector3? ScreenToSpinePlane(Vector2 screenPosition)
	{
		var rayOrigin = camera.ProjectRayOrigin(screenPosition);
		var rayDirection = camera.ProjectRayNormal(screenPosition);
		var planeNormal = (spineboy.GlobalTransform.Basis * Vector3.Back).Normalized();
		var plane = new Plane(planeNormal, spineboy.GlobalPosition);
		return plane.IntersectsRay(rayOrigin, rayDirection);
	}
}
