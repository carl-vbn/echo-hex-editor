#include "rightpanel.h"
#include "theme/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QButtonGroup>

// Helpers

QWidget *RightPanel::makePanelHeader(const QString &text, QWidget *parent)
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

static QFrame *makeHRule(QWidget *parent = nullptr)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QString("background: %1;").arg(Theme::Color::BORDER));
    return line;
}

static void addFieldRow(QGridLayout *grid, int row,
                        const QString &label, const QString &value)
{
    auto *k = new QLabel(label);
    k->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_LABEL));

    auto *v = new QLabel(value);
    v->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_DIM));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
}

// RightPanel

RightPanel::RightPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void RightPanel::setupUi()
{
    setObjectName("RightPanel");
    setStyleSheet(QString(
        "QWidget#RightPanel { background: %1; border-left: 1px solid %2; }"
    ).arg(Theme::Color::BG_PANEL, Theme::Color::BORDER));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Selection info

    outerLayout->addWidget(makePanelHeader("SELECTION"));

    auto *selWidget = new QWidget;
    selWidget->setStyleSheet("background: transparent;");
    auto *selGrid = new QGridLayout(selWidget);
    selGrid->setContentsMargins(8, 8, 8, 8);
    selGrid->setHorizontalSpacing(10);
    selGrid->setVerticalSpacing(5);
    selGrid->setColumnStretch(1, 1);

    addFieldRow(selGrid, 0, "OFFSET",    "—");
    addFieldRow(selGrid, 1, "LENGTH",    "—");
    addFieldRow(selGrid, 2, "NODES",     "—");

    outerLayout->addWidget(selWidget);
    outerLayout->addWidget(makeHRule());

    // Type interpretation

    // Header row with LE/BE toggle on the right
    auto *interpHeader = new QWidget;
    interpHeader->setFixedHeight(20);
    interpHeader->setStyleSheet(QString(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::BORDER));

    auto *interpHeaderLayout = new QHBoxLayout(interpHeader);
    interpHeaderLayout->setContentsMargins(8, 0, 4, 0);
    interpHeaderLayout->setSpacing(0);

    auto *interpLabel = new QLabel("INTERPRET AS");
    interpLabel->setStyleSheet(QString("color: %1; font-size: 7pt; background: transparent;")
        .arg(Theme::Color::TEXT_LABEL));

    const QString endianBtnStyle = QString(R"(
        QPushButton {
            background: transparent;
            color: %1;
            border: none;
            padding: 0px 5px;
            font-size: 7pt;
        }
        QPushButton:hover {
            color: %2;
        }
        QPushButton:checked {
            color: %3;
        }
    )").arg(Theme::Color::TEXT_DIM, Theme::Color::TEXT, Theme::Color::TEXT_BRIGHT);

    auto *leBtn = new QPushButton("LE");
    auto *beBtn = new QPushButton("BE");
    leBtn->setCheckable(true);
    beBtn->setCheckable(true);
    leBtn->setChecked(true);
    leBtn->setStyleSheet(endianBtnStyle);
    beBtn->setStyleSheet(endianBtnStyle);
    leBtn->setFixedHeight(20);
    beBtn->setFixedHeight(20);

    m_endianGroup = new QButtonGroup(this);
    m_endianGroup->setExclusive(true);
    m_endianGroup->addButton(leBtn, 0);
    m_endianGroup->addButton(beBtn, 1);

    interpHeaderLayout->addWidget(interpLabel);
    interpHeaderLayout->addStretch();
    interpHeaderLayout->addWidget(leBtn);
    interpHeaderLayout->addWidget(beBtn);

    outerLayout->addWidget(interpHeader);

    // Type rows
    auto *typesWidget = new QWidget;
    typesWidget->setStyleSheet("background: transparent;");
    auto *typesGrid = new QGridLayout(typesWidget);
    typesGrid->setContentsMargins(8, 8, 8, 8);
    typesGrid->setHorizontalSpacing(10);
    typesGrid->setVerticalSpacing(5);
    typesGrid->setColumnStretch(1, 1);

    const char *typeLabels[] = {
        "U8", "I8",
        "U16", "I16",
        "U32", "I32",
        "U64", "I64",
        "F32", "F64",
    };
    for (int i = 0; i < (int)(sizeof(typeLabels) / sizeof(typeLabels[0])); ++i)
        addFieldRow(typesGrid, i, typeLabels[i], "—");

    outerLayout->addWidget(typesWidget);
    outerLayout->addStretch();
    outerLayout->addWidget(makeHRule());

    // Create Node button

    auto *createBtn = new QPushButton("CREATE NODE");
    createBtn->setFixedHeight(28);
    createBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent;
            color: %1;
            border: none;
            font-size: 8pt;
        }
        QPushButton:hover {
            background: %2;
            color: %3;
        }
        QPushButton:pressed {
            background: %4;
        }
    )").arg(
        Theme::Color::TEXT_DIM,
        Theme::Color::BG_HOVER,
        Theme::Color::TEXT,
        Theme::Color::BG_ACTIVE
    ));

    outerLayout->addWidget(createBtn);
}
