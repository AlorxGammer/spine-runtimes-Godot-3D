using Godot;
using Godot.Collections;

public static class SpineBridge
{
	public static GodotObject AsObject(Variant value)
	{
		return value.AsGodotObject();
	}

	public static GodotObject AnimationState(GodotObject sprite)
	{
		return AsObject(sprite.Call("get_animation_state"));
	}

	public static GodotObject Skeleton(GodotObject sprite)
	{
		return AsObject(sprite.Call("get_skeleton"));
	}

	public static GodotObject SkeletonData(GodotObject sprite)
	{
		return AsObject(Skeleton(sprite).Call("get_data"));
	}

	public static GodotObject SetAnimation(GodotObject sprite, string animationName, bool loop, int track = 0)
	{
		return AsObject(AnimationState(sprite).Call("set_animation", animationName, loop, track));
	}

	public static GodotObject AddAnimation(GodotObject sprite, string animationName, float delay, bool loop, int track = 0)
	{
		return AsObject(AnimationState(sprite).Call("add_animation", animationName, delay, loop, track));
	}

	public static void SetScaleX(GodotObject sprite, float scale)
	{
		Skeleton(sprite).Call("set_scale_x", scale);
	}

	public static GodotObject NewSkin(GodotObject sprite, string name)
	{
		return AsObject(sprite.Call("new_skin", name));
	}

	public static GodotObject FindSkin(GodotObject skeletonData, string name)
	{
		return AsObject(skeletonData.Call("find_skin", name));
	}

	public static void AddSkin(GodotObject skin, GodotObject other)
	{
		if (skin != null && other != null)
			skin.Call("add_skin", other);
	}

	public static Array GetAttachments(GodotObject skin)
	{
		return skin.Call("get_attachments").AsGodotArray();
	}

	public static void SetSkin(GodotObject sprite, GodotObject skin)
	{
		Skeleton(sprite).Call("set_skin", skin);
	}

	public static void SetSlotsToSetupPose(GodotObject sprite)
	{
		Skeleton(sprite).Call("set_slots_to_setup_pose");
	}

	public static void UpdateSkeleton(GodotObject sprite, double delta = 0.0)
	{
		sprite.Call("update_skeleton", delta);
	}

	public static void PhysicsTranslate(GodotObject sprite, float x, float y)
	{
		Skeleton(sprite).Call("physics_translate", x, y);
	}

	public static float GetPixelsPerUnit(GodotObject sprite)
	{
		return (float)sprite.Call("get_pixels_per_unit").AsDouble();
	}

	public static void SetGlobalBoneTransform(GodotObject sprite, string boneName, Transform2D transform)
	{
		sprite.Call("set_global_bone_transform", boneName, transform);
	}

	public static void SetTimeScale(GodotObject trackEntry, float timeScale)
	{
		trackEntry?.Call("set_time_scale", timeScale);
	}

	public static string TrackAnimationName(GodotObject trackEntry)
	{
		var animation = AsObject(trackEntry.Call("get_animation"));
		return animation?.Call("get_name").AsString() ?? string.Empty;
	}

	public static string EventName(GodotObject spineEvent)
	{
		var data = AsObject(spineEvent.Call("get_data"));
		return data?.Call("get_event_name").AsString() ?? string.Empty;
	}
}
