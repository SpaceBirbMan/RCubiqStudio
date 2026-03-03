#ifndef ABSTRACTUINODES_H
#define ABSTRACTUINODES_H

#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// TODO: Возможна ли тут система координат?

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

    // единый сигнал обновления
    std::function<void()> onChange;

    std::shared_ptr<UiElement> parrent;

public:

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
        this->parrent = parrent;
    }

    /// Создаёт пустой контейнер с инициализированным по умолчанию именем элемента и присоединяет его в элемент parent
    UiContainer(std::shared_ptr<UiElement> parent) {
        resetName();
        this->parrent = parrent;
    }

    UiContainer(std::string name, std::shared_ptr<UiElement> parent, std::vector<std::shared_ptr<UiElement>> childrens, CompositionType composition) {
        this->name = name;
        this->parrent = parrent;
        this->childrens = childrens;
        this->composition = composition;
    }

    /// Создаёт контейнер и заполняет его данными из copy (name, parrent, onChange, childrens, composition)
    UiContainer(const UiContainer &copy) : UiContainer { copy.name, copy.parrent, copy.childrens, copy.composition} {
    }

    /// Создаёт контейнер и заполняет его данными из moved (name, parrent, onChange, childrens, composition)
    UiContainer(UiContainer &&moved) {
        this->name = moved.name;
        this->parrent = moved.parrent;
        this->childrens = moved.childrens;
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
    UiPage(UiPage &&moved) : UiContainer(moved) {
        this->title = moved.title;
        std::string tmp = this->name;
        resetName();
        moved.title = name;
        this->name = name;
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

class UiTreeView : public UiContainer {
public:
    // конструктор
    // поле дерева (указатель)
    //
};

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
public:
    const char* imagePath;

    const uint32_t* pixels = nullptr;

    bool hasImage = false;

    std::function<void(const std::string&)> onImageSet;
    std::function<void()> onImageCleared;
    std::function<void()> onRequestImage;

    const uint32_t* getPixels() {
        return this->pixels;
    }

    void setPixels(const uint32_t* pixptr) {
        this->pixels = pixptr;
    }

    void setImage(const std::string& path) { // setPath
        imagePath = path.c_str();
        hasImage = !path.empty();
        std::cout << "IMAGEIN: " << imagePath << std::endl;
        if (onImageSet && hasImage) onImageSet(path);
        if (onChange) onChange();
    }

    void clearImage() {
        imagePath = nullptr;
        hasImage = false;
        if (onImageCleared) onImageCleared();
        if (onChange) onChange();
    }

    std::string getPath() {
        return imagePath;
    }

    const char* getConstPath() {
        return imagePath;
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

private:

    int minValue = 0;
    int maxValue = 100;
    int value = 0;

public:

    bool isPercentMode = false; // |--○--- 5%| ???
    std::function<void(int)> onSlide;

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


// todo: Мб стоит подумать о вариантах передаваемых функций, как это сделано в
//       блоке маршрутизации сообщений

} // ns:RUI

#endif // ABSTRACTUINODES_H
