using Godot;

public partial class MixAndMatch3D : Node3D
{
	public override async void _Ready()
	{
		await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);

		var data = SpineBridge.SkeletonData(this);
		var customSkin = SpineBridge.NewSkin(this, "custom-skin");
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "skin-base"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "nose/short"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "eyelids/girly"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "eyes/violet"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "hair/brown"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "clothes/hoodie-orange"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "legs/pants-jeans"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "accessories/bag"));
		SpineBridge.AddSkin(customSkin, SpineBridge.FindSkin(data, "accessories/hat-red-yellow"));
		SpineBridge.SetSkin(this, customSkin);
		SpineBridge.SetSlotsToSetupPose(this);

		foreach (Variant item in SpineBridge.GetAttachments(customSkin))
		{
			var entry = SpineBridge.AsObject(item);
			GD.Print(entry.Call("get_slot_index").AsInt32() + " " + entry.Call("get_name").AsString());
		}

		SpineBridge.SetAnimation(this, "dance", true, 0);
	}
}
