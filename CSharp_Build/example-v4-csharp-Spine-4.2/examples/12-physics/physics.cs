using Godot;
using System;

public partial class physics : Node2D
{
	private Node2D celestial_circus;
	
	private float last_x = -1;
	private float last_y = -1;
	private bool isMouseOver = false;

	public override void _Ready()
	{
		celestial_circus = GetNode<Node2D>("celestial-circus");
		SpineBridge.SetAnimation(celestial_circus, "wind-idle", true, 0);
		SpineBridge.SetAnimation(celestial_circus, "eyeblink-long", true, 1);
		SpineBridge.SetAnimation(celestial_circus, "stars", true, 2);
	}

	public override void _Process(double delta)
	{	
		if (Input.IsMouseButtonPressed(MouseButton.Left) && isMouseOver){
			var pos = GetViewport().GetMousePosition();
			if(last_x != -1){
				var dx = pos.X - last_x;
				var dy = pos.Y - last_y;
				celestial_circus.GlobalPosition += new Vector2(dx, dy);
				SpineBridge.PhysicsTranslate(celestial_circus, dx / celestial_circus.Scale.X, dy / celestial_circus.Scale.Y);
			}
			last_x = pos.X;
			last_y = pos.Y;
		}
		else{
			last_x = -1;
			last_y = -1;
		}
	}

	private void _on_area_2d_mouse_entered()
	{
		isMouseOver = true;
	}

	private void _on_area_2d_mouse_exited()
	{
		isMouseOver = false;
	}
}
