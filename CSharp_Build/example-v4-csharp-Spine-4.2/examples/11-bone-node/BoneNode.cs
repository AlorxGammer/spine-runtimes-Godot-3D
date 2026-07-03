using Godot;
using System;

public partial class BoneNode : Node2D
{
	private Node2D spineboy;
	private Node2D centerBone;
	private RayCast2D centerRay;
	private Node2D targetBone;
	private RayCast2D targetRay;
	private Node2D hipBone;
	private float centerHipDistance = 0;
	
	public override void _Ready()
	{
		spineboy = GetNode<Node2D>("SpineSprite");
		centerBone = GetNode<Node2D>("SpineSprite/HoverboardCenterBone");
		centerRay = GetNode<RayCast2D>("SpineSprite/HoverboardCenterBone/CenterRay");
		targetBone = GetNode<Node2D>("SpineSprite/HoverboardTargetBone");
		targetRay = GetNode<RayCast2D>("SpineSprite/HoverboardTargetBone/TargetRay");
		hipBone = GetNode<Node2D>("SpineSprite/HipBone");
		SpineBridge.SetAnimation(spineboy, "hoverboard", true, 0);
		SpineBridge.UpdateSkeleton(spineboy, 0);
		centerHipDistance = hipBone.GlobalPosition.Y - centerBone.GlobalPosition.Y;
	}
	
	public override void _Process(double delta) 
	{
		if (targetRay.IsColliding())
		{
			var newPosition = targetBone.GlobalPosition;
			newPosition.Y = targetRay.GetCollisionPoint().Y - 30;
			targetBone.Position = newPosition;
		}
		
		if (centerRay.IsColliding())
		{
			var newPosition = centerBone.GlobalPosition;
			newPosition.Y = centerRay.GetCollisionPoint().Y - 30;
			centerBone.Position = newPosition;
		}

		if (Math.Abs(hipBone.GlobalPosition.Y - centerBone.GlobalPosition.Y) - Math.Abs(centerHipDistance) < 20)
		{
			var newPosition = hipBone.GlobalPosition;
			newPosition.Y = centerBone.GlobalPosition.Y + centerHipDistance;
			hipBone.Position = newPosition;
		}

		var position = spineboy.GlobalPosition;
		position.X += (float)delta * 150;
		spineboy.GlobalPosition = position;
	}
}
