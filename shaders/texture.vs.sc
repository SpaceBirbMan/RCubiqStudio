$include "common.sc"

// Входные атрибуты вершин (должны совпадать с VertexLayout в C++)
IN vec3 a_position;
IN vec2 a_texcoord0;

// Выходящие данные во фрагментный шейдер
OUT vec2 v_texcoord0;

void main()
{
    // Передаем UV-координаты дальше
    v_texcoord0 = a_texcoord0;

    // Так как квад уже в координатах экрана (-1..1), просто присваиваем позицию.
    // Если бы нужна была проекция, мы бы умножали на u_modelViewProj.
    gl_Position = vec4(a_position, 1.0);
}
