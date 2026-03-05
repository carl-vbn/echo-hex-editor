#pragma once
#include <QTreeWidget>

class Document;
class NodeModel;
class Node;

class ParsedView : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ParsedView(QWidget *parent = nullptr);

    void setDocument(Document *doc);
    void setNodeModel(NodeModel *model);
    void setSelection(qint64 start, qint64 end);

signals:
    void selectionChanged(qint64 start, qint64 end);
    void nodeSelected(Node *node);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemChanged(QTreeWidgetItem *item, int column);

    void onNodeCreated(Node *node);
    void onNodeRemoved(quint64 id, Node *formerParent);
    void onNodeChanged(Node *node);
    void onModelReset();
    void onDataChanged(qint64 offset, qint64 length);

private:
    Document  *m_document  = nullptr;
    NodeModel *m_nodeModel = nullptr;
    bool       m_updating  = false;

    enum Column { ColName = 0, ColType, ColValue, ColSize };

    void rebuildTree();
    void addNodeItem(QTreeWidgetItem *parentItem, Node *node);
    void updateItemDisplay(QTreeWidgetItem *item, Node *node);
    QTreeWidgetItem *findItemById(quint64 id) const;
    QTreeWidgetItem *findItemByIdImpl(QTreeWidgetItem *item, quint64 id) const;

    Node *nodeFromItem(QTreeWidgetItem *item) const;
};
