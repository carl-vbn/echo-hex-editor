#include "leftpanel.h"
#include "theme/theme.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLabel>
#include <QGridLayout>

// Shared panel header widget

QWidget *LeftPanel::makePanelHeader(const QString &text, QWidget *parent)
{
    auto *lbl = new QLabel(text, parent);
    lbl->setFixedHeight(20);
    lbl->setContentsMargins(8, 0, 8, 0);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    lbl->setStyleSheet(QString(
        "background: %1;"
        "color: %2;"
        "border-bottom: 1px solid %3;"
        "font-size: 7pt;"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::TEXT_LABEL, Theme::Color::BORDER));
    return lbl;
}

// LeftPanel

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void LeftPanel::setupUi()
{
    setObjectName("LeftPanel");
    setStyleSheet(QString(
        "QWidget#LeftPanel { background: %1; border-right: 1px solid %2; }"
    ).arg(Theme::Color::BG_PANEL, Theme::Color::BORDER));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    // Top: node tree

    auto *treeContainer = new QWidget;
    treeContainer->setStyleSheet("background: transparent;");
    auto *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->setContentsMargins(0, 0, 0, 0);
    treeLayout->setSpacing(0);

    treeLayout->addWidget(makePanelHeader("NODE HIERARCHY"));

    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({"NAME", "TYPE", "SIZE"});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setDefaultSectionSize(60);
    m_tree->setColumnWidth(0, 110);
    m_tree->setColumnWidth(1, 54);

    // Placeholder root node
    auto *root = new QTreeWidgetItem(m_tree, {QString("(no file)"), QString(""), QString("")});
    root->setForeground(0, QColor(Theme::Color::TEXT_DIM));
    m_tree->addTopLevelItem(root);

    treeLayout->addWidget(m_tree);

    // Bottom: node details

    auto *detailsContainer = new QWidget;
    detailsContainer->setStyleSheet("background: transparent;");
    auto *detailsLayout = new QVBoxLayout(detailsContainer);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(0);

    detailsLayout->addWidget(makePanelHeader("NODE DETAILS"));

    auto *fields = new QWidget;
    fields->setStyleSheet("background: transparent;");
    auto *grid = new QGridLayout(fields);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(5);
    grid->setColumnStretch(1, 1);

    const struct { const char *key; const char *value; } rows[] = {
        { "NAME",   "—" },
        { "TYPE",   "—" },
        { "OFFSET", "—" },
        { "LENGTH", "—" },
        { "ENDIAN", "—" },
        { "COLOR",  "—" },
    };

    for (int i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); ++i) {
        auto *key = new QLabel(rows[i].key);
        key->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_LABEL));
        auto *val = new QLabel(rows[i].value);
        val->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_DIM));
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(key, i, 0);
        grid->addWidget(val, i, 1);
    }

    detailsLayout->addWidget(fields);
    detailsLayout->addStretch();

    splitter->addWidget(treeContainer);
    splitter->addWidget(detailsContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    outerLayout->addWidget(splitter);
}
