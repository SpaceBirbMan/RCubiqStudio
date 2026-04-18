#ifndef ABSTRACTUINODES_H
#define ABSTRACTUINODES_H

#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

// TODO: Возможна ли тут система координат?

#ifndef IMAGEDATA_DEFINED
#define IMAGEDATA_DEFINED
struct ImageData {
    uint32_t* pixels = nullptr;  // RGBA8, владелец обязан освободить через delete[]
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t channels = 4;

    ImageData() = default;
    ImageData(uint32_t* p, uint16_t w, uint16_t h, uint8_t c = 4)
        : pixels(p), width(w), height(h), channels(c) {}

    ImageData(const ImageData&) = delete;
    ImageData& operator=(const ImageData&) = delete;
    ImageData(ImageData&& other) noexcept
        : pixels(other.pixels), width(other.width), height(other.height), channels(other.channels) {
        other.pixels = nullptr;
    }
    ImageData& operator=(ImageData&& other) noexcept {
        if (this != &other) {
            delete[] pixels;
            pixels = other.pixels;
            width = other.width; height = other.height; channels = other.channels;
            other.pixels = nullptr;
        }
        return *this;
    }
    ~ImageData() { delete[] pixels; }
};
#endif // IMAGEDATA_DEFINED


namespace RUI {

class RuiException: public std::exception
{
public:
    RuiException(const std::string& message): message{message}
    {}
    const char* what() const noexcept override
    {
        return message.c_str();
    }
private:
    std::string message;    // сообщение об ошибке
    // нужно выводить больше служебной инфы (имя класса исключателя, время и короче т.д.)
};

inline std::string createName(const std::string name)  {
    return name + std::to_string(std::time_t());
}

enum CompositionType {
    VBOX,
    HBOX,
    FREE,
};

enum SliderPolicy {
    IF_NEEDED,
    ALWAYS,
    NEVER
};

enum ItemType {
    TEXT,
    IMAGE,
    GROUP
};

enum ProgressBarOrientation {
    HORIZONTAL,
    VERTICAL
};

enum TextFormat {
    NORMAL,
    ITALIC,
    BOLD,
    UNDERLINE
};

enum TextType {
    STRING,
    INT,
    FLOAT,
    HIDDENTEXT
};

//////////////////////////////////////////////////////////
// Base UI nodes
//////////////////////////////////////////////////////////

class UiElement {
private:

protected:
    std::string name;
    const std::string basic_name = "UI_element";

    std::shared_ptr<UiElement> parrent;

public:
    std::function<void()> onChange; // Made public for dynamic UI updating
    std::string getName() const { return name; }
    void setName(const std::string& newName) { name = newName; }

    virtual ~UiElement() = default;

    void resetName() {
        name = createName(basic_name);
    }

};


//////////////////////////////////////////////////////////
// Containers
//////////////////////////////////////////////////////////

class UiContainer : public UiElement {
private:

    const std::string basic_name = "сontainer"; // базовое имя элемента, не изменяется
    std::vector<std::shared_ptr<UiElement>> childrens; // массив дочерних элементов

protected:

    CompositionType composition = VBOX; // способ визуального размещения элементов в контейнере

public:
    /// Создаёт пустой контейнер с инициализированным по умолчанию именем элемента
    UiContainer() {
        resetName();
    }

    /// Создаёт пустой контейнер с именем name
    UiContainer(std::string name) {
        this->name = name;
    }

    /// Создаёт пустой контейнер с именем name и присоединяет его в элемент parent
    UiContainer(std::string name, std::shared_ptr<UiElement> parent) {
        this->name = name;
        this->parrent = parent;
    }

    /// Создаёт пустой контейнер с инициализированным по умолчанию именем элемента и присоединяет его в элемент parent
    UiContainer(std::shared_ptr<UiElement> parent) {
        resetName();
        this->parrent = parent;
    }

    UiContainer(std::string name, std::shared_ptr<UiElement> parent, std::vector<std::shared_ptr<UiElement>> childrens, CompositionType composition) {
        this->name = name;
        this->parrent = parrent;
        this->childrens = childrens;
        this->composition = composition;
    }

    /// Создаёт контейнер и заполняет его данными из copy (name, parrent, onChange, childrens, composition)
    UiContainer(const UiContainer &copy) : UiContainer { copy.name, copy.parrent, copy.childrens, copy.composition} {
        this->onChange = copy.onChange;
    }

    /// Создаёт контейнер и заполняет его данными из moved (name, parrent, onChange, childrens, composition)
    UiContainer(UiContainer &&moved) noexcept {
        this->name = std::move(moved.name);
        this->parrent = std::move(moved.parrent);
        this->childrens = std::move(moved.childrens);
        this->onChange = std::move(moved.onChange);
    }

    void setChildrens(std::vector<std::shared_ptr<UiElement>> childrens) {
        this->childrens = childrens;
    }

    const std::vector<std::shared_ptr<UiElement>>& getChildrens() const {
        return this->childrens;
    }

    void setComposition(CompositionType composition) {
        this->composition = composition;
    }

    CompositionType getComposition() {
        return this->composition;
    }

    /// Добавляет в childrens элемент el
    void add(std::shared_ptr<UiElement> el) {
        childrens.push_back(std::move(el));
    }

    /// Добавляет в childrens несколько элементов els Использует vector для объединения элементов
    void add(const std::vector<std::shared_ptr<UiElement>>& els) {
        for (const auto& el : els) {
            childrens.push_back(el);
        }
    }
};

// class UiGroup : public UiContainer {
// public:
//

//     // sort type, сделать пустой метод, get|set
// };

class UiScrollBox : public UiContainer {
private:

    SliderPolicy spH = IF_NEEDED;
    SliderPolicy spV = IF_NEEDED;

public:

    /// Создаёт пустой контейнер с скролл-барами с инициализированным по умолчанию именем элемента
    UiScrollBox() = default;

    // UiSliderBox(SliderPolicy spH, SliderPolicy spV) {
    //     this->spH = spH;
    //     this->spV = spV;
    // }

    /// Создаёт копию контейнера с скролл-барами
    UiScrollBox(const UiScrollBox &copy) : UiContainer(copy) {
        this->spH = copy.spH;
        this->spV = copy.spV;
    }

    /// Создаёт копию контейнера с скролл-барами, оригинал очищается
    UiScrollBox(UiScrollBox &&moved) {
        this->spH = moved.spH;
        this->spV = moved.spV;
    }

    void setSliderHPolicy(SliderPolicy spH) {
        this->spH = spH;
    }

    void setSliderVPolicy(SliderPolicy spV) {
        this->spV = spV;
    }

    void setSlidersPolicy(SliderPolicy spH, SliderPolicy spV) {
        this->spH = spH;
        this->spV = spV;
    }

    SliderPolicy getSliderHPolicy() {
        return spH;
    }

    SliderPolicy getSliderVPolicy() {
        return spV;
    }
};

class UiPageBox; // forward declaration
class UiPage;
std::shared_ptr<UiElement> findByIndex(unsigned int index, std::vector<std::shared_ptr<UiPage>> array);

class UiPageBox : public UiContainer {

private:

    std::vector<std::shared_ptr<UiPage>> childrens {};

public:

    /// Создаёт пустой страничный контейнер с инициализированнымы по умолчанию значениями
    UiPageBox();

    /// Создаёт копию страничного контейнера copy
    UiPageBox(const UiPageBox& copy) : UiContainer(copy) {
        this->childrens = copy.childrens;
    }

    /// Создаёт копию страничного контейнера moved, moved инициализируется значениями по умолчанию
    UiPageBox(UiPageBox&& moved) : UiContainer(moved) {
        this->childrens = moved.childrens;
        moved.childrens = {};
    }

    /// Добавляет в childrens страницу pg
    void add(std::shared_ptr<UiPage> pg) {
        childrens.push_back(std::move(pg));
    }

    /// Добавляет в childrens несколько страниц pg. Использует vector для объединения элементов
    void add(const std::vector<std::shared_ptr<UiPage>>& pgs) {
        for (const auto& pg : pgs) {
            childrens.push_back(pg);
        }
    }

    std::vector<std::shared_ptr<UiPage>> getChildrens() {
        return this->childrens;
    }
};

class UiPage : public UiContainer {

private:

    std::string title;
    unsigned char index = 0;
    std::shared_ptr<UiPageBox> parrent;

public:

    /// Создаёт пустой контейнер-старницу с инициализированными по умолчанию значениями
    UiPage() {
        resetName();
        title = name;
    }

    /// Создаёт копию контейнер-страницы copy
    UiPage(const UiPage &copy) : UiContainer(copy) {
        this->title = copy.title;
    }

    /// Создаёт копию контейнер-страницы moved, moved инициализируется по умолчанию
    UiPage(UiPage &&moved) noexcept : UiContainer(std::move(moved)) {
        this->title = std::move(moved.title);
    }

    void setTitle(std::string title) {
        this->title = title;
    }

    void setIndex(char index) {
        if (findByIndex(index, parrent->getChildrens()) != nullptr) {
            throw RuiException("Index already exists");
            return;
        }
        this->index = index;
    }

    unsigned char getIndex() {
        return index;
    }

    std::string getTitle() {
        return title;
    }
};

/// Ищет элемент по индексу index в массиве array. Предназначен для UiPageBox
inline std::shared_ptr<UiElement> findByIndex(unsigned int index, std::vector<std::shared_ptr<UiPage>> array) {
    for (std::shared_ptr<UiPage> element : array) {
        if (element->getIndex() == index) {
            return element;
        }
    }
    return nullptr;
}



//////////////////////////////////////////////////////////
// Display elements
//////////////////////////////////////////////////////////

class UiTitle : public UiElement {

private:

    std::string text;
    TextFormat format = NORMAL;

public:

    UiTitle(std::string text) {
        this->text = text;
    }

    UiTitle(std::string text, TextFormat tf) {
        this->format = tf;
        this->text = text;
    }

    // дописать конструкторы копирования/переноса

    void setFormat(TextFormat tf) {
        this->format = tf;
    }

    void setText(std::string text) {
        this->text = text;
    }

    std::string getText() {
        return text;
    }

    TextFormat getFormat() {
        return format;
    }

};

/**
 * @brief The UiProgressBar class
 *
 * Пометка: ориентация прогрессбара определяется минимальным и максимальным значением. То есть можно сделать min > max и тогда он будет заполняться задом наперёд
 */
class UiProgressBar : public UiElement {
private:

    int minValue = 0;
    int maxValue = 100;

    int value = 0;

    ProgressBarOrientation orientation = HORIZONTAL;

public:

    /**
     * @brief UiProgressBar создаёт прогрессбар
     * @param minValue минимальное значение
     * @param maxValue максимальное значение
     * @param orientation ориентация прогрессбара (вертикально или горизонтально)
     */
    UiProgressBar(int minValue, int maxValue, bool is_inverted, ProgressBarOrientation orientation) {
        this->maxValue = maxValue;
        this->minValue = minValue;
        this->orientation = orientation;
    }

    void setMinValue(int v) {
        if (this->maxValue != v) {
            this->minValue = v;
        } else {
            throw RuiException("Min and Max values is the same");
        }
    }

    void setMaxValue(int v) {
        if (this->minValue != v) {
            this->maxValue = v;
        } else {
            throw RuiException("Min and Max values is the same");
        }
    }

    void setValue(int v) {
        this->value = v;
    }

    void setOrientation(ProgressBarOrientation orientation) {
        this->orientation = orientation;
    }

    int getMinValue() {
        return minValue;
    }

    int getMaxValue() {
        return maxValue;
    }

    int getValue() {
        return value;
    }

    ProgressBarOrientation getOrientation() {
        return orientation;
    }
};

// TODO: Дорефакторить

class UiImageBox : public UiElement {
private:
    std::string imagePath;
    std::shared_ptr<ImageData> idata;
    bool has_image = false;
public:


    std::function<void(const std::string&)> onImageSet;
    std::function<void()> onImageCleared;
    std::function<void()> onRequestImage;

    UiImageBox() = default;

    // Copy constructor
    UiImageBox(const UiImageBox& other) : UiElement(other),
        imagePath(other.imagePath),
        idata(other.idata),  // shallow copy
        has_image(other.has_image),
        onImageSet(other.onImageSet),
        onImageCleared(other.onImageCleared),
        onRequestImage(other.onRequestImage) {}

    // Move constructor
    UiImageBox(UiImageBox&& other) noexcept : UiElement(std::move(other)),
        imagePath(std::move(other.imagePath)),
        idata(std::move(other.idata)),
        has_image(other.has_image),
        onImageSet(std::move(other.onImageSet)),
        onImageCleared(std::move(other.onImageCleared)),
        onRequestImage(std::move(other.onRequestImage)) {
        other.has_image = false;
    }

    std::shared_ptr<ImageData> getImageData() const { return idata; }
    void setImageData(std::shared_ptr<ImageData> data) { idata = std::move(data); }

    void setImage(const std::string& path) {
        imagePath = path;
        has_image = !path.empty();
        if (onImageSet && has_image) onImageSet(path);
        if (onChange) onChange();
    }

    void clearImage() {
        imagePath.clear();  // Fixed: was imagePath = nullptr
        has_image = false;
        if (onImageCleared) onImageCleared();
        if (onChange) onChange();
    }

    [[nodiscard]] const std::string& getPath() const { return imagePath; }
    [[nodiscard]] bool hasImage() const { return has_image; }
};

class UiCanvas : public UiElement {
public:
    struct Point {
        float x;
        float y;
        uint32_t color = 0xFFFFFFFF; // RGBA format
        float size = 2.0f;
        std::string label;
    };

    std::vector<Point> points;
    std::shared_ptr<std::mutex> pointsMutex = std::make_shared<std::mutex>();
    std::function<void()> onCanvasUpdate;
    
    // Default constructor
    UiCanvas() { resetName(); }
};

//////////////////////////////////////////////////////////
// Buttons
//////////////////////////////////////////////////////////

class UiButton : public UiElement {
public:
    std::string text;
    std::function<void()> onClick;

    UiButton(std::string text) {
        this->text = text;
    }

    UiButton(std::string text, std::function<void()> onClick) {
        this->text = text;
        this->onClick = onClick;
    }
};

class UiToggleableButton : public UiButton {
public:
    bool active = false;
    std::function<void(bool)> onToggle;
};

class UiCheckBox : public UiToggleableButton {
public:
};

//////////////////////////////////////////////////////////
// Inputs
//////////////////////////////////////////////////////////

class UiSlider : public UiElement {

private:

    int minValue = 0;
    int maxValue = 100;
    int value = 0;

public:

    bool isPercentMode = false; // |--○--- 5%| ???
    std::function<void(int)> onSlide;

    void setMinValue(int val) { minValue = val; }
    void setMaxValue(int val) { maxValue = val; }
    void setValue(int val) { value = val; if(onChange) onChange(); }

    int getMinValue() {
        return minValue;
    }

    int getMaxValue() {
        return maxValue;
    }

    int getValue() {
        return value;
    }
};

class UiDial : public UiSlider {
public:
    bool isFloat = false;


};

class UiComboBox : public UiElement {
public:
    std::vector<std::string> items;
    int currentIndex = 0;
    ItemType type = TEXT;

    std::function<void(int)> onSelect;
};

class UiInputField : public UiElement {
public:
    std::string hint;
    std::string value;
    RUI::TextType inputType = STRING;

    std::function<void(const std::string&)> onTextChanged;
};

class UiSpinField : public UiInputField {
public:
    int minValue = 0;
    int maxValue = 100;
    int intValue = 0;

    bool isFloat = false;
    bool negativable = false;

    std::function<void(int)> onValueChanged;
};

// todo: inputbox для файлов, пружинки для более точного позиционирования (v, h) [el1] |////13px////| [el2]

//////////////////////////////////////////////////////////

class UiTreeNode : public UiElement {
public:
    std::string text;
    std::vector<std::shared_ptr<UiTreeNode>> children;
    std::function<void()> onSelect;

    UiTreeNode(std::string text) : text(std::move(text)) { resetName(); }
    void add(std::shared_ptr<UiTreeNode> node) { children.push_back(node); }
};

class UiTreeView : public UiElement {
public:
    std::vector<std::shared_ptr<UiTreeNode>> rootNodes;
    std::function<void(std::string)> onNodeSelected;
    
    void add(std::shared_ptr<UiTreeNode> node) { rootNodes.push_back(node); }
};

class UiListView : public UiElement {
public:
    std::vector<std::string> items;
    int selectedIndex = -1;
    std::function<void(int)> onSelect;
};

struct GridItem {
    std::string text;
    std::string imagePath;
};

class UiGridView : public UiElement {
public:
    std::vector<GridItem> items;
    std::function<void(int)> onSelect;
};

class UiWindow : public UiContainer {
public:
    std::string windowTitle;
    bool isVisible = false;
    std::function<void()> onClose;

    UiWindow(std::string title) : windowTitle(std::move(title)) { resetName(); }
    // Invoke window show/hide dynamically
    void showWindow() { isVisible = true; if(onChange) onChange(); }
    void hideWindow() { isVisible = false; if(onChange) onChange(); }
};

class UiFileDialog : public UiElement {
public:
    std::string filter = "All Files (*.*)";
    std::string title = "Select File";
    std::function<void(const std::string&)> onFileSelected;
};

class UiMenuButton : public UiButton {
public:
    UiMenuButton(std::string text, std::function<void()> onClick = nullptr) : UiButton(text, onClick) { resetName(); }
};

class UiContextMenu : public UiContainer {
public:
    std::shared_ptr<UiElement> target;

    UiContextMenu(std::shared_ptr<UiElement> targetElem = nullptr) : target(targetElem) { resetName(); }
    void setTarget(std::shared_ptr<UiElement> targetElem) { target = targetElem; }
};

// todo: Мб стоит подумать о вариантах передаваемых функций, как это сделано в
//       блоке маршрутизации сообщений

} // ns:RUI

#endif // ABSTRACTUINODES_H
