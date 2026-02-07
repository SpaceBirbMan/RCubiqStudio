$input a_position, a_color0
$output v_color0

#include "bgfx_shader.sh"

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

void main() {
    gl_Position = u_proj * u_view * u_model * vec4(a_position, 1.0);
    v_color0 = a_color0;
}
