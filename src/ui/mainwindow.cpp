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
#include <QWindow>
#include <QApplication>

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

// Layout

void MainWindow::setupLayout()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new TitleBar(this);
    m_titleBar->setTitle("Hex Editor");
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
}

// Menu bar

void MainWindow::setupMenuBar()
{
    auto *file = m_menuBar->addMenu("FILE");
    file->addAction("Open...",  QKeySequence::Open);
    file->addAction("Save",     QKeySequence::Save);
    file->addAction("Save As...");
    file->addSeparator();
    file->addAction("Close",    QKeySequence::Close);
    file->addAction("Exit",     QKeySequence::Quit);

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

    // Wire to document
    Document *doc = m_centerPanel->document();
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

    auto *view = m_menuBar->addMenu("VIEW");
    auto *hexViewAct    = view->addAction("Hex View");
    auto *parsedViewAct = view->addAction("Parsed View");
    hexViewAct->setCheckable(true);
    parsedViewAct->setCheckable(true);
    hexViewAct->setChecked(true);

    auto *nodeMenu = m_menuBar->addMenu("NODE");
    nodeMenu->addAction("Create Node");
    nodeMenu->addAction("Delete Node");
    nodeMenu->addSeparator();
    nodeMenu->addAction("Select Node Bytes");

    (void)hexViewAct;
    (void)parsedViewAct;
}

// Edge resize

Qt::Edges MainWindow::edgesAt(const QPoint &pos) const
{
    const int m = Theme::Metric::RESIZE_MARGIN;
    Qt::Edges edges;
    if (pos.x() <= m)           edges |= Qt::LeftEdge;
    if (pos.y() <= m)           edges |= Qt::TopEdge;
    if (pos.x() >= width() - m) edges |= Qt::RightEdge;
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
    // Only intercept events for widgets that belong to this window
    auto *w = qobject_cast<QWidget *>(obj);
    if (!w || (w != this && !isAncestorOf(w)))
        return false;

    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        updateResizeCursor(mapFromGlobal(me->globalPosition().toPoint()));
        return false; // don't consume — just update cursor
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            const QPoint local = mapFromGlobal(me->globalPosition().toPoint());
            const Qt::Edges edges = edgesAt(local);
            if (edges && windowHandle()) {
                windowHandle()->startSystemResize(edges);
                return true; // consume — don't let child widget start a drag
            }
        }
    }

    return false;
}
