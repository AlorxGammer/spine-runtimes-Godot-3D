shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix;

uniform sampler2D spine_texture : source_color;
varying vec4 v_color;

void vertex() {
    v_color = COLOR;
}

void fragment() {
    vec4 tex = texture(spine_texture, UV) * v_color;
    tex.b = 0.0;
    ALBEDO = tex.rgb;
    ALPHA = tex.a;
}