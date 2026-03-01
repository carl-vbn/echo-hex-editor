#include "mainwindow.h"
#include "titlebar.h"
#include "toolbar.h"
#include "panels/leftpanel.h"
#include "panels/centerpanel.h"
#include "panels/rightpanel.h"
#include "hexview.h"
#include "core/document.h"
#include "theme/theme.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QAction>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QWindow>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_Hover);
    setMinimumSize(900, 600);
    resize(1400, 900);

    qApp->installEventFilter(this);

    setupLayout();
    setupMenuBar();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void MainWindow::setupLayout()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new TitleBar(this);
    m_titleBar->setTitle("Echo");
    root->addWidget(m_titleBar);

    m_menuBar = new QMenuBar(this);
    m_menuBar->setNativeMenuBar(false);
    root->addWidget(m_menuBar);

    m_toolbar = new Toolbar(this);
    root->addWidget(m_toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    m_leftPanel   = new LeftPanel(splitter);
    m_centerPanel = new CenterPanel(splitter);
    m_rightPanel  = new RightPanel(splitter);

    m_leftPanel->setMinimumWidth(Theme::Metric::PANEL_MIN_W);
    m_rightPanel->setMinimumWidth(Theme::Metric::PANEL_MIN_W);

    splitter->addWidget(m_leftPanel);
    splitter->addWidget(m_centerPanel);
    splitter->addWidget(m_rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({220, 800, 220});

    root->addWidget(splitter, 1);

    // Wire toolbar mode toggles to the hex view
    HexView *hv = m_centerPanel->hexView();
    connect(m_toolbar, &Toolbar::insertModeChanged, hv, &HexView::setInsertMode);
    connect(m_toolbar, &Toolbar::directEditChanged, hv, &HexView::setDirectEdit);

    // Wire hex view selection to the right panel
    m_rightPanel->setDocument(m_centerPanel->document());
    connect(hv, &HexView::selectionChanged, m_rightPanel, &RightPanel::onSelectionChanged);
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void MainWindow::setupMenuBar()
{
    Document *doc = m_centerPanel->document();

    // -- FILE ----------------------------------------------------------------
    auto *file = m_menuBar->addMenu("FILE");

    auto *openAction = file->addAction("Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpen);

    m_saveAction = file->addAction("Save");
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSave);

    m_saveAsAction = file->addAction("Save As...");
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);

    file->addSeparator();

    m_closeAction = file->addAction("Close");
    m_closeAction->setShortcut(QKeySequence::Close);
    m_closeAction->setEnabled(false);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::onClose);

    auto *exitAction = file->addAction("Exit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Update title and Save enabled state when modified flag changes
    connect(doc, &Document::modifiedChanged, this, &MainWindow::updateWindowTitle);
    connect(doc, &Document::modifiedChanged, this, [this](bool modified) {
        m_saveAction->setEnabled(modified && !m_currentFilePath.isEmpty());
    });

    // -- EDIT ----------------------------------------------------------------
    auto *edit = m_menuBar->addMenu("EDIT");

    m_undoAction = edit->addAction("Undo");
    m_undoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    m_undoAction->setEnabled(false);

    m_redoAction = edit->addAction("Redo");
    m_redoAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z),
        QKeySequence(Qt::CTRL | Qt::Key_Y),
    });
    m_redoAction->setEnabled(false);

    connect(m_undoAction, &QAction::triggered, doc, &Document::undo);
    connect(m_redoAction, &QAction::triggered, doc, &Document::redo);
    connect(doc, &Document::undoAvailabilityChanged, this,
            [this](bool canUndo, bool canRedo) {
                m_undoAction->setEnabled(canUndo);
                m_redoAction->setEnabled(canRedo);
            });

    edit->addSeparator();
    edit->addAction("Cut",   QKeySequence::Cut);
    edit->addAction("Copy",  QKeySequence::Copy);
    edit->addAction("Paste", QKeySequence::Paste);
    edit->addSeparator();
    edit->addAction("Select All", QKeySequence::SelectAll);

    // -- VIEW ----------------------------------------------------------------
    auto *view = m_menuBar->addMenu("VIEW");
    auto *hexViewAct    = view->addAction("Hex View");
    auto *parsedViewAct = view->addAction("Parsed View");
    hexViewAct->setCheckable(true);
    parsedViewAct->setCheckable(true);
    hexViewAct->setChecked(true);
    (void)hexViewAct;
    (void)parsedViewAct;

    // -- NODE ----------------------------------------------------------------
    auto *nodeMenu = m_menuBar->addMenu("NODE");
    nodeMenu->addAction("Create Node");
    nodeMenu->addAction("Delete Node");
    nodeMenu->addSeparator();
    nodeMenu->addAction("Select Node Bytes");
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void MainWindow::onOpen()
{
    if (!maybeSaveChanges()) return;

    const QString startDir = m_currentFilePath.isEmpty()
                             ? QDir::homePath()
                             : QFileInfo(m_currentFilePath).absolutePath();

    const QString path = QFileDialog::getOpenFileName(this, "Open File", startDir);
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Open Failed",
            QString("Could not open \"%1\":\n%2").arg(path, f.errorString()));
        return;
    }

    m_currentFilePath = path;
    m_centerPanel->document()->loadData(f.readAll());

    m_saveAction->setEnabled(false);  // file just loaded — not yet modified
    m_closeAction->setEnabled(true);
    updateWindowTitle();
}

void MainWindow::onSave()
{
    if (m_currentFilePath.isEmpty()) {
        onSaveAs();
        return;
    }
    saveToPath(m_currentFilePath);
}

void MainWindow::onSaveAs()
{
    const QString startPath = m_currentFilePath.isEmpty()
                              ? QDir::homePath()
                              : m_currentFilePath;

    const QString path = QFileDialog::getSaveFileName(this, "Save As", startPath);
    if (path.isEmpty()) return;

    if (saveToPath(path)) {
        const bool hadPath = !m_currentFilePath.isEmpty();
        m_currentFilePath = path;
        if (!hadPath) m_closeAction->setEnabled(true);
        updateWindowTitle();
    }
}

void MainWindow::onClose()
{
    if (!maybeSaveChanges()) return;

    m_currentFilePath.clear();
    m_centerPanel->document()->loadData({});

    m_saveAction->setEnabled(false);
    m_closeAction->setEnabled(false);
    updateWindowTitle();
}

bool MainWindow::maybeSaveChanges()
{
    Document *doc = m_centerPanel->document();
    if (!doc->isModified() || m_currentFilePath.isEmpty()) return true;

    const QString name = QFileInfo(m_currentFilePath).fileName();
    const auto btn = QMessageBox::question(this, "Unsaved Changes",
        QString("Save changes to \"%1\"?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (btn == QMessageBox::Cancel)  return false;
    if (btn == QMessageBox::Discard) return true;

    // Save — if it fails or the user cancels a Save As dialog, stay put
    onSave();
    return !doc->isModified();
}

bool MainWindow::saveToPath(const QString &path)
{
    Document *doc = m_centerPanel->document();

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Save Failed",
            QString("Could not write to \"%1\":\n%2").arg(path, f.errorString()));
        return false;
    }

    f.write(doc->read(0, doc->size()));

    if (!f.commit()) {
        QMessageBox::critical(this, "Save Failed",
            QString("Could not commit \"%1\":\n%2").arg(path, f.errorString()));
        return false;
    }

    doc->markClean();
    return true;
}

void MainWindow::updateWindowTitle()
{
    if (m_currentFilePath.isEmpty()) {
        m_titleBar->setTitle("Echo");
        return;
    }
    const QString name = QFileInfo(m_currentFilePath).fileName();
    const bool modified = m_centerPanel->document()->isModified();
    m_titleBar->setTitle(QString("%1%2 \u2014 Echo").arg(name, modified ? "*" : ""));
}

// ---------------------------------------------------------------------------
// Close event
// ---------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    maybeSaveChanges() ? event->accept() : event->ignore();
}

// ---------------------------------------------------------------------------
// Edge resize
// ---------------------------------------------------------------------------

Qt::Edges MainWindow::edgesAt(const QPoint &pos) const
{
    const int m = Theme::Metric::RESIZE_MARGIN;
    Qt::Edges edges;
    if (pos.x() <= m)            edges |= Qt::LeftEdge;
    if (pos.y() <= m)            edges |= Qt::TopEdge;
    if (pos.x() >= width() - m)  edges |= Qt::RightEdge;
    if (pos.y() >= height() - m) edges |= Qt::BottomEdge;
    return edges;
}

void MainWindow::updateResizeCursor(const QPoint &pos)
{
    const Qt::Edges e = edgesAt(pos);
    if ((e & Qt::LeftEdge && e & Qt::TopEdge) ||
        (e & Qt::RightEdge && e & Qt::BottomEdge))
        setCursor(Qt::SizeFDiagCursor);
    else if ((e & Qt::RightEdge && e & Qt::TopEdge) ||
             (e & Qt::LeftEdge && e & Qt::BottomEdge))
        setCursor(Qt::SizeBDiagCursor);
    else if (e & (Qt::LeftEdge | Qt::RightEdge))
        setCursor(Qt::SizeHorCursor);
    else if (e & (Qt::TopEdge | Qt::BottomEdge))
        setCursor(Qt::SizeVerCursor);
    else
        setCursor(Qt::ArrowCursor);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    auto *w = qobject_cast<QWidget *>(obj);
    if (!w || (w != this && !isAncestorOf(w)))
        return false;

    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        updateResizeCursor(mapFromGlobal(me->globalPosition().toPoint()));
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            const QPoint local = mapFromGlobal(me->globalPosition().toPoint());
            const Qt::Edges edges = edgesAt(local);
            if (edges && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true;
            }
        }
    }

    return false;
}
