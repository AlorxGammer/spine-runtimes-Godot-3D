using Godot;
using System;

public partial class BoneNode3D : Node3D
{
	[Export] public float MoveSpeed = 1.5f;
	[Export] public float HoverHeight = 0.3f;
	[Export] public float GroundProbeHeight = 3.0f;
	[Export] public float GroundProbeDepth = 8.0f;
	[Export] public float GroundFollowSpeed = 8.0f;
	[Export] public float HipFollowSlack = 0.2f;
	[Export] public bool LoopMotion = true;
	[Export] public bool UseInitialXAsStart = true;
	[Export] public float ResetX = 5.35f;
	[Export] public float StartX = -5.0f;

	private const string CenterBoneName = "hoverboard-controller";
	private const string TargetBoneName = "board-ik";
	private const string HipBoneName = "hip";

	private Node3D spineboy;
	private Node3D centerBone;
	private RayCast3D centerRay;
	private Node3D targetBone;
	private RayCast3D targetRay;
	private Node3D hipBone;
	private float centerHipDistance;

	public override void _Ready()
	{
		spineboy = GetNode<Node3D>("Spineboy");
		centerBone = GetNode<Node3D>("Spineboy/HoverboardCenterBone");
		centerRay = GetNode<RayCast3D>("Spineboy/HoverboardCenterBone/CenterRay");
		targetBone = GetNode<Node3D>("Spineboy/HoverboardTargetBone");
		targetRay = GetNode<RayCast3D>("Spineboy/HoverboardTargetBone/TargetRay");
		hipBone = GetNode<Node3D>("Spineboy/HipBone");

		SetEnabled(centerBone, false);
		SetEnabled(targetBone, false);
		SetEnabled(hipBone, false);
		SetupWorldProbe(centerRay);
		SetupWorldProbe(targetRay);
		if (UseInitialXAsStart)
			StartX = spineboy.GlobalPosition.X;

		SpineBridge.SetAnimation(spineboy, "hoverboard", true, 0);
		SpineBridge.UpdateSkeleton(spineboy, 0.0);
		centerHipDistance = hipBone.GlobalPosition.Y - centerBone.GlobalPosition.Y;
	}

	public override void _PhysicsProcess(double delta)
	{
		var step = (float)delta;
		HoverBoneToGround(targetBone, targetRay, TargetBoneName, step);
		HoverBoneToGround(centerBone, centerRay, CenterBoneName, step);
		StabilizeHip();
		MoveSpineboy(step);
	}

	private static void SetEnabled(Node3D node, bool enabled)
	{
		node.Set("enabled", enabled);
	}

	private void SetupWorldProbe(RayCast3D ray)
	{
		ray.TopLevel = true;
		ray.Enabled = true;
		ray.TargetPosition = new Vector3(0.0f, -GroundProbeHeight - GroundProbeDepth, 0.0f);
	}

	private void HoverBoneToGround(Node3D bone, RayCast3D ray, string boneName, float delta)
	{
		var probeOrigin = bone.GlobalPosition;
		probeOrigin.Y += GroundProbeHeight;
		ray.GlobalTransform = new Transform3D(Basis.Identity, probeOrigin);
		ray.ForceRaycastUpdate();
		if (!ray.IsColliding())
			return;

		var bonePosition = bone.GlobalPosition;
		var targetY = ray.GetCollisionPoint().Y + HoverHeight;
		bonePosition.Y = Mathf.MoveToward(bonePosition.Y, targetY, GroundFollowSpeed * delta);
		bone.GlobalPosition = bonePosition;
		DriveSpineBoneFromMarker(boneName, bone);
	}

	private void StabilizeHip()
	{
		var currentDistance = hipBone.GlobalPosition.Y - centerBone.GlobalPosition.Y;
		if (Math.Abs(currentDistance) - Math.Abs(centerHipDistance) >= HipFollowSlack)
			return;

		var hipPosition = hipBone.GlobalPosition;
		hipPosition.Y = centerBone.GlobalPosition.Y + centerHipDistance;
		hipBone.GlobalPosition = hipPosition;
		DriveSpineBoneFromMarker(HipBoneName, hipBone);
	}

	private void MoveSpineboy(float delta)
	{
		var position = spineboy.GlobalPosition;
		position.X += delta * MoveSpeed;
		if (LoopMotion && position.X > ResetX)
			position.X = StartX;
		spineboy.GlobalPosition = position;
	}

	private void DriveSpineBoneFromMarker(string boneName, Node3D marker)
	{
		var markerInSpineboySpace = spineboy.GlobalTransform.AffineInverse() * marker.GlobalTransform;
		SpineBridge.SetGlobalBoneTransform(spineboy, boneName, Transform3DToSpine2D(markerInSpineboySpace));
	}

	private Transform2D Transform3DToSpine2D(Transform3D sourceTransform)
	{
		var ppu = SpineBridge.GetPixelsPerUnit(spineboy);
		var xAxis3D = sourceTransform.Basis.X;
		var yAxis3D = sourceTransform.Basis.Y;
		var xAxis = new Vector2(xAxis3D.X, -xAxis3D.Y);
		var yAxis = new Vector2(yAxis3D.X, -yAxis3D.Y);
		var origin = new Vector2(sourceTransform.Origin.X * ppu, -sourceTransform.Origin.Y * ppu);
		var boneTransform = new Transform2D(Mathf.Atan2(xAxis.Y, xAxis.X), origin);
		boneTransform.X *= xAxis.Length();
		boneTransform.Y *= yAxis.Length();
		return boneTransform;
	}
}
