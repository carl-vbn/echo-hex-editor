#include "nodemodel.h"
#include <QRandomGenerator>

NodeModel::NodeModel(QObject *parent)
    : QObject(parent)
{}

NodeModel::~NodeModel()
{
    delete m_root;
}

void NodeModel::reset(qint64 fileSize)
{
    delete m_root;
    m_nextId = 0;
    m_root = new Node(m_nextId++, nullptr, 0, fileSize, "File");
    m_root->m_littleEndian = true;
    emit modelReset();
}

void NodeModel::loadFrom(NodeModel *source)
{
    delete m_root;
    m_root = source->m_root;
    m_nextId = source->m_nextId;
    source->m_root = nullptr;  // prevent double-free in source's destructor
    emit modelReset();
}

Node *NodeModel::nodeById(quint64 id) const
{
    return m_root ? nodeByIdImpl(m_root, id) : nullptr;
}

Node *NodeModel::nodeByIdImpl(Node *node, quint64 id) const
{
    if (node->id() == id) return node;
    for (Node *child : node->children()) {
        Node *result = nodeByIdImpl(child, id);
        if (result) return result;
    }
    return nullptr;
}

Node *NodeModel::deepestContaining(qint64 absStart, qint64 length) const
{
    return m_root ? deepestContainingImpl(m_root, absStart, length) : nullptr;
}

Node *NodeModel::deepestContainingImpl(Node *node, qint64 absStart, qint64 absLength) const
{
    const qint64 nodeStart = node->absoluteStart();
    const qint64 nodeEnd   = nodeStart + node->length();
    const qint64 selEnd    = absStart + absLength;

    if (absStart < nodeStart || selEnd > nodeEnd)
        return nullptr;

    // DFS: prefer a deeper match
    for (Node *child : node->children()) {
        Node *result = deepestContainingImpl(child, absStart, absLength);
        if (result) return result;
    }

    return node;
}

Node *NodeModel::createNode(Node *parent, qint64 relStart, qint64 length, const QString &name, Node::Type type)
{
    if (!parent) {
        if (m_root) return nullptr;  // Can't create root if one already exists
        m_root = new Node(m_nextId++, nullptr, relStart, length, name, type);
        m_root->m_littleEndian = true;
        emit nodeCreated(m_root);
        emit modelReset();
        return m_root;
    }

    Node *node = new Node(m_nextId++, parent, relStart, length, name, type);
    node->m_littleEndian = parent->isLittleEndian();

    // Assign a random saturated color
    const int hue = QRandomGenerator::global()->bounded(360);
    node->m_color = QColor::fromHsl(hue, 200, 145);

    parent->m_children.append(node);
    emit nodeCreated(node);
    emit nodeChanged(parent);
    return node;
}

void NodeModel::removeNode(Node *node)
{
    if (!node || node->isRoot()) return;
    Node *parent = node->parent();

    parent->m_children.removeOne(node);
    emit nodeRemoved(node->id(), parent);
    emit nodeChanged(parent);
    delete node;
}

void NodeModel::renameNode(Node *node, const QString &name)
{
    node->m_name = name;
    emit nodeChanged(node);
}

void NodeModel::retypeNode(Node *node, Node::Type type)
{
    node->m_type = type;
    emit nodeChanged(node);
}

void NodeModel::recolorNode(Node *node, const QColor &color)
{
    node->m_color = color;
    emit nodeChanged(node);
}

void NodeModel::setEndian(Node *node, bool le)
{
    node->m_littleEndian = le;
    emit nodeChanged(node);
}

void NodeModel::setRootLength(qint64 size)
{
    if (m_root) {
        m_root->m_length = size;
        emit nodeChanged(m_root);
    }
}

QColor NodeModel::colorAt(qint64 absOffset) const
{
    if (!m_root) return {};
    // Skip the root itself; only color bytes covered by non-root nodes
    for (Node *child : m_root->children()) {
        QColor c = colorAtImpl(child, absOffset);
        if (c.isValid()) return c;
    }
    return {};
}

QColor NodeModel::colorAtImpl(Node *node, qint64 absOffset) const
{
    const qint64 start = node->absoluteStart();
    if (absOffset < start || absOffset >= start + node->length())
        return {};

    // Prefer a deeper child
    for (Node *child : node->children()) {
        QColor c = colorAtImpl(child, absOffset);
        if (c.isValid()) return c;
    }

    return node->color().isValid() ? node->color() : QColor();
}
