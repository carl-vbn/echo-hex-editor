#pragma once
#include <QWidget>

class HexView;
class Document;
class NodeModel;

class CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CenterPanel(QWidget *parent = nullptr);

    HexView   *hexView()   const { return m_hexView;   }
    Document  *document()  const { return m_document;  }
    NodeModel *nodeModel() const { return m_nodeModel; }

private:
    HexView   *m_hexView   = nullptr;
    Document  *m_document  = nullptr;
    NodeModel *m_nodeModel = nullptr;

    void setupUi();
    static QByteArray makeTestData();
};
