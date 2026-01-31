#ifndef RENDERAPI_H
#define RENDERAPI_H


class Renderer {

public:

    Renderer() {}
    ~Renderer() {}

    typedef void* RenderContext;
    typedef void* TextureHandle;

    RenderContext CreateRenderContext(const char* backend); // "opengl" или "directx"
    void DestroyRenderContext(RenderContext ctx);

    TextureHandle CreateTexture(RenderContext ctx, int width, int height, void* data);
    void UpdateTexture(RenderContext ctx, TextureHandle tex, void* data);
    void ReadTexture(RenderContext ctx, TextureHandle tex, void* out_data);
    void BindTexture(RenderContext ctx, TextureHandle tex);
    void RenderQuad(RenderContext ctx); // или другая отрисовка

};

#endif // RENDERAPI_H
