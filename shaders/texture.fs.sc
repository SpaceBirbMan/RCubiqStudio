$include "common.sc"

// Входные данные из вершинного шейдера
IN vec2 v_texcoord0;

// Юниформ для сэмплера текстуры (имя должно совпадать с createUniform в C++)
SAMPLER2D(u_texColor, 0);

void main()
{
    // Сэмплируем текстуру по UV-координатам
    // Функция texture2D автоматически обрабатывает фильтрацию и wrapping
    gl_FragColor = texture2D(u_texColor, v_texcoord0);
}
