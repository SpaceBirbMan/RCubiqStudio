#ifndef ABSTRACTUINODES_H
#define ABSTRACTUINODES_H

#include <string>
#include <vector>
#include <memory>
#include <functional>

enum GroupSortType {
    VBox,
    HBox,
};

enum ItemType {
    Text,
    Image,
    Group
};

enum ProgressBarOrientation {
    Horizontal,
    HInverted,
    Vertical,
    VInverted
};

enum ProgressBarType {
    Positive,
    Negative,
    Bidirectional,
};

enum TextFormat {
    Normal,
    Italic,
    Bold,
    Underline
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

// todo: inputbox для файлов, пружинки для более точного позиционирования (v, h) [el1] |////13px////| [el2]

class UiElement {
public:
    std::string name;

    // единый сигнал обновления
    std::function<void()> onChange;

    virtual ~UiElement() = default;
};

//////////////////////////////////////////////////////////
// Containers
//////////////////////////////////////////////////////////

class UiContainer : public UiElement {
public:
    std::vector<std::shared_ptr<UiElement>> children;

    template<typename T, typename... Args>
    T* add(Args&&... args) {
        auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        children.push_back(std::move(ptr));
        return raw;
    }
};

class UiGroup : public UiContainer {
public:
    GroupSortType sort = VBox;
};

class UiSliderBox : public UiContainer {
public:
    GroupSortType sort = VBox;
};

class UiPageBox : public UiContainer {
};

class UiPage : public UiContainer {
public:
    std::string title;
    unsigned short index = 0;
};

class UiTreeView : public UiContainer {
public:

};

//////////////////////////////////////////////////////////
// Display elements
//////////////////////////////////////////////////////////

class UiTitle : public UiElement {
public:
    std::string text;
    TextFormat format = Normal;

    UiTitle(std::string text) {
        this->text = text;
    }

};

class UiProgressBar : public UiElement {
public:
    ProgressBarOrientation orientation = Horizontal;
    ProgressBarType type = Positive;

    int minValue = 0;
    int maxValue = 100;
    int value = 0;
};

class UiImageBox : public UiElement {
public:
    std::string imagePath; // путь к изображению или URI
    bool hasImage = false;

    // Опциональные коллбэки
    std::function<void(const std::string&)> onImageSet;   // вызывается при установке изображения
    std::function<void()> onImageCleared;                 // при очистке
    std::function<void()> onRequestImage;                 // триггер для открытия диалога/DnD

    // Методы для внешнего управления (опционально)
    void setImage(const std::string& path) {
        imagePath = path;
        hasImage = !path.empty();
        if (onImageSet && hasImage) onImageSet(path);
        if (onChange) onChange();
    }

    void clearImage() {
        imagePath.clear();
        hasImage = false;
        if (onImageCleared) onImageCleared();
        if (onChange) onChange();
    }
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
public:
    int minValue = 0;
    int maxValue = 100;
    int value = 0;

    bool isPercentMode = false; // |--○--- 5%|
    std::function<void(int)> onSlide;
};

class UiDial : public UiSlider {
public:
    bool isFloat = false;
};

class UiComboBox : public UiElement {
public:
    std::vector<std::string> items;
    int currentIndex = 0;
    ItemType type = Text;

    std::function<void(int)> onSelect;
};

class UiInputField : public UiElement {
public:
    std::string hint;
    std::string value;
    TextType inputType = STRING;

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

//////////////////////////////////////////////////////////


// todo: Мб стоит подумать о вариантах передаваемых функций, как это сделано в
//       блоке маршрутизации сообщений

#endif // ABSTRACTUINODES_H
