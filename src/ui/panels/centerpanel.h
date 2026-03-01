#pragma once
#include <QWidget>

class HexView;
class Document;

class CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CenterPanel(QWidget *parent = nullptr);

    HexView  *hexView()  const { return m_hexView;  }
    Document *document() const { return m_document; }

private:
    HexView  *m_hexView  = nullptr;
    Document *m_document = nullptr;

    void setupUi();
    static QByteArray makeTestData();
};
