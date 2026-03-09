#include "mainwindow.h"
#include "titlebar.h"
#include "toolbar.h"
#include "panels/leftpanel.h"
#include "panels/centerpanel.h"
#include "panels/rightpanel.h"
#include "hexview.h"
#include "parsedview.h"
#include "core/document.h"
#include "core/node.h"
#include "core/nodemodel.h"
#include "theme/theme.h"

#include "ext/impl/bmp/parser.h"

#include <functional>

#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QAction>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QWindow>
#include <QApplication>
#include <QFileDialog>
#include "messagebox.h"
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

    connect(m_createNodeAction, &QAction::triggered, this, [this] {
        createNodeFromSelection(m_rightPanel->selectionStart(),
                                m_rightPanel->selectionEnd());
    });
    connect(m_deleteNodeAction, &QAction::triggered,
            this, &MainWindow::deleteSelectedNode);
    connect(m_leftPanel, &LeftPanel::deleteSelectedNodeRequested,
            this, &MainWindow::deleteSelectedNode);
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
    connect(m_toolbar, &Toolbar::nodeSelectModeChanged, hv, &HexView::setNodeSelectMode);

    // Wire hex view selection to the right panel
    m_rightPanel->setDocument(m_centerPanel->document());

    // Wire node model to left panel, right panel, and hex view
    NodeModel *nm = m_centerPanel->nodeModel();
    m_leftPanel->setNodeModel(nm);
    m_leftPanel->setDocument(m_centerPanel->document());
    m_rightPanel->setNodeModel(nm);
    hv->setNodeModel(nm);

    // Auto-select+edit newly created nodes in the left panel
    connect(nm, &NodeModel::nodeCreated, m_leftPanel, &LeftPanel::selectAndEditNode);

    // Handle node creation requests from the right panel
    connect(m_rightPanel, &RightPanel::createNodeRequested,
            this, &MainWindow::createNodeFromSelection);

    // Wire "Select Bytes" from left panel back to hex view
    connect(m_leftPanel, &LeftPanel::selectBytesRequested,
            hv, &HexView::setSelection);

    // Navigate to node by ID (from right panel reference link)
    connect(m_rightPanel, &RightPanel::navigateToNodeRequested,
            this, [this](quint64 nodeId) {
        NodeModel *nm = m_centerPanel->nodeModel();
        HexView   *hv = m_centerPanel->hexView();
        if (!nm || !hv) return;
        Node *node = nm->nodeById(nodeId);
        if (!node) return;
        m_leftPanel->selectNode(node);
        hv->setSelection(node->absoluteStart(), node->absoluteStart() + node->length() - 1);
    });

    // Follow Reference: select target node (or byte) in tree + hex view
    connect(m_leftPanel, &LeftPanel::followReferenceRequested,
            this, [this](qint64 absOffset) {
        NodeModel *nm = m_centerPanel->nodeModel();
        HexView   *hv = m_centerPanel->hexView();
        if (!nm || !hv) return;

        // Find the shallowest node that starts exactly at absOffset
        std::function<Node*(Node*)> findStarting = [&](Node *node) -> Node* {
            for (Node *child : node->children()) {
                if (child->absoluteStart() == absOffset)
                    return child;
                Node *found = findStarting(child);
                if (found) return found;
            }
            return nullptr;
        };

        Node *target = nm->root() ? findStarting(nm->root()) : nullptr;
        if (target) {
            m_leftPanel->selectNode(target);
            hv->setSelection(target->absoluteStart(),
                             target->absoluteStart() + target->length() - 1);
        } else {
            hv->setSelection(absOffset, absOffset);
        }
    });

    // Connect hex view signals (default active view)
    connectHexViewSignals();

    // Wire toolbar view toggle
    connect(m_toolbar, &Toolbar::parsedViewChanged, this, [this](bool parsed) {
        if (parsed)
            switchToParsedView();
        else
            switchToHexView();
    });
}

// ---------------------------------------------------------------------------
// View switching
// ---------------------------------------------------------------------------

void MainWindow::connectHexViewSignals()
{
    HexView *hv = m_centerPanel->hexView();
    connect(hv, &HexView::nodeSelected, m_leftPanel, &LeftPanel::selectNode);
    connect(hv, &HexView::selectionChanged, m_rightPanel, &RightPanel::onSelectionChanged);
}

void MainWindow::disconnectHexViewSignals()
{
    HexView *hv = m_centerPanel->hexView();
    disconnect(hv, &HexView::nodeSelected, m_leftPanel, &LeftPanel::selectNode);
    disconnect(hv, &HexView::selectionChanged, m_rightPanel, &RightPanel::onSelectionChanged);
}

void MainWindow::connectParsedViewSignals()
{
    ParsedView *pv = m_centerPanel->parsedView();
    if (!pv) return;
    connect(pv, &ParsedView::nodeSelected, m_leftPanel, &LeftPanel::selectNode);
    connect(pv, &ParsedView::selectionChanged, m_rightPanel, &RightPanel::onSelectionChanged);
}

void MainWindow::disconnectParsedViewSignals()
{
    ParsedView *pv = m_centerPanel->parsedView();
    if (!pv) return;
    disconnect(pv, &ParsedView::nodeSelected, m_leftPanel, &LeftPanel::selectNode);
    disconnect(pv, &ParsedView::selectionChanged, m_rightPanel, &RightPanel::onSelectionChanged);
}

void MainWindow::switchToHexView()
{
    disconnectParsedViewSignals();
    m_centerPanel->showHexView();
    connectHexViewSignals();

    if (m_hexViewAction) m_hexViewAction->setChecked(true);
    if (m_parsedViewAction) m_parsedViewAction->setChecked(false);
}

void MainWindow::switchToParsedView()
{
    disconnectHexViewSignals();
    m_centerPanel->showParsedView();
    connectParsedViewSignals();

    if (m_hexViewAction) m_hexViewAction->setChecked(false);
    if (m_parsedViewAction) m_parsedViewAction->setChecked(true);
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
    auto *selectAllAction = edit->addAction("Select All");
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this] {
        Document *doc = m_centerPanel->document();
        if (doc->size() <= 0) return;

        if (m_centerPanel->isParsedViewActive()) {
            // In parsed view, select root node
            Node *root = m_centerPanel->nodeModel()->root();
            if (!root) return;
            m_centerPanel->parsedView()->setSelection(0, doc->size() - 1);
        } else {
            HexView *hv = m_centerPanel->hexView();
            if (hv->isNodeSelectMode()) {
                Node *root = m_centerPanel->nodeModel()->root();
                if (!root) return;
                hv->setSelection(0, doc->size() - 1);
                emit hv->nodeSelected(root);
            } else {
                hv->setSelection(0, doc->size() - 1);
            }
        }
    });

    // -- VIEW ----------------------------------------------------------------
    auto *view = m_menuBar->addMenu("VIEW");
    m_hexViewAction    = view->addAction("Hex View");
    m_parsedViewAction = view->addAction("Parsed View");
    m_hexViewAction->setCheckable(true);
    m_parsedViewAction->setCheckable(true);
    m_hexViewAction->setChecked(true);

    connect(m_hexViewAction, &QAction::triggered, this, [this] {
        m_toolbar->setParsedView(false);
    });
    connect(m_parsedViewAction, &QAction::triggered, this, [this] {
        m_toolbar->setParsedView(true);
    });

    // -- NODE ----------------------------------------------------------------
    auto *nodeMenu = m_menuBar->addMenu("NODE");
    m_createNodeAction = nodeMenu->addAction("Create Node");
    m_createNodeAction->setShortcut(Qt::Key_Q);
    m_deleteNodeAction = nodeMenu->addAction("Delete Node");
    m_deleteNodeAction->setShortcut(Qt::Key_Delete);
    nodeMenu->addSeparator();
    nodeMenu->addAction("Select Node Bytes");
    nodeMenu->addSeparator();
    auto *parseMenu = nodeMenu->addMenu("Parse");
    auto *parseBmpAction = parseMenu->addAction("BMP");
    connect(parseBmpAction, &QAction::triggered, this, [this] {
        BmpParser parser;
        runParser(parser);
    });
}

// ---------------------------------------------------------------------------
// Node operations
// ---------------------------------------------------------------------------

void MainWindow::createNodeFromSelection(qint64 selStart, qint64 selEnd)
{
    NodeModel *nm = m_centerPanel->nodeModel();
    if (selStart < 0 || !nm) return;
    Node *parent = m_leftPanel->selectedNode();
    if (!parent) parent = nm->root();

    // Walk up until we find an ancestor whose range contains selStart
    while (parent && !parent->isRoot()) {
        const qint64 start = parent->absoluteStart();
        if (selStart >= start && selStart < start + parent->length())
            break;
        parent = parent->parent();
    }
    if (!parent) parent = nm->root();

    const qint64 len = selEnd - selStart + 1;
    const qint64 relStart = selStart - parent->absoluteStart();
    nm->createNode(parent, relStart, len, "New node");
}

void deleteNodeRecursive(Node *node, NodeModel *nm)
{
    for (Node *child : node->children())
        deleteNodeRecursive(child, nm);
    nm->removeNode(node);
}

void MainWindow::deleteSelectedNode()
{
    NodeModel *nm = m_centerPanel->nodeModel();
    Node *node = m_leftPanel->selectedNode();
    if (!node || !nm || node->isRoot()) return;
    deleteNodeRecursive(node, nm);
}

void MainWindow::runParser(Parser& parser)
{
    Document *doc = m_centerPanel->document();
    if (!doc || doc->size() == 0) return;

    if (!parser.checkCompatibility(*doc)) {
        MessageBox::warning(this, "Parse Failed",
            QString("File does not appear to be a valid %1 file.").arg(parser.name()));
        return;
    }

    NodeModel *parsed = parser.parse(*doc);
    if (!parsed) {
        MessageBox::warning(this, "Parse Failed", 
            QString("Failed to parse %1 file.").arg(parser.name()));
        return;
    }

    m_centerPanel->nodeModel()->loadFrom(parsed);
    delete parsed;
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void MainWindow::openFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        MessageBox::critical(this, "Open Failed",
            QString("Could not open \"%1\":\n%2").arg(path, f.errorString()));
        return;
    }

    m_currentFilePath = path;
    m_centerPanel->document()->loadData(f.readAll());
    m_centerPanel->nodeModel()->reset(m_centerPanel->document()->size());
    m_centerPanel->nodeModel()->renameNode(m_centerPanel->nodeModel()->root(),
                                           QFileInfo(m_currentFilePath).fileName());

    m_saveAction->setEnabled(false);  // file just loaded, not yet modified
    m_closeAction->setEnabled(true);
    updateWindowTitle();

    // Check all known parsers and prompt if one recognises the file
    Document *doc = m_centerPanel->document();
    BmpParser bmpParser;
    Parser *parsers[] = { &bmpParser }; // Temp
    for (Parser *p : parsers) {
        if (p->checkCompatibility(*doc)) {
            const auto btn = MessageBox::question(this, "Parse File",
                QString("This file appears to be a '%1' file. Do you want to run the associated parser?").arg(p->name()));
            if (btn == QMessageBox::Yes)
                runParser(*p);
            break;
        }
    }
}

void MainWindow::onOpen()
{
    if (!maybeSaveChanges()) return;

    const QString startDir = m_currentFilePath.isEmpty()
                             ? QDir::homePath()
                             : QFileInfo(m_currentFilePath).absolutePath();

    const QString path = QFileDialog::getOpenFileName(this, "Open File", startDir);
    if (path.isEmpty()) return;

    openFile(path);
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
    m_centerPanel->nodeModel()->reset(m_centerPanel->document()->size());

    m_saveAction->setEnabled(false);
    m_closeAction->setEnabled(false);
    updateWindowTitle();
}

bool MainWindow::maybeSaveChanges()
{
    Document *doc = m_centerPanel->document();
    if (!doc->isModified() || m_currentFilePath.isEmpty()) return true;

    const QString name = QFileInfo(m_currentFilePath).fileName();
    const auto btn = MessageBox::question(this, "Unsaved Changes",
        QString("Save changes to \"%1\"?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (btn == QMessageBox::Cancel)  return false;
    if (btn == QMessageBox::Discard) return true;

    // Save
    // if it fails or the user cancels a Save As dialog, stay put
    onSave();
    return !doc->isModified();
}

bool MainWindow::saveToPath(const QString &path)
{
    Document *doc = m_centerPanel->document();

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        MessageBox::critical(this, "Save Failed",
            QString("Could not write to \"%1\":\n%2").arg(path, f.errorString()));
        return false;
    }

    f.write(doc->read(0, doc->size()));

    if (!f.commit()) {
        MessageBox::critical(this, "Save Failed",
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
