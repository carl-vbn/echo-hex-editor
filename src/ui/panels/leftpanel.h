#pragma once
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QLineEdit;
class QComboBox;
class QButtonGroup;
class QPushButton;
class NodeModel;
class Document;
class Node;

class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);

    void setNodeModel(NodeModel *model);
    void setDocument(Document *doc);
    void selectAndEditNode(Node *node);
    void selectNode(Node *node);

signals:
    void nodeSelected(Node *node);
    void selectBytesRequested(qint64 absStart, qint64 absEnd);

private slots:
    void onTreeSelectionChanged();
    void onNodeCreated(Node *node);
    void onNodeRemoved(quint64 id, Node *formerParent);
    void onNodeChanged(Node *node);
    void onModelReset();

private:
    NodeModel    *m_nodeModel      = nullptr;
    Document     *m_document      = nullptr;
    Node         *m_selected       = nullptr;
    bool          m_updating       = false;

    QTreeWidget  *m_tree           = nullptr;
    QLineEdit    *m_nameEdit       = nullptr;
    QComboBox    *m_typeCombo      = nullptr;
    QLabel       *m_offsetLabel    = nullptr;
    QLabel       *m_lengthLabel    = nullptr;
    QButtonGroup *m_endianGroup    = nullptr;
    QPushButton  *m_colorBtn       = nullptr;
    QPushButton  *m_selectBytesBtn = nullptr;
    QLabel       *m_valueLabel     = nullptr;

    void setupUi();
    static QWidget *makePanelHeader(const QString &text, QWidget *parent = nullptr);

    void rebuildTree();
    void addNodeItem(QTreeWidgetItem *parentItem, Node *node);
    void updateItemDisplay(QTreeWidgetItem *item, Node *node);
    QTreeWidgetItem *findItemById(quint64 id) const;
    QTreeWidgetItem *findItemByIdImpl(QTreeWidgetItem *item, quint64 id) const;

    void showDetails(Node *node);
    void updateColorButton();
};
