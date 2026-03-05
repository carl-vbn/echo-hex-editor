#pragma once
#include <QWidget>
#include <QString>

class TitleBar;
class Toolbar;
class LeftPanel;
class CenterPanel;
class RightPanel;
class QMenuBar;
class QAction;
class QCloseEvent;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    TitleBar    *m_titleBar    = nullptr;
    QMenuBar    *m_menuBar     = nullptr;
    Toolbar     *m_toolbar     = nullptr;
    LeftPanel   *m_leftPanel   = nullptr;
    CenterPanel *m_centerPanel = nullptr;
    RightPanel  *m_rightPanel  = nullptr;

    // File menu actions whose enabled state changes at runtime
    QAction *m_saveAction   = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_closeAction  = nullptr;
    QAction *m_undoAction   = nullptr;
    QAction *m_redoAction   = nullptr;

    // VIEW menu actions
    QAction *m_hexViewAction    = nullptr;
    QAction *m_parsedViewAction = nullptr;

    // NODE menu actions
    QAction *m_createNodeAction = nullptr;
    QAction *m_deleteNodeAction = nullptr;

    QString m_currentFilePath;

    void setupLayout();
    void setupMenuBar();

    // View switching
    void switchToHexView();
    void switchToParsedView();
    void connectHexViewSignals();
    void disconnectHexViewSignals();
    void connectParsedViewSignals();
    void disconnectParsedViewSignals();

    // Node operations
    void createNodeFromSelection(qint64 selStart, qint64 selEnd);
    void deleteSelectedNode();

    // File operations
    void onOpen();
    void onSave();
    void onSaveAs();
    void onClose();

    // Returns false if the user cancelled (e.g. chose Cancel in the save prompt)
    bool maybeSaveChanges();
    bool saveToPath(const QString &path);
    void updateWindowTitle();

    Qt::Edges edgesAt(const QPoint &localPos) const;
    void updateResizeCursor(const QPoint &localPos);
};
