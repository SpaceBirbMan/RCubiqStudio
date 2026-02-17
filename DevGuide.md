# Основы

В этом файле описаны ключевые требования и инструкции по сборке и пониманию функционирования приложения.

## Структура проекта

<span style="color:red; font-weight:bold; font-size: 30px"> Обязательно! </span>

proj_dir
* M3 - основное приложение
* bgfx - основная зависимость в рендере
    * bgfx
    * bgfx.cmake
    * bx
    * bimg

Дополнительно в папку proj_dir можно добавлять плагины, так как они имеют одинаковые с главным приложением зависимости
bgfx предварительно надо собрать с отключенным SPIRV (по неизвестной причине в библиотеке есть единственная ошибка связанная с этим).
### Ссылки на зависимости

* bgfx - https://github.com/bkaradzic/bgfx
* pacman -S mingw-w64-x86_64-opencv mingw-w64-x86_64-hidapi mingw-w64-x86_64-libusb
* libserial
* miniaudio

### Может понадобиться 

* mingw-w64-x86_64-libbluray
* mingw-w64-x86_64-imath
* mingw-w64-x86_64-ffmpeg
* mingw-w64-x86_64-openexr

```todo: Картинки```

```todo: Docker```
## Сообщения

