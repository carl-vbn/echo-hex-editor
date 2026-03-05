#pragma once
#include <QWidget>

class HexView;
class ParsedView;
class Document;
class NodeModel;

class CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CenterPanel(QWidget *parent = nullptr);

    HexView    *hexView()    const { return m_hexView;    }
    ParsedView *parsedView() const { return m_parsedView; }
    Document   *document()   const { return m_document;   }
    NodeModel  *nodeModel()  const { return m_nodeModel;  }

    bool isParsedViewActive() const { return m_parsedViewActive; }

public slots:
    void showHexView();
    void showParsedView();

private:
    HexView    *m_hexView    = nullptr;
    ParsedView *m_parsedView = nullptr;
    Document   *m_document   = nullptr;
    NodeModel  *m_nodeModel  = nullptr;
    bool        m_parsedViewActive = false;

    void setupUi();
    void ensureParsedView();
    static QByteArray makeTestData();
};
