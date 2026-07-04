using Godot;

[Tool]
public partial class OmniLight25D : OmniLight3D
{
    private const string FillLightName = "__fill_light_25d";
    private const string RimLightName = "__rim_light_25d";
    [Export] public Color key_color { get; set; } = new(1.0f, 0.15f, 0.08f, 1.0f);
    [Export] public Color fill_color { get; set; } = new(0.58f, 0.16f, 0.11f, 1.0f);
    [Export] public Color rim_color { get; set; } = new(1.0f, 0.38f, 0.18f, 1.0f);
    [Export] public float key_energy { get; set; } = 1.45f;
    [Export] public float key_range { get; set; } = 3.5f;
    [Export] public bool key_shadows { get; set; } = false;
    [Export] public float fill_energy { get; set; } = 0.18f;
    [Export] public float fill_range { get; set; } = 4.2f;
    [Export] public Vector3 fill_offset { get; set; } = new(-0.24f, 0.18f, 0.24f);
    [Export] public float rim_energy { get; set; } = 0.34f;
    [Export] public float rim_range { get; set; } = 4.0f;
    [Export] public Vector3 rim_offset { get; set; } = new(0.12f, 0.14f, -0.18f);
    [Export] public float flicker_strength { get; set; } = 0.03f;
    [Export] public float flicker_speed { get; set; } = 2.6f;
    [Export] public bool animate_in_editor { get; set; } = false;
    private OmniLight3D _fillLight;
    private OmniLight3D _rimLight;
    public override void _Ready() { EnsureRig(); SyncRig(1.0f); SetProcess(!Engine.IsEditorHint() || animate_in_editor); }
    public override void _Process(double delta) { SyncRig(GetFlickerMultiplier()); }
    private void EnsureRig() { _fillLight = EnsureLight(FillLightName); _rimLight = EnsureLight(RimLightName); }
    private OmniLight3D EnsureLight(string nodeName) { if (GetNodeOrNull<OmniLight3D>(nodeName) is { } existing) return existing; var light = new OmniLight3D { Name = nodeName }; AddChild(light); return light; }
    private void SyncRig(float flickerMultiplier) { EnsureRig(); LightColor = key_color; LightEnergy = key_energy * flickerMultiplier; OmniRange = key_range; ShadowEnabled = key_shadows; _fillLight.Position = fill_offset; _fillLight.LightColor = fill_color; _fillLight.LightEnergy = fill_energy * Mathf.Lerp(1.0f, flickerMultiplier, 0.35f); _fillLight.OmniRange = fill_range; _fillLight.ShadowEnabled = false; _rimLight.Position = rim_offset; _rimLight.LightColor = rim_color; _rimLight.LightEnergy = rim_energy * Mathf.Lerp(1.0f, flickerMultiplier, 0.45f); _rimLight.OmniRange = rim_range; _rimLight.ShadowEnabled = false; }
    private float GetFlickerMultiplier() { if (flicker_strength <= 0.0f || flicker_speed <= 0.0f) return 1.0f; var t = Time.GetTicksMsec() * 0.001f * flicker_speed; var wave = Mathf.Sin(t * 1.13f) + Mathf.Sin(t * 2.27f + 0.7f) * 0.55f + Mathf.Sin(t * 4.91f + 1.9f) * 0.22f; return 1.0f + wave * flicker_strength * 0.5f; }
}
