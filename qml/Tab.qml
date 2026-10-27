// Tab.qml
// Декларативная вкладка для TabWidget:
//   TabWidget { Tab { title: "1"; <content...> } Tab { title: "2"; ... } }
import QtQuick

QtObject {
    id: root

    /// Заголовок вкладки.
    property string title: ""

    /// Контент. По умолчанию принимает декларативные дети (Item / Layout).
    /// Используем list<Item>, чтобы можно было передавать сразу несколько верхнеуровневых элементов
    /// (TabWidget сам положит их в свой StackLayout-хост).
    default property list<Item> content
}
