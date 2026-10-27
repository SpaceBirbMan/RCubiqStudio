#ifndef RSPACE_H
#define RSPACE_H

/// Общий формат отрисовываемых объектов любой размерности.
/// Slice, extend и привязка к мешу - общие, их вызывает более вместительный по пространству плагин.
/// Платформенных и графических API нет. Исключений функции не бросают.
///
///
/// Нейронагенерено. Носит экспериментальный характер

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace RSpace {

constexpr std::uint8_t kDimMax = 32;
constexpr std::uint8_t kNoAxis = 255;

inline std::uint8_t clampDim(std::size_t d)
{
    return d > kDimMax ? kDimMax : static_cast<std::uint8_t>(d);
}

/// Как оси меньшего объекта входят в большее пространство.
/// Пустой srcAxis — тождество: dest[i] = src[i] пока оси хватает, иначе заполнение.
struct AxisMap {
    std::vector<std::uint8_t> srcAxis;
};

struct Coord {
    std::vector<float> v;

    std::uint8_t dim() const { return clampDim(v.size()); }

    float operator[](std::size_t i) const
    {
        return i < v.size() ? v[i] : 0.f;
    }

    void setDim(std::uint8_t n, float fill = 0.f)
    {
        v.assign(clampDim(n), fill);
    }

    Coord slice(std::uint8_t destDim, const AxisMap& map = {}) const;
    Coord extend(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;
    Coord bringTo(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;
};

inline Coord coord(std::initializer_list<float> xs)
{
    Coord c;
    const std::size_t n = xs.size() < kDimMax ? xs.size() : kDimMax;
    c.v.assign(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n));
    return c;
}

inline Coord zeros(std::uint8_t d)
{
    Coord c;
    c.setDim(d, 0.f);
    return c;
}

inline Coord ones(std::uint8_t d)
{
    Coord c;
    c.setDim(d, 1.f);
    return c;
}

inline std::uint8_t mappedSrc(const AxisMap& map, std::uint8_t destAxis,
                             std::uint8_t srcDim, std::uint8_t destDim)
{
    if (destAxis >= destDim)
        return kNoAxis;
    if (map.srcAxis.empty())
        return destAxis < srcDim ? destAxis : kNoAxis;
    if (destAxis >= map.srcAxis.size())
        return kNoAxis;
    const std::uint8_t s = map.srcAxis[destAxis];
    if (s == kNoAxis || s >= srcDim)
        return kNoAxis;
    return s;
}

inline Coord reproject(const Coord& src, std::uint8_t destDim,
                       const AxisMap& map = {}, const Coord& fill = {})
{
    destDim = clampDim(destDim);
    Coord out;
    out.v.resize(destDim, 0.f);
    const std::uint8_t srcDim = src.dim();
    for (std::uint8_t i = 0; i < destDim; ++i) {
        const std::uint8_t s = mappedSrc(map, i, srcDim, destDim);
        out.v[i] = (s != kNoAxis) ? src[s] : fill[i];
    }
    return out;
}

inline Coord Coord::slice(std::uint8_t destDim, const AxisMap& map) const
{
    return reproject(*this, destDim, map, {});
}

inline Coord Coord::extend(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    return reproject(*this, destDim, map, fill);
}

inline Coord Coord::bringTo(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    return reproject(*this, destDim, map, fill);
}

inline Coord add(const Coord& a, const Coord& b)
{
    const std::uint8_t n = a.dim() > b.dim() ? a.dim() : b.dim();
    Coord o;
    o.v.resize(n, 0.f);
    for (std::uint8_t i = 0; i < n; ++i)
        o.v[i] = a[i] + b[i];
    return o;
}

/// Покомпонентно. Недостающая ось у множителя считается 1, не 0.
inline Coord scaleBy(const Coord& a, const Coord& s)
{
    const std::uint8_t n = a.dim() > s.dim() ? a.dim() : s.dim();
    Coord o;
    o.v.resize(n, 0.f);
    for (std::uint8_t i = 0; i < n; ++i)
        o.v[i] = a[i] * (i < s.v.size() ? s.v[i] : 1.f);
    return o;
}

inline Coord reprojectScale(const Coord& src, std::uint8_t destDim, const AxisMap& map = {})
{
    return reproject(src, destDim, map, ones(clampDim(destDim)));
}

inline std::uint8_t roomierDim(std::uint8_t a, std::uint8_t b)
{
    a = clampDim(a);
    b = clampDim(b);
    return a > b ? a : b;
}

/// Привязка к чужому объекту. Указателей нет, родитель ищется по id.
/// Vertex — одна вершина. Primitive — точка на грани (веса; пустые — среднее).
/// Frame — система координат родителя: локальные оси ребёнка, шкала родителя применяется в placedOn.
struct Bind {
    enum class Kind : std::uint8_t { None = 0, Vertex = 1, Primitive = 2, Frame = 3 };

    Kind kind = Kind::None;
    std::string parentId;
    std::uint32_t index = 0;
    std::vector<float> weights;
    Coord localOffset;

    bool active() const { return kind != Kind::None; }
};

/// Упорядоченный набор индексов: 1 — точка, 2 — ребро, 3+ — грань любой размерности.
struct Primitive {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

struct Mesh {
    std::uint8_t dim = 0;
    std::vector<Coord> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Primitive> primitives;

    std::uint8_t spaceDim() const
    {
        if (dim)
            return clampDim(dim);
        if (!vertices.empty())
            return vertices.front().dim();
        return 0;
    }

    Coord sample(const Bind& bind) const;
    Mesh slice(std::uint8_t destDim, const AxisMap& map = {}) const;
    Mesh extend(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;
    Mesh bringTo(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;
};

inline Coord samplePrimitive(const Mesh& mesh, const Primitive& prim, const std::vector<float>& weights)
{
    const std::uint8_t d = mesh.spaceDim();
    if (prim.count == 0 || prim.first >= mesh.indices.size())
        return zeros(d);

    const std::uint32_t available = static_cast<std::uint32_t>(mesh.indices.size() - prim.first);
    const std::uint32_t n = prim.count < available ? prim.count : available;
    if (n == 0)
        return zeros(d);

    std::vector<float> w(n, 0.f);
    if (weights.size() == n) {
        w = weights;
    } else if (weights.empty()) {
        const float eq = 1.f / static_cast<float>(n);
        w.assign(n, eq);
    } else {
        const std::uint32_t copy = weights.size() < n ? static_cast<std::uint32_t>(weights.size()) : n;
        for (std::uint32_t i = 0; i < copy; ++i)
            w[i] = weights[i];
    }

    float sum = 0.f;
    for (std::uint32_t i = 0; i < n; ++i)
        sum += w[i];
    if (sum == 0.f) {
        const float eq = 1.f / static_cast<float>(n);
        w.assign(n, eq);
    } else {
        for (std::uint32_t i = 0; i < n; ++i)
            w[i] /= sum;
    }

    Coord out = zeros(d);
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t vi = mesh.indices[prim.first + i];
        if (vi >= mesh.vertices.size())
            continue;
        const Coord& p = mesh.vertices[vi];
        for (std::uint8_t a = 0; a < d; ++a)
            out.v[a] += w[i] * p[a];
    }
    return out;
}

inline Coord Mesh::sample(const Bind& bind) const
{
    const std::uint8_t d = spaceDim();
    if (bind.kind == Bind::Kind::None || bind.kind == Bind::Kind::Frame || vertices.empty())
        return zeros(d);
    if (bind.kind == Bind::Kind::Vertex) {
        if (bind.index >= vertices.size())
            return zeros(d);
        return vertices[bind.index].bringTo(d);
    }
    if (bind.index >= primitives.size())
        return zeros(d);
    return samplePrimitive(*this, primitives[bind.index], bind.weights);
}

inline Mesh reprojectMesh(const Mesh& src, std::uint8_t destDim,
                          const AxisMap& map = {}, const Coord& fill = {})
{
    Mesh m;
    m.dim = clampDim(destDim);
    m.indices = src.indices;
    m.primitives = src.primitives;
    m.vertices.reserve(src.vertices.size());
    for (const Coord& v : src.vertices)
        m.vertices.push_back(reproject(v, m.dim, map, fill));
    return m;
}

inline Mesh Mesh::slice(std::uint8_t destDim, const AxisMap& map) const
{
    return reprojectMesh(*this, destDim, map, {});
}

inline Mesh Mesh::extend(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    return reprojectMesh(*this, destDim, map, fill);
}

inline Mesh Mesh::bringTo(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    destDim = clampDim(destDim);
    if (map.srcAxis.empty() && spaceDim() == destDim)
        return *this;
    return reprojectMesh(*this, destDim, map, fill);
}

/// Объект сцены. Размерность — поле, не отдельный тип на каждую ось.
/// Видимость и zIndex — подсказки движку, не вызов отрисовки.
struct Object {
    std::string id;
    std::string kind;
    std::uint8_t dim = 0;
    Coord origin;
    Coord scale;
    Mesh mesh;
    Bind bind;
    /// Свободные пометки для движка: подпись, адрес картинки. Пространство их не толкует.
    std::unordered_map<std::string, std::string> props;
    int zIndex = 0;
    float opacity = 1.f;
    bool visible = true;

    std::uint8_t spaceDim() const
    {
        if (dim)
            return clampDim(dim);
        if (origin.dim())
            return origin.dim();
        return mesh.spaceDim();
    }

    Object slice(std::uint8_t destDim, const AxisMap& map = {}) const;
    Object extend(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;
    Object bringTo(std::uint8_t destDim, const AxisMap& map = {}, const Coord& fill = {}) const;

    /// Точка привязки в мире носителя (origin и шкала носителя уже учтены).
    Coord sampleHost(const Bind& b) const
    {
        return add(origin, scaleBy(mesh.sample(b), scale));
    }

    Coord worldVertex(std::size_t i) const
    {
        const std::uint8_t d = spaceDim();
        if (i >= mesh.vertices.size())
            return zeros(d);
        return add(origin, scaleBy(mesh.vertices[i], scale));
    }

    /// Снимок в пространстве max(своё, носитель). Вызывает вместительный плагин.
    /// Оси носителя, которых у ребёнка нет, копируются с точки привязки;
    /// свои оси — точка привязки плюс origin и localOffset.
    Object placedOn(const Object& host, const AxisMap& map = {}) const;
};

inline Object reprojectObject(const Object& src, std::uint8_t destDim,
                              const AxisMap& map = {}, const Coord& fill = {})
{
    destDim = clampDim(destDim);
    Object o = src;
    o.dim = destDim;
    o.origin = reproject(src.origin, destDim, map, fill);
    o.scale = reprojectScale(src.scale, destDim, map);
    o.mesh = reprojectMesh(src.mesh, destDim, map, fill);
    o.bind.localOffset = reproject(src.bind.localOffset, destDim, map, {});
    return o;
}

inline Object Object::slice(std::uint8_t destDim, const AxisMap& map) const
{
    return reprojectObject(*this, destDim, map, {});
}

inline Object Object::extend(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    return reprojectObject(*this, destDim, map, fill);
}

inline Object Object::bringTo(std::uint8_t destDim, const AxisMap& map, const Coord& fill) const
{
    destDim = clampDim(destDim);
    if (map.srcAxis.empty() && spaceDim() == destDim && mesh.spaceDim() == destDim)
        return *this;
    if (spaceDim() <= destDim)
        return extend(destDim, map, fill);
    return slice(destDim, map);
}

inline Object Object::placedOn(const Object& host, const AxisMap& map) const
{
    const std::uint8_t dest = roomierDim(spaceDim(), host.spaceDim());
    const Object hostH = host.bringTo(dest);
    Object out = bringTo(dest, map);
    if (!bind.active())
        return out;
    if (bind.kind == Bind::Kind::Frame) {
        const Coord local = add(out.origin, bind.localOffset.bringTo(dest, map));
        out.origin = add(hostH.origin, scaleBy(local, hostH.scale));
        const Coord childScale = out.scale.dim() ? out.scale : ones(dest);
        out.scale = scaleBy(childScale, hostH.scale);
        out.bind = Bind{};
        return out;
    }
    const Coord attach = hostH.sampleHost(bind);
    const Coord local = bind.localOffset.bringTo(dest, map);
    out.origin = add(add(attach, out.origin), local);
    out.bind = Bind{};
    return out;
}

/// Доска на шине хоста: движки кладут плоскости, остальные — добавки.
/// Писать только под мьютексом `kSceneBoardMutex`. Указатели живут в хосте.
struct SceneBoard {
    std::unordered_map<std::string, Object> surfaces;
    std::unordered_map<std::string, Object> overlays;
};

inline constexpr const char* kSceneBoard = "scene_board";
inline constexpr const char* kSceneBoardMutex = "scene_board_mutex";

} // namespace RSpace

#endif
