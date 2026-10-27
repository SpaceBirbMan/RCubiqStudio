#ifndef RCQAPI_H
#define RCQAPI_H

#include <set>
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <deque>
#include "abstractuinodes.h"
#include "ieventmanager.h"


// Общая синхронизация для слотов шины, которые плагины меняют при работе/выгрузке,
// а хост (движок) читает из другого потока (например control_table).
namespace PluginBusLocks {
inline std::mutex& streamBusMutex() {
    static std::mutex m;
    return m;
}
}

// N-мерные объекты — src/rspace.h. Доска плоскостей и добавок на шине: scene_board, scene_board_mutex.
class ITexture {
protected:
    std::string backend = "";
    void* rawPointer = nullptr;
};

using path_string = std::string;

/// Общий мьютекс для `render_pipeline` на шине: плагин меняет вектор (emplace / nullptr),
/// хост итерирует на тике — без блокировки возможна гонка.
namespace HostInterop {
inline std::mutex& renderPipelineMutex() {
    static std::mutex m;
    return m;
}
}

using json = nlohmann::json;
using payload = std::vector<uint8_t>; //байт-буфер для payload
using void_func = std::function<void(const std::any&)>;

struct cacheForm {
    std::string name;
    std::function<void(const nlohmann::json&)> desfn;   // десериализация: принимает json
    std::function<nlohmann::json()> sefn;               // сериализация: возвращает json
};

struct Frame {
    std::vector<uint8_t> pixels{}; // пиксели (8 бит на каждый канал)
    int width;
    int height;
    short stride; // шаг для массива пикселей
};

using renderQueue = std::deque<Frame>;

struct subStruct {
    std::string receiver = "N/A";
    std::string name;
    std::function<void(const std::any&)> callback;

    subStruct(std::string r, std::string m, std::function<void(const std::any&)> cb)
        : receiver(std::move(r)), name(std::move(m)), callback(std::move(cb)) {}
};

struct GraphicBus {
    std::deque<std::shared_ptr<void>> *textures_handlers_in_app = nullptr;
    std::deque<std::shared_ptr<void>> *textures_handlers_in_engine = nullptr;
    std::deque<std::shared_ptr<void>> *images = nullptr;
};


struct TextureHandle {
    uint32_t id;
};

enum class PixelFormat : uint8_t {
    RGBA8,
    BGRA8,
    R8,
    // ...
};

class HttpRequest {
public:
    std::string sender = "";
    std::string url = "";
    json object = json();
    std::string reason = "";
    /// Если не пусто, уходит как тело запроса вместо `object`.
    std::string rawBody;
    std::string contentType = "application/json";
    std::unordered_map<std::string, std::string> headers;

    HttpRequest(std::string s, std::string u, std::string r, json obj) : sender(s), url(u), reason(r), object(obj) {}
    HttpRequest(std::string s, std::string u, std::string r) : sender(s), url(u), reason(r) {}
};

/// Хост открывает url в браузере и ловит redirect на 127.0.0.1:port.
/// Ответ — HttpRespond на `{sender}_browser_auth_respond`: raw это fragment или query.
struct BrowserAuth {
    std::string sender;
    std::string url;
    std::string reason;
    std::string state;
    int port = 17563;
    int timeoutSec = 180;
};

/// Хост держит сокет. Пустой url закрывает текущий.
/// Кадры приходят как WebSocketFrame на `{sender}_websocket_frame`.
struct WebSocketOpen {
    std::string sender;
    std::string url;
    std::string reason;
};

struct WebSocketFrame {
    std::string reason;
    std::string text;
    bool closed = false;
};

class HttpRespond {
public:
    std::string code = "0";
    json object = json();
    std::string reason = "";
    std::string raw;

    HttpRespond() = default;
    HttpRespond(std::string c, std::string r, json o): code(c), object(o){}
    HttpRespond(std::string c, std::string r) : code(c) {}
};

struct TextureDesc {
    uint16_t width;
    uint16_t height;
    uint8_t layers;
    PixelFormat format;
    const void* data; // только для создания, не хранится
    uint32_t dataSize;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual TextureHandle createTexture(const TextureDesc* desc) = 0;
    virtual void destroyTexture(TextureHandle handle) = 0;
    virtual void setWindowHandle(void* handle) = 0;
    virtual void frame() = 0;
    virtual void test() = 0;
};

struct CacheObject {
    nlohmann::json object;
    std::string name;
};

struct CameraInfo {
    int index;
    std::string name;
    int width;
    int height;
    double maxFps;

    std::string to_string() {
        return std::to_string(index) + " " + name + " " + std::to_string(width) + "x" + std::to_string(height) + " "+  std::to_string(maxFps) + "\n";
    }
};

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    std::string type;
};

/// Единая запись списка устройств (камеры, захват звука, HID, зарегистрированные в DeviceManager).
struct PluginDeviceDescriptor {
    std::string id;
    std::string name;
    /// Например: "camera", "audio_capture", "hid", "device".
    std::string kind;
};

class Service {
    private:
        std::string name = "unnamed";

    public:
        virtual ~Service() = default;
        virtual std::string getName() const final { return name; }
        virtual void setName(const std::string n) final { this->name = n; }
};

/// Хост предоставляет поток захвата; плагин только читает моно F32 сэмплы (без SDL/miniaudio в DLL).
class IAudioCaptureStream {
public:
    virtual ~IAudioCaptureStream() = default;
    /// Читает до maxSamples моно-сэмплов. Возвращает число записанных float.
    virtual int read(float* out, int maxSamples) noexcept = 0;
    virtual bool isOpen() const noexcept = 0;
};

/// Устройства, доступные приложению (агрегирует DeviceManager + miniaudio для открытия захвата).
class IPluginDeviceBroker {
public:
    virtual ~IPluginDeviceBroker() = default;
    virtual std::vector<PluginDeviceDescriptor> listDevices() = 0;
    /// Только микрофоны (без камер/HID) — не блокировать поток сообщений полным listDevices().
    virtual std::vector<PluginDeviceDescriptor> listAudioCaptureDevices() = 0;
    /// Только для kind == "audio_capture": id — бинарный blob `ma_device_id` (как раньше у списка микрофонов).
    virtual IAudioCaptureStream* openAudioCapture(const std::string& deviceId) = 0;
};

struct PluginFileReadResult {
    bool ok = false;
    std::vector<uint8_t> bytes;
};

/// Файлы плагина под `plugins_cache` (записываемый корень задаёт хост) + относительный путь на scopeKey из `setPluginStorageRelativePath`.
class IAppLogger;
class ModuleLogProvider;

/// Центральный логгер хоста на шине как `app_logger` (см. `logger.h`).
/// Плагин: `auto* lg = std::any_cast<IAppLogger*>(bus->getData("app_logger")); lg->module("MyPlugin").info("...");`
/// Либо сообщение `LoggerEvents::kLogWrite` с `LogRecord` через `IEventManager::sendMessage`.

class IPluginFileBroker {
public:
    virtual ~IPluginFileBroker() = default;
    /// Не более одного сегмента или подпути без `..`; пустая строка — только каталог по scopeKey.
    virtual void setPluginStorageRelativePath(const std::string& scopeKey, const std::string& relativePathUnderPluginsCache) = 0;
    virtual PluginFileReadResult read(const std::string& scopeKey, const std::string& fileName) = 0;
    virtual bool write(const std::string& scopeKey, const std::string& fileName, const uint8_t* data, size_t size) = 0;
};

struct TrackerInfo {

};

enum class PluginUIType { Engine, Tracker, Generic };

struct PluginUIInfo {
    std::string name;        // display name (human-readable)
    std::string description; // short purpose blurb for toolbox
    std::string path;        // file path (key for removal/identification)
    PluginUIType type;
};

/// Трекер сообщает хосту о своих контроллерах (до или после activate).
struct ControlTableRegister {
    std::string trackerPath;
    std::unordered_map<std::string, std::shared_ptr<void>> entries;
};

/// Хост публикует после merge/remove (`AppLifecycleEvents::kControlTableUpdated`).
/// Движки/плагины с локальным кэшем указателя на control_table могут подписаться и обновить ссылку.
struct ControlTableUpdate {
    std::unordered_map<std::string, std::shared_ptr<void>>* table = nullptr;
    std::string sourceTrackerPath;
    std::vector<std::string> addedKeys;
    std::vector<std::string> removedKeys;
    /// control_table key → tracker display name (e.g. "OpenSeeFace")
    std::unordered_map<std::string, std::string> keySources;
};

/// Снимок нажатых клавиш и кнопок мыши (слот данных шины `keyboard_state`; читать под `mutex`).
/// Коды клавиш — Win32 VK (не Qt); toggles используют id вида `Vk_65` или `Mouse_Left`.
struct KeyboardKeysState {
    mutable std::mutex mutex;
#ifdef _WIN32
    std::unordered_set<int> keysHeldVk;
#endif
    std::unordered_set<std::string> mouseButtonsHeld;

    /// Старые сохранённые toggles могли использовать Qt Key enum в `Key_*` — переводим в VK.
    static int legacyKeyIdToVk(int legacyId) {
        if ((legacyId >= '0' && legacyId <= '9') || (legacyId >= 'A' && legacyId <= 'Z'))
            return legacyId;
        if (legacyId >= 0x01000030 && legacyId <= 0x01000030 + 23)
            return 0x70 + (legacyId - 0x01000030); // VK_F1 = 0x70
        switch (legacyId) {
        case 32:           return 0x20; // VK_SPACE
        case 0x01000000:   return 0x1B; // VK_ESCAPE
        case 0x01000004:   return 0x0D; // VK_RETURN
        case 0x01000001:   return 0x09; // VK_TAB
        case 0x01000003:   return 0x08; // VK_BACK
        case 0x01000007:   return 0x2E; // VK_DELETE
        case 0x01000006:   return 0x2D; // VK_INSERT
        case 0x01000010:   return 0x24; // VK_HOME
        case 0x01000011:   return 0x23; // VK_END
        case 0x01000016:   return 0x21; // VK_PRIOR
        case 0x01000017:   return 0x22; // VK_NEXT
        case 0x01000012:   return 0x25; // VK_LEFT
        case 0x01000014:   return 0x27; // VK_RIGHT
        case 0x01000013:   return 0x26; // VK_UP
        case 0x01000015:   return 0x28; // VK_DOWN
        case 0x01000020:   return 0x10; // VK_SHIFT
        case 0x01000021:   return 0x11; // VK_CONTROL
        case 0x01000023:   return 0x12; // VK_MENU
        default:           return 0;
        }
    }

    static bool vkHeld(int vk, const KeyboardKeysState& st) {
#ifdef _WIN32
        return vk != 0 && st.keysHeldVk.count(vk) > 0;
#else
        (void)vk; (void)st;
        return false;
#endif
    }

    /// Toggle input id: `Vk_<VK>` (preferred), legacy `Key_<Qt enum>` or `Mouse_Left`.
    static bool isInputActive(const std::string& inputId, const KeyboardKeysState& st)
    {
        if (inputId.rfind("Mouse_", 0) == 0) {
            const std::string btn = inputId.substr(6);
            return st.mouseButtonsHeld.count(btn) > 0;
        }
        if (inputId.rfind("Vk_", 0) == 0) {
            try {
                return vkHeld(std::stoi(inputId.substr(3)), st);
            } catch (...) {
                return false;
            }
        }
        if (inputId.rfind("Key_", 0) == 0) {
            try {
                return vkHeld(legacyKeyIdToVk(std::stoi(inputId.substr(4))), st);
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};

/// Описание деревьев интерфейса движка: путь DLL + идентификатор для AppMessage::sender + указатель на страницы.
struct PluginUiEngineTrees {
    std::string libraryPath;
    std::string pluginMessagingId;
    std::shared_ptr<std::vector<RUI::UiPage>> pages;
};

/// Описание вкладок интерфейса трекера (упорядоченный список страниц для одного плагина).
struct PluginUiTrackerTrees {
    std::string libraryPath;
    std::string pluginMessagingId;
    std::shared_ptr<std::vector<RUI::UiPage>> pages;
};


class BusHandleBase;
class IDataBus {
public:
    virtual void registerData(const std::string& key, std::any data) = 0;
    virtual std::any& getData(const std::string& key) = 0;
    virtual void remove(const std::string& key) = 0;

    /// Зарегистрировать «живой канал»: при clearLive указатели остаются валидными, чтение даёт локальный дефолт шины.
    virtual void registerBusHandle(const std::string& key, std::unique_ptr<BusHandleBase> handle) = 0;
    virtual BusHandleBase* tryBusHandle(const std::string& key) noexcept = 0;
    virtual void clearBusHandleLive(const std::string& key) noexcept = 0;

    /// Ключ чужой шины читается и пишется как свой. Тип значения остаётся в std::any, отдельный класс на каждый ключ не нужен.
    virtual void follow(IDataBus* /*upstream*/, const std::string& /*key*/, bool /*asHandle*/) {}
};

class ITracker {
public:
    ITracker() = default;
    ITracker(IEventManager* eventManager, IDataBus* dataBus) {}
    virtual ~ITracker() = default;
    /// Путь загруженной DLL; хост вызывает после create, до start (чтобы UI знал ключ для реестра вкладок).
    virtual void setLibraryPath(const std::string& /*path*/) {}
    /// Остановить потоки и снять с шины все указатели на память этого модуля до выгрузки DLL.
    virtual void shutdown() {}
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    /// When true, host auto-starts tracking after session restore (not on manual plugin load).
    virtual bool startTrackingOnStartup() const { return false; }
    virtual std::unordered_map<std::string, std::shared_ptr<void>>* getTable() const = 0;
};

struct EngineMeta {

    std::unordered_map<std::string, std::shared_ptr<void>>* table;
    IRenderer* renderer;
    uintptr_t windowHandle;

};

struct ViewportBus {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    double dpr = 1.0;

    bool isVisible = true;
};

class IModel {
public:
    IModel() = default;
    IModel(IEventManager* eventManager, IDataBus* dbus) {}
    virtual ~IModel() = default;
    virtual void shutdown() {}
    /// Путь загруженной DLL; вызывается хостом до test()/первой публикации UI.
    virtual void setLibraryPath(const std::string& /*path*/) {}
    virtual void test() = 0;
    virtual std::shared_ptr<std::vector<RUI::UiPage>> getUiPages() = 0;
    virtual void setDataBus(IDataBus* db) = 0;
    virtual void setMeta(EngineMeta meta) = 0;
    virtual void tick() = 0;
    virtual void initRender() = 0;
    virtual void update(ViewportBus) = 0;
};

class IGenPlugin {
public:
    IGenPlugin() = default;
    IGenPlugin(IEventManager* eventManager, IDataBus* dataBus) {}
    virtual ~IGenPlugin() = default;
    /// Путь пакета (.ofp) / модуля — для тегирования вкладок (свойство `plugin_library_path` на Qt-виджете) при init_ui_eng из плагина.
    virtual void setLibraryPath(const std::string& /*path*/) {}
    /// Снять привязки к шине (control_table и т.д.) до выгрузки DLL — иначе остаются висячие указатели.
    virtual void shutdown() {}
    virtual bool isActive() const { return true; }
    virtual void setDataBus(IDataBus* db) = 0;
    virtual std::string getName() = 0;
};

#if defined(_WIN32)
#  if defined(BUILD_DLL)
#    define DLL_EXPORT __declspec(dllexport)
#  else
#    define DLL_EXPORT __declspec(dllimport)
#  endif
#else
#  if defined(BUILD_DLL)
#    define DLL_EXPORT __attribute__((visibility("default")))
#  else
#    define DLL_EXPORT
#  endif
#endif

/// Плагин нового типа. События и данные приходят от менеджера плагинов, не от ядра.
/// `send` / `subscribe` прячут AppMessage. Страницы UI — абстрактные узлы, хост только рисует их.
class IPlugin {
private:
    IEventManager* _em;
    IDataBus* _db;
    std::string libraryPath_;
protected:
    std::string name;

    void send(const std::string& message, std::any data) {
        _em->sendMessage(AppMessage(name, message, std::move(data)));
    }

    void subscribe(const std::string& message, std::function<void(const std::any&)> fn) {
        _em->subscribe(name, message, std::move(fn));
    }

    template <typename C>
    void subscribe(const std::string& message, void (C::*method)(), C* instance) {
        _em->subscribe(name, message, method, instance);
    }

    template <typename T, typename C>
    void subscribe(const std::string& message, void (C::*method)(T), C* instance) {
        _em->subscribe(name, message, method, instance);
    }

    IDataBus* bus() const { return _db; }
    IEventManager* events() const { return _em; }

    std::any getVariable(const std::string& key) {
        return _db->getData(key);
    }

    void registerVariable(const std::string& key, std::any value) {
        _db->registerData(key, std::move(value));
    }

    /// Хост снимет страницы и повесит `onClick` на виджеты. Логика остаётся в плагине.
    void publishPages(std::shared_ptr<std::vector<RUI::UiPage>> pages) {
        PluginUiTrackerTrees submission;
        submission.libraryPath = libraryPath_;
        submission.pluginMessagingId = name;
        submission.pages = std::move(pages);
        send("plugin_ui_pages", std::move(submission));
    }

public:
    IPlugin(IEventManager* em, IDataBus* db, const std::string& n = "unnamed") : name(n), _em(em), _db(db) {}
    virtual ~IPlugin() = default;

    void setLibraryPath(const std::string& path) { libraryPath_ = path; }
    const std::string& libraryPath() const { return libraryPath_; }

    virtual void created() {}
    virtual void inited() {}
    virtual void start() {}
    virtual void update(float /*deltaTime*/) {}
    virtual void stop() {}
    virtual void cleared() {}
    virtual void destroyed() {}

    /// Часть интерфейса, не экспорт библиотеки. Выгрузка объекта в том модуле, где он создан.
    virtual void release() { delete this; }

    const std::string& getName() const { return name; }
};

#define IMPLEMENT_PLUGIN(ClassName) \
extern "C" { \
    DLL_EXPORT IPlugin* create(IEventManager* em, IDataBus* db) { \
        return new ClassName(em, db, #ClassName); \
    } \
}

using CreateEngine = IModel* (*)(IEventManager*, IDataBus*);
using DestroyEngine = void (*)(IModel*);
using CreateRenderer = IRenderer* (*)(void);
using CreateTracker = ITracker* (*)(IEventManager*, IDataBus*);
using DestroyTracker = void (*)(ITracker*);
using CreatePlugin = IGenPlugin* (*)(IEventManager*, IDataBus*);
using DestroyPlugin = void (*)(IGenPlugin*);
using CreateNewPlugin = IPlugin* (*)(IEventManager*, IDataBus*);
using DestroyNewPlugin = void (*)(IPlugin*);

struct LibMeta {
    std::string path;
    std::vector<std::string> func_names;
};

struct Meta {
    std::string path;
    std::vector<std::string> func_names;
    /// Пустой — резолвер отвечает в тему своего типа плагина.
    std::string respond;
};

struct EngineFuncs {
    CreateEngine ce;
    // ...
};

struct ViewportCommand {
    int mouseX = 0;
    int mouseY = 0;
    int scroll = 0;
    std::set<std::string> currentCommand {};
};

#define _DO_SUBSCRIBE(em, sender, msg, method_ptr, obj) \
(em).subscribe(sender, msg, [obj, method_ptr](const std::any& data) { \
        (obj->*method_ptr)(data); \
})

#define BIND_GROUP(obj, sender, em, ...) \
    do { \
        auto _bind_tuple = std::make_tuple(__VA_ARGS__); \
        std::apply([&](auto&&... pairs) { \
                (_DO_SUBSCRIBE(em, sender, \
                               std::get<0>(pairs), \
                               std::get<1>(pairs), \
                               obj), ...); \
        }, _bind_tuple); \
} while(0)

#endif // RCQAPI_H
