#include "leftpanel.h"
#include "core/node.h"
#include "core/nodemodel.h"
#include "core/document.h"
#include "theme/theme.h"

#include <cstring>  // memcpy

static QString interpretNodeValue(Node *node, Document *doc);  // defined below

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QButtonGroup>
#include <QPushButton>
#include <QGridLayout>
#include <QPixmap>
#include <QColorDialog>
#include <QMenu>
#include <QContextMenuEvent>

// ---------------------------------------------------------------------------
// Panel header
// ---------------------------------------------------------------------------

QWidget *LeftPanel::makePanelHeader(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setFixedHeight(20);
    lbl->setContentsMargins(8, 0, 8, 0);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    lbl->setStyleSheet(QString(
        "background: %1;"
        "color: %2;"
        "border-bottom: 1px solid %3;"
        "font-size: 7pt;"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::TEXT_LABEL, Theme::Color::BORDER));
    return lbl;
}

// ---------------------------------------------------------------------------
// Construction / UI
// ---------------------------------------------------------------------------

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void LeftPanel::setupUi()
{
    setObjectName("LeftPanel");
    setStyleSheet(QString(
        "QWidget#LeftPanel { background: %1; border-right: 1px solid %2; }"
    ).arg(Theme::Color::BG_PANEL, Theme::Color::BORDER));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    // -----------------------------------------------------------------------
    // Top: node tree
    // -----------------------------------------------------------------------

    auto *treeContainer = new QWidget;
    treeContainer->setStyleSheet("background: transparent;");
    auto *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    treeLayout->setSpacing(0);

    treeLayout->addWidget(makePanelHeader("NODE HIERARCHY"));

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"NAME", "TYPE", "SIZE"});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setDefaultSectionSize(60);
    m_tree->setColumnWidth(0, 110);
    m_tree->setColumnWidth(1, 54);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &LeftPanel::onTreeSelectionChanged);

    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        QTreeWidgetItem *item = m_tree->itemAt(pos);
        if (!item) return;

        const quint64 id = item->data(0, Qt::UserRole).toULongLong();
        Node *node = m_nodeModel ? m_nodeModel->nodeById(id) : nullptr;
        if (!node || node->isRoot()) return;

        QMenu menu(this);
        QAction *deleteAct = menu.addAction("Delete Node");
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == deleteAct)
            m_nodeModel->removeNode(node);
    });

    treeLayout->addWidget(m_tree);

    // -----------------------------------------------------------------------
    // Bottom: node details
    // -----------------------------------------------------------------------

    auto *detailsContainer = new QWidget;
    detailsContainer->setStyleSheet("background: transparent;");
    auto *detailsLayout = new QVBoxLayout(detailsContainer);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(0);

    detailsLayout->addWidget(makePanelHeader("NODE DETAILS"));

    auto *fields = new QWidget;
    fields->setStyleSheet("background: transparent;");
    auto *grid = new QGridLayout(fields);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);

    const QString labelStyle = QString("color: %1; font-size: 8pt;")
                                   .arg(Theme::Color::TEXT_LABEL);
    const QString valueStyle = QString("color: %1; font-size: 8pt;")
                                   .arg(Theme::Color::TEXT_DIM);

    auto makeLabel = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setStyleSheet(labelStyle);
        return l;
    };

    int row = 0;

    // NAME
    grid->addWidget(makeLabel("NAME"), row, 0);
    m_nameEdit = new QLineEdit;
    m_nameEdit->setStyleSheet(QString(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " padding: 1px 4px; font-size: 8pt; }"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::TEXT, Theme::Color::BORDER));
    m_nameEdit->setEnabled(false);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, [this] {
        if (m_updating || !m_selected || !m_nodeModel) return;
        const QString name = m_nameEdit->text().trimmed();
        if (!name.isEmpty())
            m_nodeModel->renameNode(m_selected, name);
    });
    grid->addWidget(m_nameEdit, row++, 1);

    // TYPE
    grid->addWidget(makeLabel("TYPE"), row, 0);
    m_typeCombo = new QComboBox;
    m_typeCombo->addItems(Node::allTypeNames());
    m_typeCombo->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3;"
        " padding: 1px 4px; font-size: 8pt; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %2; border: 1px solid %4;"
        "  selection-background-color: #152638; selection-color: #D8D8D8;"
        "  outline: none; }"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::TEXT,
          Theme::Color::BORDER, Theme::Color::BORDER_MID));
    m_typeCombo->setEnabled(false);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (m_updating || !m_selected || !m_nodeModel) return;
        if (!m_selected->isLeaf() || m_selected->isRoot()) return;
        m_nodeModel->retypeNode(m_selected,
            Node::typeFromName(m_typeCombo->itemText(index)));
    });
    grid->addWidget(m_typeCombo, row++, 1);

    // OFFSET
    grid->addWidget(makeLabel("OFFSET"), row, 0);
    m_offsetLabel = new QLabel("\xe2\x80\x94");
    m_offsetLabel->setStyleSheet(valueStyle);
    m_offsetLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_offsetLabel, row++, 1);

    // LENGTH
    grid->addWidget(makeLabel("LENGTH"), row, 0);
    m_lengthLabel = new QLabel("\xe2\x80\x94");
    m_lengthLabel->setStyleSheet(valueStyle);
    m_lengthLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_lengthLabel, row++, 1);

    // VALUE
    grid->addWidget(makeLabel("VALUE"), row, 0);
    m_valueLabel = new QLabel("\xe2\x80\x94");
    m_valueLabel->setStyleSheet(valueStyle);
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(m_valueLabel, row++, 1);

    // ENDIAN
    grid->addWidget(makeLabel("ENDIAN"), row, 0);
    {
        const QString endianBtnStyle = QString(R"(
            QPushButton {
                background: transparent; color: %1;
                border: 1px solid %2; padding: 0px 6px; font-size: 7pt;
            }
            QPushButton:hover   { color: %3; border-color: %4; }
            QPushButton:checked { color: %3; border-color: %4; background: %5; }
        )").arg(Theme::Color::TEXT_DIM, Theme::Color::BORDER,
                Theme::Color::TEXT, Theme::Color::BORDER_MID,
                Theme::Color::BG_ACTIVE);

        auto *leBtn = new QPushButton("LE");
        auto *beBtn = new QPushButton("BE");
        leBtn->setCheckable(true);
        beBtn->setCheckable(true);
        leBtn->setChecked(true);
        leBtn->setStyleSheet(endianBtnStyle);
        beBtn->setStyleSheet(endianBtnStyle);
        leBtn->setFixedHeight(18);
        beBtn->setFixedHeight(18);

        m_endianGroup = new QButtonGroup(this);
        m_endianGroup->setExclusive(true);
        m_endianGroup->addButton(leBtn, 0);  // 0 = LE
        m_endianGroup->addButton(beBtn, 1);  // 1 = BE

        connect(m_endianGroup, &QButtonGroup::idClicked, this, [this](int id) {
            if (m_updating || !m_selected || !m_nodeModel) return;
            m_nodeModel->setEndian(m_selected, id == 0);
        });

        auto *endianWidget = new QWidget;
        endianWidget->setStyleSheet("background: transparent;");
        auto *endianLayout = new QHBoxLayout(endianWidget);
        endianLayout->setContentsMargins(0, 0, 0, 0);
        endianLayout->setSpacing(4);
        endianLayout->addWidget(leBtn);
        endianLayout->addWidget(beBtn);
        endianLayout->addStretch();
        grid->addWidget(endianWidget, row++, 1);
    }

    // COLOR
    grid->addWidget(makeLabel("COLOR"), row, 0);
    m_colorBtn = new QPushButton("Set...");
    m_colorBtn->setFixedHeight(18);
    m_colorBtn->setEnabled(false);
    m_colorBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_colorBtn, &QPushButton::clicked, this, [this] {
        if (!m_selected || !m_nodeModel) return;
        QColor initial = m_selected->color().isValid() ? m_selected->color() : Qt::white;
        QColor color = QColorDialog::getColor(initial, this, "Node Color");
        if (color.isValid()) {
            m_nodeModel->recolorNode(m_selected, color);
        }
    });
    connect(m_colorBtn, &QPushButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!m_selected || !m_nodeModel) return;
        QMenu menu(this);
        QAction *clearAct = menu.addAction("Clear color");
        if (menu.exec(m_colorBtn->mapToGlobal(pos)) == clearAct) {
            m_nodeModel->recolorNode(m_selected, QColor());
        }
    });
    grid->addWidget(m_colorBtn, row++, 1);

    detailsLayout->addWidget(fields);

    // SELECT BYTES button
    m_selectBytesBtn = new QPushButton("Select Bytes");
    m_selectBytesBtn->setFixedHeight(22);
    m_selectBytesBtn->setEnabled(false);
    m_selectBytesBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent; color: %1;
            border: none; font-size: 8pt;
        }
        QPushButton:hover   { background: %2; color: %3; }
        QPushButton:pressed { background: %4; }
    )").arg(
        Theme::Color::TEXT_DIM, Theme::Color::BG_HOVER,
        Theme::Color::TEXT,     Theme::Color::BG_ACTIVE));
    connect(m_selectBytesBtn, &QPushButton::clicked, this, [this] {
        if (!m_selected) return;
        const qint64 start = m_selected->absoluteStart();
        const qint64 end   = start + m_selected->length() - 1;
        emit selectBytesRequested(start, end);
    });

    detailsLayout->addWidget(m_selectBytesBtn);
    detailsLayout->addStretch();

    // -----------------------------------------------------------------------
    splitter->addWidget(treeContainer);
    splitter->addWidget(detailsContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    outerLayout->addWidget(splitter);
}

// ---------------------------------------------------------------------------
// NodeModel wiring
// ---------------------------------------------------------------------------

void LeftPanel::setNodeModel(NodeModel *model)
{
    if (m_nodeModel)
        disconnect(m_nodeModel, nullptr, this, nullptr);

    m_nodeModel = model;

    if (m_nodeModel) {
        connect(m_nodeModel, &NodeModel::nodeCreated, this, &LeftPanel::onNodeCreated);
        connect(m_nodeModel, &NodeModel::nodeRemoved, this, &LeftPanel::onNodeRemoved);
        connect(m_nodeModel, &NodeModel::nodeChanged, this, &LeftPanel::onNodeChanged);
        connect(m_nodeModel, &NodeModel::modelReset,  this, &LeftPanel::onModelReset);
    }

    rebuildTree();
}

void LeftPanel::setDocument(Document *doc)
{
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);
    m_document = doc;
    if (m_document) {
        connect(m_document, &Document::dataChanged, this, [this](qint64, qint64) {
            if (m_selected) {
                const QString val = interpretNodeValue(m_selected, m_document);
                m_valueLabel->setText(val.isEmpty() ? "\xe2\x80\x94" : val);
            }
        });
    }
}

void LeftPanel::selectAndEditNode(Node *node)
{
    if (!node) return;
    QTreeWidgetItem *item = node->isRoot()
        ? m_tree->topLevelItem(0)
        : findItemById(node->id());
    if (!item) return;

    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);

    // Put focus on the name field so the user can rename immediately
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

// ---------------------------------------------------------------------------
// Tree management
// ---------------------------------------------------------------------------

void LeftPanel::rebuildTree()
{
    m_tree->clear();
    m_selected = nullptr;
    showDetails(nullptr);

    if (!m_nodeModel || !m_nodeModel->root()) return;

    auto *invisRoot = m_tree->invisibleRootItem();
    addNodeItem(invisRoot, m_nodeModel->root());
    m_tree->topLevelItem(0)->setExpanded(true);
}

void LeftPanel::addNodeItem(QTreeWidgetItem *parentItem, Node *node)
{
    auto *item = new QTreeWidgetItem(parentItem);
    item->setData(0, Qt::UserRole, QVariant::fromValue(node->id()));
    updateItemDisplay(item, node);

    for (Node *child : node->children())
        addNodeItem(item, child);
}

void LeftPanel::updateItemDisplay(QTreeWidgetItem *item, Node *node)
{
    item->setText(0, node->name());
    item->setText(1, node->isLeaf() && !node->isRoot()
                      ? Node::typeName(node->type()) : QString());
    item->setText(2, QString::number(node->length()));

    if (node->color().isValid()) {
        QPixmap pix(10, 10);
        pix.fill(node->color());
        item->setData(0, Qt::DecorationRole, pix);
    } else {
        item->setData(0, Qt::DecorationRole, QVariant());
    }
}

QTreeWidgetItem *LeftPanel::findItemById(quint64 id) const
{
    return findItemByIdImpl(m_tree->invisibleRootItem(), id);
}

QTreeWidgetItem *LeftPanel::findItemByIdImpl(QTreeWidgetItem *item, quint64 id) const
{
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem *child = item->child(i);
        if (child->data(0, Qt::UserRole).toULongLong() == id)
            return child;
        QTreeWidgetItem *result = findItemByIdImpl(child, id);
        if (result) return result;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// NodeModel slots
// ---------------------------------------------------------------------------

void LeftPanel::onNodeCreated(Node *node)
{
    Node *parent = node->parent();
    if (!parent) return;

    QTreeWidgetItem *parentItem = parent->isRoot()
        ? m_tree->topLevelItem(0)
        : findItemById(parent->id());
    if (!parentItem) return;

    addNodeItem(parentItem, node);
    parentItem->setExpanded(true);
}

void LeftPanel::onNodeRemoved(quint64 id, Node *formerParent)
{
    QTreeWidgetItem *item = findItemById(id);
    if (!item) return;

    if (m_selected && m_selected->id() == id) {
        m_selected = nullptr;
        showDetails(nullptr);
    }

    QTreeWidgetItem *parentItem = item->parent();
    if (!parentItem) parentItem = m_tree->invisibleRootItem();
    parentItem->removeChild(item);
    delete item;

    if (formerParent) {
        QTreeWidgetItem *parentTreeItem = formerParent->isRoot()
            ? m_tree->topLevelItem(0)
            : findItemById(formerParent->id());
        if (parentTreeItem)
            updateItemDisplay(parentTreeItem, formerParent);
    }
}

void LeftPanel::onNodeChanged(Node *node)
{
    QTreeWidgetItem *item = node->isRoot()
        ? m_tree->topLevelItem(0)
        : findItemById(node->id());
    if (item)
        updateItemDisplay(item, node);

    if (m_selected && m_selected->id() == node->id())
        showDetails(node);
}

void LeftPanel::onModelReset()
{
    rebuildTree();
}

// ---------------------------------------------------------------------------
// Tree selection
// ---------------------------------------------------------------------------

void LeftPanel::onTreeSelectionChanged()
{
    const QList<QTreeWidgetItem *> sel = m_tree->selectedItems();
    if (sel.isEmpty()) {
        m_selected = nullptr;
        showDetails(nullptr);
        emit nodeSelected(nullptr);
        return;
    }

    const quint64 id = sel.first()->data(0, Qt::UserRole).toULongLong();
    Node *node = m_nodeModel ? m_nodeModel->nodeById(id) : nullptr;
    m_selected = node;
    showDetails(node);
    emit nodeSelected(node);
}

// ---------------------------------------------------------------------------
// Details pane
// ---------------------------------------------------------------------------

static QString interpretNodeValue(Node *node, Document *doc)
{
    if (!doc || !node || node->type() == Node::Type::Blob || node->isRoot())
        return {};

    const int needed = Node::typeSize(node->type());
    if (needed == 0 || node->length() < needed)
        return {};

    const QByteArray bytes = doc->read(node->absoluteStart(), needed);
    if (bytes.size() < needed) return {};

    const bool le = node->isLittleEndian();

    auto readU = [&](int size) -> quint64 {
        quint64 result = 0;
        if (le) {
            for (int i = size - 1; i >= 0; --i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        } else {
            for (int i = 0; i < size; ++i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        }
        return result;
    };

    switch (node->type()) {
    case Node::Type::U8:  return QString::number(static_cast<quint8>(readU(1)));
    case Node::Type::I8:  return QString::number(static_cast<qint8>(readU(1)));
    case Node::Type::U16: return QString::number(static_cast<quint16>(readU(2)));
    case Node::Type::I16: return QString::number(static_cast<qint16>(readU(2)));
    case Node::Type::U32: return QString::number(static_cast<quint32>(readU(4)));
    case Node::Type::I32: return QString::number(static_cast<qint32>(readU(4)));
    case Node::Type::U64: return QString::number(static_cast<quint64>(readU(8)));
    case Node::Type::I64: return QString::number(static_cast<qint64>(readU(8)));
    case Node::Type::F32: {
        quint32 bits = static_cast<quint32>(readU(4));
        float f; std::memcpy(&f, &bits, sizeof f);
        return QString::number(static_cast<double>(f), 'g', 7);
    }
    case Node::Type::F64: {
        quint64 bits = readU(8);
        double d; std::memcpy(&d, &bits, sizeof d);
        return QString::number(d, 'g', 15);
    }
    default: return {};
    }
}

void LeftPanel::showDetails(Node *node)
{
    m_updating = true;

    const QString dash = "\xe2\x80\x94";

    if (!node) {
        m_nameEdit->clear();
        m_nameEdit->setEnabled(false);
        m_typeCombo->setCurrentIndex(0);
        m_typeCombo->setEnabled(false);
        m_offsetLabel->setText(dash);
        m_lengthLabel->setText(dash);
        m_valueLabel->setText(dash);
        m_endianGroup->buttons().at(0)->setChecked(true);  // default LE
        m_colorBtn->setEnabled(false);
        updateColorButton();
        m_selectBytesBtn->setEnabled(false);
        m_updating = false;
        return;
    }

    m_nameEdit->setText(node->name());
    m_nameEdit->setEnabled(true);

    m_typeCombo->setCurrentText(Node::typeName(node->type()));
    m_typeCombo->setEnabled(node->isLeaf() && !node->isRoot());

    m_offsetLabel->setText(node->isRoot()
        ? dash
        : QString("0x%1").arg(node->startOffset(), 0, 16).toUpper());

    m_lengthLabel->setText(QString::number(node->length()) + " B");

    const QString val = interpretNodeValue(node, m_document);
    m_valueLabel->setText(val.isEmpty() ? dash : val);

    if (node->isLittleEndian())
        m_endianGroup->button(0)->setChecked(true);
    else
        m_endianGroup->button(1)->setChecked(true);

    m_colorBtn->setEnabled(true);
    updateColorButton();
    m_selectBytesBtn->setEnabled(true);

    m_updating = false;
}

void LeftPanel::updateColorButton()
{
    if (m_selected && m_selected->color().isValid()) {
        const QColor c = m_selected->color();
        m_colorBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: 1px solid %3;"
            " font-size: 8pt; }"
            "QPushButton:hover { border-color: %4; }"
        ).arg(c.name(),
              c.lightness() > 128 ? "#000000" : "#ffffff",
              c.darker(130).name(),
              Theme::Color::TEXT));
    } else {
        m_colorBtn->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1;"
            " border: 1px solid %2; font-size: 8pt; }"
            "QPushButton:hover { color: %3; }"
        ).arg(Theme::Color::TEXT_DIM, Theme::Color::BORDER, Theme::Color::TEXT));
    }
}
