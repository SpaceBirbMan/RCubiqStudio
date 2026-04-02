$input a_position
$input a_color0
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    vec4 pos = vec4(a_position, 1.0);
    gl_Position = mul(u_modelViewProj, pos);
    v_color0 = a_color0;
}
