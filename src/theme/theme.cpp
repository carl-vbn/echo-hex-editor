#include "theme.h"
#include <QComboBox>
#include <QListView>
#include <QStyledItemDelegate>

// Qt's default combo box delegate on some platforms ignores stylesheet
// ::item padding entirely when calculating row heights. This delegate
// extends QStyledItemDelegate (which enables stylesheet ::item pseudo-state
// styling) and adds vertical padding to each item's size hint, since
// stylesheet padding/min-height properties have no effect on layout.
namespace {
class PaddedItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(s.height() + 8); // 4px top + 4px bottom
        return s;
    }
};
}

QString Theme::appStyleSheet(const QString &fontFamily)
{
    // language=CSS
    return QString(R"(

* {
    font-family: "%1";
    font-size: 9pt;
    outline: none;
}

QWidget {
    background-color: #080808;
    color: #B0B0B0;
    border: none;
    border-radius: 0px;
}

/* Menu bar */

QMenuBar {
    background-color: #111111;
    color: #B0B0B0;
    border-bottom: 1px solid #181818;
    padding: 0px;
    spacing: 0px;
}
QMenuBar::item {
    background: transparent;
    padding: 3px 10px;
    margin: 0px;
}
QMenuBar::item:selected {
    background: #1C1C1C;
    color: #FFFFFF;
}
QMenuBar::item:pressed {
    background: #242424;
    color: #FFFFFF;
}

/* Menu dropdowns */

QMenu {
    background-color: #0C0C0C;
    border: 1px solid #282828;
    color: #B0B0B0;
    padding: 2px 0px;
}
QMenu::item {
    padding: 4px 28px 4px 12px;
    background: transparent;
}
QMenu::item:selected {
    background: #1C1C1C;
    color: #FFFFFF;
}
QMenu::item:disabled {
    color: #363636;
}
QMenu::separator {
    height: 1px;
    background: #1C1C1C;
    margin: 3px 0px;
}

/* Combo dropdowns */

QComboBox {
    background: #111111;
    color: #B0B0B0;
    border: 1px solid #282828;
    padding: 1px 4px;
    font-size: 8pt;
}
QComboBox QAbstractItemView {
    background: #111111;
    color: #B0B0B0;
    border: 1px solid #282828;
    padding: 0px;
    outline: none;
}
QComboBox QAbstractItemView::item {
    padding: 0px 8px;
    background: #111111;
    color: #B0B0B0;
}
QComboBox QAbstractItemView::item:hover,
QComboBox QAbstractItemView::item:selected {
    background: #1C1C1C;
    color: #FFFFFF;
}
QComboBox:disabled {
    color: #363636;
    border-color: #141414;
}

/* Splitter */

QSplitter {
    background: #080808;
}
QSplitter::handle:horizontal {
    width: 1px;
    background: #181818;
}
QSplitter::handle:vertical {
    height: 1px;
    background: #181818;
}

/* Tree widget */

QTreeWidget {
    background-color: #0C0C0C;
    alternate-background-color: #0E0E0E;
    color: #B0B0B0;
    border: none;
    selection-background-color: transparent;
    selection-color: #FFFFFF;
    show-decoration-selected: 1;
}
QTreeWidget::item {
    padding: 2px 4px;
    border: none;
}
QTreeWidget::item:hover {
    background: #141414;
}
QTreeWidget::item:selected {
    background: #1A1A1A;
    color: #FFFFFF;
}
QTreeWidget::branch {
    background: none;
}
QHeaderView {
    background: #111111;
    border: none;
    border-bottom: 1px solid #181818;
}
QHeaderView::section {
    background: #111111;
    color: #424242;
    border: none;
    border-right: 1px solid #181818;
    padding: 3px 6px;
    font-size: 7pt;
}
QHeaderView::section:last {
    border-right: none;
}

/* Scroll bars */

QScrollBar:vertical {
    background: #0C0C0C;
    width: 6px;
    margin: 0px;
}
QScrollBar::handle:vertical {
    background: #242424;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover {
    background: #303030;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0px;
}
QScrollBar:horizontal {
    background: #0C0C0C;
    height: 6px;
    margin: 0px;
}
QScrollBar::handle:horizontal {
    background: #242424;
    min-width: 24px;
}
QScrollBar::handle:horizontal:hover {
    background: #303030;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    width: 0px;
}

/* Labels */

QLabel {
    background: transparent;
    color: #B0B0B0;
    border: none;
}

/* Push buttons */

QPushButton {
    background: transparent;
    color: #545454;
    border: none;
    padding: 4px 10px;
}
QPushButton:hover {
    background: #181818;
    color: #B0B0B0;
}
QPushButton:checked {
    color: #FFFFFF;
}
QPushButton:pressed {
    background: #202020;
}

    )").arg(fontFamily);
}

// Prepares a QComboBox so its dropdown respects the app stylesheet.
// Must be called on every combo box because Qt's default platform delegate
// ignores stylesheet ::item rules. This function:
//  1. Sets PaddedItemDelegate so ::item hover/selected styles work and
//     items have proper height.
//  2. Enables uniform item sizes so the list view lays out rows using
//     the delegate's size hint (otherwise rows overlap).
//  3. Styles the popup container directly to remove its default frame
//     background, which otherwise shows as visible lines around the items.
void Theme::polishComboBox(QComboBox *combo)
{
    combo->setItemDelegate(new PaddedItemDelegate(combo));

    if (auto *listView = qobject_cast<QListView *>(combo->view()))
        listView->setUniformItemSizes(true);

    if (auto *container = combo->view()->parentWidget()) {
        container->setObjectName("comboPopup");
        container->setStyleSheet(QString(
            "#comboPopup { background: %1; border: none; padding: 0px; }")
            .arg(Color::BG_HEADER));
    }
}
