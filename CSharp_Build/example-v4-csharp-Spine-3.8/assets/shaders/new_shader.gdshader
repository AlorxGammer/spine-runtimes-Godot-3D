shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix;

uniform sampler2D spine_texture : source_color;
uniform vec3 tint_color : source_color = vec3(0.45, 1.0, 0.95);
uniform float tint_strength : hint_range(0.0, 1.0) = 0.38;
uniform float saturation : hint_range(0.0, 2.0) = 1.18;
varying vec4 v_color;

void vertex() {
    v_color = COLOR;
}

void fragment() {
    vec4 tex = texture(spine_texture, UV) * v_color;

    vec3 tinted = mix(tex.rgb, tex.rgb * tint_color, tint_strength);
    float luma = dot(tinted, vec3(0.299, 0.587, 0.114));
    ALBEDO = mix(vec3(luma), tinted, saturation);
    ALPHA = tex.a;
}
