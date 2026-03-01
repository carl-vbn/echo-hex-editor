#pragma once
#include <QWidget>

class QButtonGroup;

class Toolbar : public QWidget
{
    Q_OBJECT

public:
    explicit Toolbar(QWidget *parent = nullptr);

    bool isInsertMode()     const;
    bool isDirectEdit()     const;
    bool isNodeSelectMode() const;
    bool isParsedView()     const;

signals:
    void insertModeChanged(bool insertMode);
    void directEditChanged(bool enabled);
    void nodeSelectModeChanged(bool enabled);
    void parsedViewChanged(bool parsedView);

private:
    QButtonGroup *m_modeGroup = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
    class QPushButton *m_directEdit   = nullptr;
    class QPushButton *m_nodeSelect   = nullptr;

    void setupUi();
    QWidget *makeSeparator();
};
