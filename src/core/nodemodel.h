#pragma once
#include <QObject>
#include "node.h"

class NodeModel : public QObject
{
    Q_OBJECT

public:
    explicit NodeModel(QObject *parent = nullptr);
    ~NodeModel() override;

    Node *root()             const { return m_root; }
    Node *nodeById(quint64 id) const;

    void   reset(qint64 fileSize);
    void   loadFrom(NodeModel *source);
    Node  *deepestContaining(qint64 absStart, qint64 length) const;
    Node  *createNode(Node *parent, qint64 relStart, qint64 length, const QString &name, Node::Type type = Node::Type::Blob);
    void   removeNode(Node *node);

    void   renameNode(Node *node, const QString &name);
    void   retypeNode(Node *node, Node::Type type);
    void   recolorNode(Node *node, const QColor &color);
    void   setEndian(Node *node, bool le);
    void   setRootLength(qint64 size);
    void   setRefBaseNode(Node *node, quint64 baseNodeId);
    void   setRefConstantOffset(Node *node, qint64 offset);

    // Returns the color of the deepest non-root node covering absOffset that
    // has a color set; invalid QColor if none.
    QColor colorAt(qint64 absOffset) const;

signals:
    void nodeCreated(Node *node);
    void nodeRemoved(quint64 id, Node *formerParent);
    void nodeChanged(Node *node);
    void modelReset();

private:
    Node   *m_root   = nullptr;
    quint64 m_nextId = 0;

    Node  *nodeByIdImpl(Node *node, quint64 id) const;
    Node  *deepestContainingImpl(Node *node, qint64 absStart, qint64 absLength) const;
    QColor colorAtImpl(Node *node, qint64 absOffset) const;
};
