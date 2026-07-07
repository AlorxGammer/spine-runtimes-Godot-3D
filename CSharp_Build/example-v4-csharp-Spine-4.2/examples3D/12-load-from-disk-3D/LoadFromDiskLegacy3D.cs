using Godot;

public partial class LoadFromDiskLegacy3D : Node3D
{
	public override void _Ready()
	{
		var skeletonFile = SpineBridge.AsObject(ClassDB.Instantiate("SpineSkeletonFileResource"));
		skeletonFile.Call("load_from_file", ProjectSettings.GlobalizePath("res://assets/Coin/coin-pro.skel"));

		var atlas = SpineBridge.AsObject(ClassDB.Instantiate("SpineAtlasResource"));
		atlas.Call("load_from_atlas_file", ProjectSettings.GlobalizePath("res://assets/Coin/coin.atlas"));

		var skeletonData = SpineBridge.AsObject(ClassDB.Instantiate("SpineSkeletonDataResource"));
		skeletonData.Set("skeleton_file_res", skeletonFile);
		skeletonData.Set("atlas_res", atlas);

		var sprite = SpineBridge.AsObject(ClassDB.Instantiate("SpineSprite3D")) as Node3D;
		if (sprite == null)
		{
			GD.PushError("SpineSprite3D is not available. Build/load the GDExtension first.");
			return;
		}

		sprite.Set("lighting_enabled", false);
		sprite.Set("generated_normal_map_enabled", false);
		sprite.Set("visible_alpha_cutoff", 0.0f);
		sprite.Set("skeleton_data_res", skeletonData);
		AddChild(sprite);

		var camera = GetNodeOrNull<Camera3D>("Camera3D") ?? GetViewport().GetCamera3D();
		if (camera != null)
		{
			camera.Current = true;
			var basis = camera.GlobalTransform.Basis;
			sprite.GlobalTransform = new Transform3D(basis, camera.GlobalPosition - basis.Z * 2.0f);
		}
		else
		{
			sprite.Position = new Vector3(0.0f, 0.0f, -2.0f);
		}

		SpineBridge.SetAnimation(sprite, "animation", true, 0);
	}
}
