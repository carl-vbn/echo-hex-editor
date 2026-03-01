#pragma once
#include <QWidget>

class TitleBar;
class Toolbar;
class LeftPanel;
class CenterPanel;
class RightPanel;
class QMenuBar;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    TitleBar    *m_titleBar    = nullptr;
    QMenuBar    *m_menuBar     = nullptr;
    Toolbar     *m_toolbar     = nullptr;
    LeftPanel   *m_leftPanel   = nullptr;
    CenterPanel *m_centerPanel = nullptr;
    RightPanel  *m_rightPanel  = nullptr;

    void setupLayout();
    void setupMenuBar();

    Qt::Edges edgesAt(const QPoint &localPos) const;
    void updateResizeCursor(const QPoint &localPos);
};
