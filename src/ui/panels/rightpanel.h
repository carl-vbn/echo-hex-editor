#pragma once
#include <QWidget>

class QLabel;
class QButtonGroup;

class RightPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RightPanel(QWidget *parent = nullptr);

private:
    QButtonGroup *m_endianGroup = nullptr;

    void setupUi();
    static QWidget *makePanelHeader(const QString &text, QWidget *parent = nullptr);
};
