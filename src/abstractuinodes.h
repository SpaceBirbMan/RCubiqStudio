#ifndef ABSTRACTUINODES_H
#define ABSTRACTUINODES_H

#include <algorithm>
#include <ctime>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// TODO: Возможна ли тут система координат?

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
    std::string message;
};

inline std::string createName(const std::string& name)  {
    return name + std::to_string(std::time(nullptr));
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
protected:
    std::string name;
    const std::string basic_name = "UI_element";
    std::function<void()> onChange;
    std::weak_ptr<UiElement> parent;  // weak_ptr to avoid circular references

public:
    virtual ~UiElement() = default;

    UiElement() : name(createName(basic_name)) {}
    explicit UiElement(std::string name_) : name(std::move(name_)) {}

    UiElement(const UiElement& other)
        : name(other.name), basic_name(other.basic_name), onChange(other.onChange) {
        // parent is not copied - new element has no parent until explicitly set
    }

    UiElement(UiElement&& other) noexcept
        : name(std::move(other.name)), basic_name(other.basic_name),
        onChange(std::move(other.onChange)), parent(std::move(other.parent)) {
    }

    UiElement& operator=(const UiElement& other) {
        if (this != &other) {
            name = other.name;
            onChange = other.onChange;
            // parent not assigned
        }
        return *this;
    }

    UiElement& operator=(UiElement&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            onChange = std::move(other.onChange);
            parent = std::move(other.parent);
        }
        return *this;
    }

    void resetName() {
        name = createName(basic_name);
    }

    const std::string& getName() const { return name; }
    void setOnChange(std::function<void()> cb) { onChange = std::move(cb); }
    void setParent(std::shared_ptr<UiElement> p) { parent = p; }
    std::shared_ptr<UiElement> getParent() const { return parent.lock(); }
};

//////////////////////////////////////////////////////////
// Containers
//////////////////////////////////////////////////////////

class UiContainer : public UiElement {
private:
    const std::string basic_name = "container";
    std::vector<std::shared_ptr<UiElement>> children;

protected:
    CompositionType composition = VBOX;

public:
    UiContainer() = default;
    explicit UiContainer(std::string name_) : UiElement(std::move(name_)) {}

    UiContainer(std::string name_, std::shared_ptr<UiElement> parent_)
        : UiElement(std::move(name_)) {
        if (parent_) {
            parent = parent_;
        }
    }

    UiContainer(std::shared_ptr<UiElement> parent_) : UiContainer() {
        if (parent_) {
            parent = parent_;
        }
    }

    UiContainer(std::string name_, std::shared_ptr<UiElement> parent_,
                std::vector<std::shared_ptr<UiElement>> children_, CompositionType comp)
        : UiElement(std::move(name_)), children(std::move(children_)), composition(comp) {
        if (parent_) {
            parent = parent_;
        }
    }

    // Copy constructor
    UiContainer(const UiContainer& other) : UiElement(other) {
        composition = other.composition;
        children.reserve(other.children.size());
        for (const auto& child : other.children) {
            // Shallow copy of shared_ptr - both point to same object
            children.push_back(child);
        }
    }

    // Move constructor
    UiContainer(UiContainer&& other) noexcept : UiElement(std::move(other)) {
        composition = other.composition;
        children = std::move(other.children);
    }

    // Copy assignment
    UiContainer& operator=(const UiContainer& other) {
        if (this != &other) {
            UiElement::operator=(other);
            composition = other.composition;
            children = other.children;  // shallow copy
        }
        return *this;
    }

    // Move assignment
    UiContainer& operator=(UiContainer&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            composition = other.composition;
            children = std::move(other.children);
        }
        return *this;
    }

    void setChildren(const std::vector<std::shared_ptr<UiElement>>& ch) { children = ch; }
    const std::vector<std::shared_ptr<UiElement>>& getChildren() const { return children; }
    void setComposition(CompositionType comp) { composition = comp; }
    CompositionType getComposition() const { return composition; }

    void add(std::shared_ptr<UiElement> el) {
        if (el) {
            el->setParent(shared_from_this_safe());
            children.push_back(std::move(el));
        }
    }

    void add(const std::vector<std::shared_ptr<UiElement>>& els) {
        for (const auto& el : els) {
            add(el);
        }
    }

private:
    // Helper to safely get shared_ptr from this (if class is enabled_shared_from_this)
    std::shared_ptr<UiElement> shared_from_this_safe() {
        // Note: UiElement should inherit from std::enable_shared_from_this<UiElement>
        // for this to work properly. Adding minimal stub for now.
        return nullptr; // Placeholder - implement enable_shared_from_this if needed
    }
};

class UiScrollBox : public UiContainer {
private:
    SliderPolicy spH = IF_NEEDED;
    SliderPolicy spV = IF_NEEDED;

public:
    UiScrollBox() = default;
    explicit UiScrollBox(std::string name_) : UiContainer(std::move(name_)) {}

    UiScrollBox(SliderPolicy h, SliderPolicy v) : spH(h), spV(v) {}

    // Copy constructor
    UiScrollBox(const UiScrollBox& other) : UiContainer(other) {
        spH = other.spH;
        spV = other.spV;
    }

    // Move constructor
    UiScrollBox(UiScrollBox&& other) noexcept : UiContainer(std::move(other)) {
        spH = other.spH;
        spV = other.spV;
    }

    // Copy assignment
    UiScrollBox& operator=(const UiScrollBox& other) {
        if (this != &other) {
            UiContainer::operator=(other);
            spH = other.spH;
            spV = other.spV;
        }
        return *this;
    }

    // Move assignment
    UiScrollBox& operator=(UiScrollBox&& other) noexcept {
        if (this != &other) {
            UiContainer::operator=(std::move(other));
            spH = other.spH;
            spV = other.spV;
        }
        return *this;
    }

    void setSliderHPolicy(SliderPolicy p) { spH = p; }
    void setSliderVPolicy(SliderPolicy p) { spV = p; }
    void setSlidersPolicy(SliderPolicy h, SliderPolicy v) { spH = h; spV = v; }
    SliderPolicy getSliderHPolicy() const { return spH; }
    SliderPolicy getSliderVPolicy() const { return spV; }
};

// Forward declarations
class UiPageBox;
class UiPage;

class UiPageBox : public UiContainer {
private:
    std::vector<std::shared_ptr<UiPage>> pages;

public:
    UiPageBox() = default;
    explicit UiPageBox(std::string name_) : UiContainer(std::move(name_)) {}

    // Copy constructor
    UiPageBox(const UiPageBox& other) : UiContainer(other) {
        pages = other.pages;  // shallow copy of shared_ptrs
    }

    // Move constructor
    UiPageBox(UiPageBox&& other) noexcept : UiContainer(std::move(other)) {
        pages = std::move(other.pages);
    }

    // Copy assignment
    UiPageBox& operator=(const UiPageBox& other) {
        if (this != &other) {
            UiContainer::operator=(other);
            pages = other.pages;
        }
        return *this;
    }

    // Move assignment
    UiPageBox& operator=(UiPageBox&& other) noexcept {
        if (this != &other) {
            UiContainer::operator=(std::move(other));
            pages = std::move(other.pages);
        }
        return *this;
    }

    void add(std::shared_ptr<UiPage> pg) {
        if (pg) {
            pages.push_back(std::move(pg));
        }
    }

    void add(const std::vector<std::shared_ptr<UiPage>>& pgs) {
        for (const auto& pg : pgs) {
            add(pg);
        }
    }

    const std::vector<std::shared_ptr<UiPage>>& getPages() const { return pages; }
    std::shared_ptr<UiPage> getPageByIndex(unsigned char idx) const;
};

class UiMenuButton;

class UiPage : public UiContainer {
private:
    std::string title;
    unsigned char index = 0;
    std::weak_ptr<UiPageBox> pageBoxParent;

public:
    UiPage() { resetName(); title = name; }
    explicit UiPage(std::string name_) : UiContainer(std::move(name_)) { title = name; }
    UiPage(std::string name_, std::string title_) : UiContainer(std::move(name_)), title(std::move(title_)) {}

    // Copy constructor
    UiPage(const UiPage& other) : UiContainer(other), title(other.title), index(other.index) {
        // pageBoxParent is not copied
    }

    // Move constructor
    UiPage(UiPage&& other) noexcept : UiContainer(std::move(other)),
        title(std::move(other.title)),
        index(other.index),
        pageBoxParent(std::move(other.pageBoxParent)) {
    }

    // Copy assignment
    UiPage& operator=(const UiPage& other) {
        if (this != &other) {
            UiContainer::operator=(other);
            title = other.title;
            index = other.index;
        }
        return *this;
    }

    // Move assignment
    UiPage& operator=(UiPage&& other) noexcept {
        if (this != &other) {
            UiContainer::operator=(std::move(other));
            title = std::move(other.title);
            index = other.index;
            pageBoxParent = std::move(other.pageBoxParent);
        }
        return *this;
    }

    void setTitle(std::string t) { title = std::move(t); }
    void setIndex(unsigned char idx) {
        // Index validation should be done by UiPageBox, not here
        index = idx;
    }

    [[nodiscard]] unsigned char getIndex() const { return index; }
    [[nodiscard]] const std::string& getTitle() const { return title; }
    void setPageBoxParent(std::shared_ptr<UiPageBox> p) { pageBoxParent = p; }
    std::shared_ptr<UiPageBox> getPageBoxParent() const { return pageBoxParent.lock(); }
};

inline std::shared_ptr<UiPage> UiPageBox::getPageByIndex(unsigned char idx) const {
    for (const auto& page : pages) {
        if (page && page->getIndex() == idx) {
            return page;
        }
    }
    return nullptr;
}

class UiListView : public UiContainer {
private:
    std::vector<std::string> items;
    int selectedIndex = -1;
    std::function<void(int)> onSelect;

public:
    UiListView() = default;
    explicit UiListView(std::string name_) : UiContainer(std::move(name_)) {}

    // Copy constructor
    UiListView(const UiListView& other) : UiContainer(other),
        items(other.items),
        selectedIndex(other.selectedIndex),
        onSelect(other.onSelect) {}

    // Move constructor
    UiListView(UiListView&& other) noexcept : UiContainer(std::move(other)),
        items(std::move(other.items)),
        selectedIndex(other.selectedIndex),
        onSelect(std::move(other.onSelect)) {}

    // Copy assignment
    UiListView& operator=(const UiListView& other) {
        if (this != &other) {
            UiContainer::operator=(other);
            items = other.items;
            selectedIndex = other.selectedIndex;
            onSelect = other.onSelect;
        }
        return *this;
    }

    // Move assignment
    UiListView& operator=(UiListView&& other) noexcept {
        if (this != &other) {
            UiContainer::operator=(std::move(other));
            items = std::move(other.items);
            selectedIndex = other.selectedIndex;
            onSelect = std::move(other.onSelect);
        }
        return *this;
    }

    void setItems(std::vector<std::string> it) { items = std::move(it); }
    const std::vector<std::string>& getItems() const { return items; }
    void addItem(std::string item) { items.push_back(std::move(item)); }
    void setSelectedIndex(int idx) {
        if (idx >= 0 && idx < static_cast<int>(items.size())) {
            selectedIndex = idx;
            if (onSelect) onSelect(selectedIndex);
        }
    }
    int getSelectedIndex() const { return selectedIndex; }
    void setOnSelect(std::function<void(int)> cb) { onSelect = std::move(cb); }
};

class UiMenu : public UiElement {
private:
    std::vector<std::shared_ptr<UiMenuButton>> buttons;
    std::weak_ptr<UiElement> target;

public:
    UiMenu() = default;

    // FIX: Инициализация target в списке инициализации
    explicit UiMenu(std::shared_ptr<UiElement> t, std::string name_)
        : UiElement(std::move(name_)), target(t) {}

    UiMenu(std::shared_ptr<UiElement> t, std::string name_, std::vector<std::shared_ptr<UiMenuButton>> btns)
        : UiElement(std::move(name_)), target(t), buttons(std::move(btns)) {}

    // Copy constructor
    UiMenu(const UiMenu& other) : UiElement(other), target(other.target) {
        buttons.reserve(other.buttons.size());
        for (const auto& btn : other.buttons) {
            buttons.push_back(btn);
        }
    }

    // Move constructor
    UiMenu(UiMenu&& other) noexcept
        : UiElement(std::move(other)), target(std::move(other.target)), buttons(std::move(other.buttons)) {}

    // Copy assignment
    UiMenu& operator=(const UiMenu& other) {
        if (this != &other) {
            UiElement::operator=(other);
            target = other.target;
            buttons = other.buttons;
        }
        return *this;
    }

    // Move assignment
    UiMenu& operator=(UiMenu&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            target = std::move(other.target);
            buttons = std::move(other.buttons);
        }
        return *this;
    }

    void setTarget(std::shared_ptr<UiElement> t) { target = t; }
    std::weak_ptr<UiElement> getTarget() const { return target; }

    void addButton(std::shared_ptr<UiMenuButton> btn) {
        if (btn) buttons.push_back(std::move(btn));
    }

    const std::vector<std::shared_ptr<UiMenuButton>>& getButtons() const { return buttons; }
};

//////////////////////////////////////////////////////////
// Display elements
//////////////////////////////////////////////////////////

class UiTitle : public UiElement {
private:
    std::string text;
    TextFormat format = NORMAL;

public:
    UiTitle() = default;
    explicit UiTitle(std::string txt) : text(std::move(txt)) {}
    UiTitle(std::string txt, TextFormat tf) : text(std::move(txt)), format(tf) {}

    // Copy constructor
    UiTitle(const UiTitle& other) : UiElement(other), text(other.text), format(other.format) {}

    // Move constructor
    UiTitle(UiTitle&& other) noexcept : UiElement(std::move(other)),
        text(std::move(other.text)),
        format(other.format) {}

    // Copy assignment
    UiTitle& operator=(const UiTitle& other) {
        if (this != &other) {
            UiElement::operator=(other);
            text = other.text;
            format = other.format;
        }
        return *this;
    }

    // Move assignment
    UiTitle& operator=(UiTitle&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            text = std::move(other.text);
            format = other.format;
        }
        return *this;
    }

    void setFormat(TextFormat tf) { format = tf; }
    void setText(std::string txt) { text = std::move(txt); }
    [[nodiscard]] const std::string& getText() const { return text; }
    [[nodiscard]] TextFormat getFormat() const { return format; }
};

class UiProgressBar : public UiElement {
private:
    int minValue = 0;
    int maxValue = 100;
    int value = 0;
    ProgressBarOrientation orientation = HORIZONTAL;

public:
    UiProgressBar() = default;
    UiProgressBar(int min, int max, ProgressBarOrientation orient)
        : minValue(min), maxValue(max), orientation(orient) {
        if (min == max) {
            throw RuiException("Min and Max values must be different");
        }
    }

    // Copy constructor
    UiProgressBar(const UiProgressBar& other) : UiElement(other),
        minValue(other.minValue),
        maxValue(other.maxValue),
        value(other.value),
        orientation(other.orientation) {}

    // Move constructor
    UiProgressBar(UiProgressBar&& other) noexcept : UiElement(std::move(other)),
        minValue(other.minValue),
        maxValue(other.maxValue),
        value(other.value),
        orientation(other.orientation) {}

    // Copy assignment
    UiProgressBar& operator=(const UiProgressBar& other) {
        if (this != &other) {
            UiElement::operator=(other);
            minValue = other.minValue;
            maxValue = other.maxValue;
            value = other.value;
            orientation = other.orientation;
        }
        return *this;
    }

    // Move assignment
    UiProgressBar& operator=(UiProgressBar&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            minValue = other.minValue;
            maxValue = other.maxValue;
            value = other.value;
            orientation = other.orientation;
        }
        return *this;
    }

    void setMinValue(int v) {
        if (v == maxValue) {
            throw RuiException("Min and Max values must be different");
        }
        minValue = v;
    }

    void setMaxValue(int v) {
        if (v == minValue) {
            throw RuiException("Min and Max values must be different");
        }
        maxValue = v;
    }

    void setValue(int v) { value = v; }
    void setOrientation(ProgressBarOrientation orient) { orientation = orient; }

    [[nodiscard]] int getMinValue() const { return minValue; }
    [[nodiscard]] int getMaxValue() const { return maxValue; }
    [[nodiscard]] int getValue() const { return value; }
    [[nodiscard]] ProgressBarOrientation getOrientation() const { return orientation; }
};

class UiImageBox : public UiElement {
public:
    std::string imagePath;
    std::shared_ptr<ImageData> idata;
    bool has_image = false;

    std::function<void(const std::string&)> onImageSet;
    std::function<void()> onImageCleared;
    std::function<void()> onRequestImage;

    UiImageBox() = default;
    explicit UiImageBox(std::string name_) : UiElement(std::move(name_)) {}

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

    // Copy assignment
    UiImageBox& operator=(const UiImageBox& other) {
        if (this != &other) {
            UiElement::operator=(other);
            imagePath = other.imagePath;
            idata = other.idata;
            has_image = other.has_image;
            onImageSet = other.onImageSet;
            onImageCleared = other.onImageCleared;
            onRequestImage = other.onRequestImage;
        }
        return *this;
    }

    // Move assignment
    UiImageBox& operator=(UiImageBox&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            imagePath = std::move(other.imagePath);
            idata = std::move(other.idata);
            has_image = other.has_image;
            onImageSet = std::move(other.onImageSet);
            onImageCleared = std::move(other.onImageCleared);
            onRequestImage = std::move(other.onRequestImage);
            other.has_image = false;
        }
        return *this;
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

//////////////////////////////////////////////////////////
// Buttons
//////////////////////////////////////////////////////////

class UiButton : public UiElement {
public:
    std::string text;
    std::function<void()> onClick;

    UiButton() = default;
    explicit UiButton(std::string name_) : UiElement(std::move(name_)) {}
    explicit UiButton(std::string name_, std::string txt) : text(std::move(txt)), UiElement(std::move(name_)) {}
    UiButton(std::string txt, std::function<void()> cb) : text(std::move(txt)), onClick(std::move(cb)) {}

    // Copy constructor
    UiButton(const UiButton& other) : UiElement(other), text(other.text), onClick(other.onClick) {}

    // Move constructor
    UiButton(UiButton&& other) noexcept : UiElement(std::move(other)),
        text(std::move(other.text)),
        onClick(std::move(other.onClick)) {}

    // Copy assignment
    UiButton& operator=(const UiButton& other) {
        if (this != &other) {
            UiElement::operator=(other);
            text = other.text;
            onClick = other.onClick;
        }
        return *this;
    }

    // Move assignment
    UiButton& operator=(UiButton&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            text = std::move(other.text);
            onClick = std::move(other.onClick);
        }
        return *this;
    }

    void setText(std::string txt) { text = std::move(txt); }
    [[nodiscard]] const std::string& getText() const { return text; }
    void setOnClick(std::function<void()> cb) { onClick = std::move(cb); }
};

class UiToggleableButton : public UiButton {
public:
    bool active = false;
    std::function<void(bool)> onToggle;

    UiToggleableButton() = default;
    using UiButton::UiButton;  // inherit constructors

    // Copy constructor
    UiToggleableButton(const UiToggleableButton& other) : UiButton(other),
        active(other.active),
        onToggle(other.onToggle) {}

    // Move constructor
    UiToggleableButton(UiToggleableButton&& other) noexcept : UiButton(std::move(other)),
        active(other.active),
        onToggle(std::move(other.onToggle)) {}

    // Copy assignment
    UiToggleableButton& operator=(const UiToggleableButton& other) {
        if (this != &other) {
            UiButton::operator=(other);
            active = other.active;
            onToggle = other.onToggle;
        }
        return *this;
    }

    // Move assignment
    UiToggleableButton& operator=(UiToggleableButton&& other) noexcept {
        if (this != &other) {
            UiButton::operator=(std::move(other));
            active = other.active;
            onToggle = std::move(other.onToggle);
        }
        return *this;
    }

    void toggle() {
        active = !active;
        if (onToggle) onToggle(active);
    }
};

class UiCheckBox : public UiToggleableButton {
public:
    using UiToggleableButton::UiToggleableButton;
};

class UiMenuButton : public UiElement {
private:
    std::string title;
    std::function<void()> callback;

public:
    UiMenuButton() = default;
    UiMenuButton(std::string title_, std::function<void()> cb)
        : title(std::move(title_)), callback(std::move(cb)) {}

    // Copy constructor
    UiMenuButton(const UiMenuButton& other) : UiElement(other),
        title(other.title),
        callback(other.callback) {}

    // Move constructor
    UiMenuButton(UiMenuButton&& other) noexcept : UiElement(std::move(other)),
        title(std::move(other.title)),
        callback(std::move(other.callback)) {}

    // Copy assignment
    UiMenuButton& operator=(const UiMenuButton& other) {
        if (this != &other) {
            UiElement::operator=(other);
            title = other.title;
            callback = other.callback;
        }
        return *this;
    }

    // Move assignment
    UiMenuButton& operator=(UiMenuButton&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            title = std::move(other.title);
            callback = std::move(other.callback);
        }
        return *this;
    }

    void setTitle(std::string t) { title = std::move(t); }
    [[nodiscard]] const std::string& getTitle() const { return title; }
    void setCallback(std::function<void()> cb) { callback = std::move(cb); }
    void trigger() { if (callback) callback(); }
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
    bool isPercentMode = false;
    std::function<void(int)> onSlide;

    UiSlider() = default;
    UiSlider(int min, int max) : minValue(min), maxValue(max) {}

    // Copy constructor
    UiSlider(const UiSlider& other) : UiElement(other),
        minValue(other.minValue),
        maxValue(other.maxValue),
        value(other.value),
        isPercentMode(other.isPercentMode),
        onSlide(other.onSlide) {}

    // Move constructor
    UiSlider(UiSlider&& other) noexcept : UiElement(std::move(other)),
        minValue(other.minValue),
        maxValue(other.maxValue),
        value(other.value),
        isPercentMode(other.isPercentMode),
        onSlide(std::move(other.onSlide)) {}

    // Copy assignment
    UiSlider& operator=(const UiSlider& other) {
        if (this != &other) {
            UiElement::operator=(other);
            minValue = other.minValue;
            maxValue = other.maxValue;
            value = other.value;
            isPercentMode = other.isPercentMode;
            onSlide = other.onSlide;
        }
        return *this;
    }

    // Move assignment
    UiSlider& operator=(UiSlider&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            minValue = other.minValue;
            maxValue = other.maxValue;
            value = other.value;
            isPercentMode = other.isPercentMode;
            onSlide = std::move(other.onSlide);
        }
        return *this;
    }

    [[nodiscard]] int getMinValue() const { return minValue; }
    [[nodiscard]] int getMaxValue() const { return maxValue; }
    [[nodiscard]] int getValue() const { return value; }

    void setValue(int v) {
        value = std::clamp(v, minValue, maxValue);
        if (onSlide) onSlide(value);
    }
};

class UiDial : public UiSlider {
public:
    bool isFloat = false;
    using UiSlider::UiSlider;
};

class UiComboBox : public UiElement {
public:
    std::vector<std::string> items;
    int currentIndex = 0;
    ItemType type = TEXT;
    std::function<void(int)> onSelect;

    UiComboBox() = default;
    explicit UiComboBox(std::string name_) : UiElement(std::move(name_)) {}

    // Copy constructor
    UiComboBox(const UiComboBox& other) : UiElement(other),
        items(other.items),
        currentIndex(other.currentIndex),
        type(other.type),
        onSelect(other.onSelect) {}

    // Move constructor
    UiComboBox(UiComboBox&& other) noexcept : UiElement(std::move(other)),
        items(std::move(other.items)),
        currentIndex(other.currentIndex),
        type(other.type),
        onSelect(std::move(other.onSelect)) {}

    // Copy assignment
    UiComboBox& operator=(const UiComboBox& other) {
        if (this != &other) {
            UiElement::operator=(other);
            items = other.items;
            currentIndex = other.currentIndex;
            type = other.type;
            onSelect = other.onSelect;
        }
        return *this;
    }

    // Move assignment
    UiComboBox& operator=(UiComboBox&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            items = std::move(other.items);
            currentIndex = other.currentIndex;
            type = other.type;
            onSelect = std::move(other.onSelect);
        }
        return *this;
    }

    void setItems(std::vector<std::string> it) { items = std::move(it); }
    void addItem(std::string item) { items.push_back(std::move(item)); }
    void setSelectedIndex(int idx) {
        if (idx >= 0 && idx < static_cast<int>(items.size())) {
            currentIndex = idx;
            if (onSelect) onSelect(currentIndex);
        }
    }
    [[nodiscard]] int getSelectedIndex() const { return currentIndex; }
};

class UiInputField : public UiElement {
public:
    std::string hint;
    std::string value;
    TextType inputType = STRING;
    std::function<void(const std::string&)> onTextChanged;

    UiInputField() = default;
    explicit UiInputField(std::string name_) : UiElement(std::move(name_)) {}

    // Copy constructor
    UiInputField(const UiInputField& other) : UiElement(other),
        hint(other.hint),
        value(other.value),
        inputType(other.inputType),
        onTextChanged(other.onTextChanged) {}

    // Move constructor
    UiInputField(UiInputField&& other) noexcept : UiElement(std::move(other)),
        hint(std::move(other.hint)),
        value(std::move(other.value)),
        inputType(other.inputType),
        onTextChanged(std::move(other.onTextChanged)) {}

    // Copy assignment
    UiInputField& operator=(const UiInputField& other) {
        if (this != &other) {
            UiElement::operator=(other);
            hint = other.hint;
            value = other.value;
            inputType = other.inputType;
            onTextChanged = other.onTextChanged;
        }
        return *this;
    }

    // Move assignment
    UiInputField& operator=(UiInputField&& other) noexcept {
        if (this != &other) {
            UiElement::operator=(std::move(other));
            hint = std::move(other.hint);
            value = std::move(other.value);
            inputType = other.inputType;
            onTextChanged = std::move(other.onTextChanged);
        }
        return *this;
    }

    void setValue(std::string v) {
        value = std::move(v);
        if (onTextChanged) onTextChanged(value);
    }
    [[nodiscard]] const std::string& getValue() const { return value; }
};

class UiSpinField : public UiInputField {
public:
    int minValue = 0;
    int maxValue = 100;
    int intValue = 0;
    bool isFloat = false;
    bool negativable = false;
    std::function<void(int)> onValueChanged;

    UiSpinField() = default;
    using UiInputField::UiInputField;

    // Copy constructor
    UiSpinField(const UiSpinField& other) : UiInputField(other),
        minValue(other.minValue),
        maxValue(other.maxValue),
        intValue(other.intValue),
        isFloat(other.isFloat),
        negativable(other.negativable),
        onValueChanged(other.onValueChanged) {}

    // Move constructor
    UiSpinField(UiSpinField&& other) noexcept : UiInputField(std::move(other)),
        minValue(other.minValue),
        maxValue(other.maxValue),
        intValue(other.intValue),
        isFloat(other.isFloat),
        negativable(other.negativable),
        onValueChanged(std::move(other.onValueChanged)) {}

    // Copy assignment
    UiSpinField& operator=(const UiSpinField& other) {
        if (this != &other) {
            UiInputField::operator=(other);
            minValue = other.minValue;
            maxValue = other.maxValue;
            intValue = other.intValue;
            isFloat = other.isFloat;
            negativable = other.negativable;
            onValueChanged = other.onValueChanged;
        }
        return *this;
    }

    // Move assignment
    UiSpinField& operator=(UiSpinField&& other) noexcept {
        if (this != &other) {
            UiInputField::operator=(std::move(other));
            minValue = other.minValue;
            maxValue = other.maxValue;
            intValue = other.intValue;
            isFloat = other.isFloat;
            negativable = other.negativable;
            onValueChanged = std::move(other.onValueChanged);
        }
        return *this;
    }

    void setIntValue(int v) {
        intValue = std::clamp(v, minValue, maxValue);
        if (onValueChanged) onValueChanged(intValue);
    }
};

//////////////////////////////////////////////////////////
// Windows
//////////////////////////////////////////////////////////

class Window {
protected:
    std::string title;
    void* winId = nullptr;
    int width = 0;
    int height = 0;
    bool isModal = false;

public:
    Window() = default;
    Window(std::string title_, void* id, int w, int h, bool modal)
        : title(std::move(title_)), winId(id), width(w), height(h), isModal(modal) {}

    // Copy constructor
    Window(const Window& other)
        : title(other.title), winId(other.winId),
        width(other.width), height(other.height), isModal(other.isModal) {}

    // Move constructor
    Window(Window&& other) noexcept
        : title(std::move(other.title)), winId(other.winId),
        width(other.width), height(other.height), isModal(other.isModal) {
        other.winId = nullptr;
    }

    // Copy assignment
    Window& operator=(const Window& other) {
        if (this != &other) {
            title = other.title;
            winId = other.winId;
            width = other.width;
            height = other.height;
            isModal = other.isModal;
        }
        return *this;
    }

    // Move assignment
    Window& operator=(Window&& other) noexcept {
        if (this != &other) {
            title = std::move(other.title);
            winId = other.winId;
            width = other.width;
            height = other.height;
            isModal = other.isModal;
            other.winId = nullptr;
        }
        return *this;
    }

    virtual ~Window() = default;

    [[nodiscard]] const std::string& getTitle() const { return title; }
    [[nodiscard]] void* getWinId() const { return winId; }
    [[nodiscard]] int getWidth() const { return width; }
    [[nodiscard]] int getHeight() const { return height; }
    [[nodiscard]] bool isModalWindow() const { return isModal; }

    void setTitle(std::string t) { title = std::move(t); }
    void setWinId(void* id) { winId = id; }
    void setSize(int w, int h) { width = w; height = h; }
    void setModal(bool modal) { isModal = modal; }

    // Windows are typically shown via methods, not stored in UI tree
    virtual void show() = 0;
    virtual void hide() = 0;
};

class LoadWindow : public Window {
private:
    std::function<void(std::vector<std::string>)> onSubmit;

public:
    LoadWindow() = default;
    LoadWindow(std::string title_, void* id, int w, int h, bool modal,
               std::function<void(std::vector<std::string>)> cb)
        : Window(std::move(title_), id, w, h, modal), onSubmit(std::move(cb)) {}

    // Copy constructor
    LoadWindow(const LoadWindow& other) : Window(other), onSubmit(other.onSubmit) {}

    // Move constructor
    LoadWindow(LoadWindow&& other) noexcept : Window(std::move(other)),
        onSubmit(std::move(other.onSubmit)) {}

    // Copy assignment
    LoadWindow& operator=(const LoadWindow& other) {
        if (this != &other) {
            Window::operator=(other);
            onSubmit = other.onSubmit;
        }
        return *this;
    }

    // Move assignment
    LoadWindow& operator=(LoadWindow&& other) noexcept {
        if (this != &other) {
            Window::operator=(std::move(other));
            onSubmit = std::move(other.onSubmit);
        }
        return *this;
    }

    void show() override {
        // Platform-specific implementation would go here
        // For abstraction, this is a placeholder
    }

    void hide() override {
        // Platform-specific implementation
    }

    void triggerSubmit(const std::vector<std::string>& data) {
        if (onSubmit) onSubmit(data);
    }

    void setOnSubmit(std::function<void(std::vector<std::string>)> cb) {
        onSubmit = std::move(cb);
    }
};

} // namespace RUI

#endif // ABSTRACTUINODES_H
