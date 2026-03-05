#include "parsedview.h"
#include "core/node.h"
#include "core/nodemodel.h"
#include "core/document.h"
#include "core/nodeutil.h"
#include "theme/theme.h"

#include <QHeaderView>
#include <QPixmap>

ParsedView::ParsedView(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderLabels({"NAME", "TYPE", "VALUE", "SIZE"});
    setRootIsDecorated(true);
    setAlternatingRowColors(false);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    header()->setStretchLastSection(true);
    header()->setDefaultSectionSize(80);
    setColumnWidth(ColName, 180);
    setColumnWidth(ColType, 60);
    setColumnWidth(ColValue, 140);

    setStyleSheet(QString(
        "QTreeWidget { background: %1; color: %2; border: none; font-size: 8pt; }"
        "QTreeWidget::item { padding: 1px 0px; }"
        "QTreeWidget::item:selected { background: %3; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        " border-right: 1px solid %6; border-bottom: 1px solid %6;"
        " padding: 2px 6px; font-size: 7pt; }"
    ).arg(Theme::Color::BG_PANEL, Theme::Color::TEXT,
          Theme::Color::BG_ACTIVE, Theme::Color::BG_HEADER,
          Theme::Color::TEXT_LABEL, Theme::Color::BORDER));

    connect(this, &QTreeWidget::itemSelectionChanged,
            this, &ParsedView::onItemSelectionChanged);
    connect(this, &QTreeWidget::itemDoubleClicked,
            this, &ParsedView::onItemDoubleClicked);
    connect(this, &QTreeWidget::itemChanged,
            this, &ParsedView::onItemChanged);
}

void ParsedView::setDocument(Document *doc)
{
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);

    m_document = doc;

    if (m_document) {
        connect(m_document, &Document::dataChanged,
                this, &ParsedView::onDataChanged);
    }

    rebuildTree();
}

void ParsedView::setNodeModel(NodeModel *model)
{
    if (m_nodeModel)
        disconnect(m_nodeModel, nullptr, this, nullptr);

    m_nodeModel = model;

    if (m_nodeModel) {
        connect(m_nodeModel, &NodeModel::nodeCreated, this, &ParsedView::onNodeCreated);
        connect(m_nodeModel, &NodeModel::nodeRemoved, this, &ParsedView::onNodeRemoved);
        connect(m_nodeModel, &NodeModel::nodeChanged, this, &ParsedView::onNodeChanged);
        connect(m_nodeModel, &NodeModel::modelReset,  this, &ParsedView::onModelReset);
    }

    rebuildTree();
}

void ParsedView::setSelection(qint64 start, qint64 end)
{
    if (!m_nodeModel) return;

    // Find the deepest node containing this range
    qint64 length = end - start + 1;
    if (length <= 0) return;

    Node *node = m_nodeModel->deepestContaining(start, length);
    if (!node) return;

    QTreeWidgetItem *item = node->isRoot()
        ? topLevelItem(0)
        : findItemById(node->id());
    if (!item) return;

    m_updating = true;
    setCurrentItem(item);
    scrollToItem(item);
    m_updating = false;
}

// ---------------------------------------------------------------------------
// Tree building
// ---------------------------------------------------------------------------

void ParsedView::rebuildTree()
{
    m_updating = true;
    clear();
    m_updating = false;

    if (!m_nodeModel || !m_nodeModel->root()) return;

    m_updating = true;
    addNodeItem(invisibleRootItem(), m_nodeModel->root());
    topLevelItem(0)->setExpanded(true);
    m_updating = false;
}

void ParsedView::addNodeItem(QTreeWidgetItem *parentItem, Node *node)
{
    auto *item = new QTreeWidgetItem(parentItem);
    item->setData(0, Qt::UserRole, QVariant::fromValue(node->id()));
    updateItemDisplay(item, node);

    for (Node *child : node->children())
        addNodeItem(item, child);
}

void ParsedView::updateItemDisplay(QTreeWidgetItem *item, Node *node)
{
    item->setText(ColName, node->name());

    // Type column: show type for typed leaves only
    if (node->isLeaf() && !node->isRoot())
        item->setText(ColType, Node::typeName(node->type()));
    else
        item->setText(ColType, QString());

    // Value column: interpreted value for typed leaves
    if (m_document && node->isLeaf() && !node->isRoot()
        && node->type() != Node::Type::Blob) {
        item->setText(ColValue, interpretNodeValue(node, m_document));
    } else if (node->type() == Node::Type::Blob && node->isLeaf()) {
        item->setText(ColValue, QString("%1 bytes").arg(node->length()));
    } else {
        item->setText(ColValue, QString());
    }

    // Size column
    item->setText(ColSize, QString::number(node->length()));

    // Color decoration
    if (node->color().isValid()) {
        QPixmap pix(10, 10);
        pix.fill(node->color());
        item->setData(ColName, Qt::DecorationRole, pix);
    } else {
        item->setData(ColName, Qt::DecorationRole, QVariant());
    }

    // Make VALUE column editable for typed leaf nodes
    if (node->isLeaf() && !node->isRoot()
        && node->type() != Node::Type::Blob) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    } else {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
}

// ---------------------------------------------------------------------------
// Item lookup
// ---------------------------------------------------------------------------

QTreeWidgetItem *ParsedView::findItemById(quint64 id) const
{
    return findItemByIdImpl(invisibleRootItem(), id);
}

QTreeWidgetItem *ParsedView::findItemByIdImpl(QTreeWidgetItem *item, quint64 id) const
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

Node *ParsedView::nodeFromItem(QTreeWidgetItem *item) const
{
    if (!item || !m_nodeModel) return nullptr;
    const quint64 id = item->data(0, Qt::UserRole).toULongLong();
    return m_nodeModel->nodeById(id);
}

// ---------------------------------------------------------------------------
// Selection handling
// ---------------------------------------------------------------------------

void ParsedView::onItemSelectionChanged()
{
    if (m_updating) return;

    const QList<QTreeWidgetItem *> sel = selectedItems();
    if (sel.isEmpty()) return;

    Node *node = nodeFromItem(sel.first());
    if (!node) return;

    emit nodeSelected(node);

    const qint64 start = node->absoluteStart();
    const qint64 end   = start + node->length() - 1;
    emit selectionChanged(start, end);
}

// ---------------------------------------------------------------------------
// Inline editing
// ---------------------------------------------------------------------------

void ParsedView::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (column != ColValue) return;

    Node *node = nodeFromItem(item);
    if (!node || !node->isLeaf() || node->isRoot()
        || node->type() == Node::Type::Blob)
        return;

    editItem(item, ColValue);
}

void ParsedView::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_updating || column != ColValue) return;

    Node *node = nodeFromItem(item);
    if (!node || !m_document) return;

    const QString newText = item->text(ColValue).trimmed();
    if (newText.isEmpty()) return;

    m_updating = true;
    if (!writeNodeValue(node, m_document, newText)) {
        // Revert to current value on failure
        updateItemDisplay(item, node);
    }
    m_updating = false;
}

// ---------------------------------------------------------------------------
// NodeModel slots
// ---------------------------------------------------------------------------

void ParsedView::onNodeCreated(Node *node)
{
    Node *parent = node->parent();
    if (!parent) return;

    QTreeWidgetItem *parentItem = parent->isRoot()
        ? topLevelItem(0)
        : findItemById(parent->id());
    if (!parentItem) return;

    m_updating = true;
    addNodeItem(parentItem, node);
    parentItem->setExpanded(true);
    m_updating = false;
}

void ParsedView::onNodeRemoved(quint64 id, Node * /*formerParent*/)
{
    QTreeWidgetItem *item = findItemById(id);
    if (!item) return;

    m_updating = true;
    QTreeWidgetItem *parentItem = item->parent();
    if (!parentItem) parentItem = invisibleRootItem();
    parentItem->removeChild(item);
    delete item;
    m_updating = false;
}

void ParsedView::onNodeChanged(Node *node)
{
    QTreeWidgetItem *item = node->isRoot()
        ? topLevelItem(0)
        : findItemById(node->id());
    if (!item) return;

    m_updating = true;
    updateItemDisplay(item, node);
    m_updating = false;
}

void ParsedView::onModelReset()
{
    rebuildTree();
}

void ParsedView::onDataChanged(qint64 /*offset*/, qint64 /*length*/)
{
    // Refresh all displayed values
    // A more targeted approach would check which nodes overlap, but
    // for simplicity we refresh everything.
    if (!m_nodeModel || !m_nodeModel->root()) return;

    m_updating = true;
    std::function<void(QTreeWidgetItem *)> refresh = [&](QTreeWidgetItem *item) {
        Node *node = nodeFromItem(item);
        if (node)
            updateItemDisplay(item, node);
        for (int i = 0; i < item->childCount(); ++i)
            refresh(item->child(i));
    };

    for (int i = 0; i < topLevelItemCount(); ++i)
        refresh(topLevelItem(i));
    m_updating = false;
}
