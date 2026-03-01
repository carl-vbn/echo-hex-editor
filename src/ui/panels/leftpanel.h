#pragma once
#include <QWidget>

class QTreeWidget;
class QLabel;

class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);

private:
    QTreeWidget *m_tree    = nullptr;
    QWidget     *m_details = nullptr;

    void setupUi();
    static QWidget *makePanelHeader(const QString &text, QWidget *parent = nullptr);
};
