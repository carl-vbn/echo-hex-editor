#include "toolbar.h"
#include "theme/theme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QFrame>

static QPushButton *makeToggle(const QString &label)
{
    auto *btn = new QPushButton(label);
    btn->setCheckable(true);
    btn->setObjectName("ToolbarBtn");
    btn->setCursor(Qt::ArrowCursor);
    return btn;
}

QWidget *Toolbar::makeSeparator()
{
    auto *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedSize(1, 14);
    sep->setStyleSheet(QString("background: %1;").arg(Theme::Color::BORDER_MID));
    return sep;
}

Toolbar::Toolbar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void Toolbar::setupUi()
{
    setFixedHeight(Theme::Metric::TOOLBAR_H);
    setObjectName("Toolbar");

    setStyleSheet(QString(R"(
        QWidget#Toolbar {
            background: %1;
            border-bottom: 1px solid %2;
        }
        QPushButton#ToolbarBtn {
            background: transparent;
            color: %3;
            border: none;
            padding: 0px 14px;
            font-size: 8pt;
        }
        QPushButton#ToolbarBtn:hover {
            background: %4;
            color: %5;
        }
        QPushButton#ToolbarBtn:checked {
            color: %6;
        }
        QPushButton#ToolbarBtn:pressed {
            background: %7;
        }
    )").arg(
        Theme::Color::BG_HEADER,
        Theme::Color::BORDER,
        Theme::Color::TEXT_DIM,
        Theme::Color::BG_HOVER,
        Theme::Color::TEXT,
        Theme::Color::TEXT_BRIGHT,
        Theme::Color::BG_ACTIVE
    ));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(0);

    // Edit mode (exclusive)
    auto *fixedBtn  = makeToggle("FIXED");
    auto *insertBtn = makeToggle("INSERT");
    fixedBtn->setChecked(true);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    m_modeGroup->addButton(fixedBtn,  0);
    m_modeGroup->addButton(insertBtn, 1);

    // Feature toggles
    m_directEdit = makeToggle("DIRECT EDIT");
    m_nodeSelect = makeToggle("NODE SELECT");

    // View mode (exclusive)
    auto *hexBtn    = makeToggle("HEX");
    auto *parsedBtn = makeToggle("PARSED");
    hexBtn->setChecked(true);

    m_viewGroup = new QButtonGroup(this);
    m_viewGroup->setExclusive(true);
    m_viewGroup->addButton(hexBtn,    0);
    m_viewGroup->addButton(parsedBtn, 1);

    layout->addWidget(fixedBtn);
    layout->addWidget(insertBtn);
    layout->addSpacing(6);
    layout->addWidget(makeSeparator());
    layout->addSpacing(6);
    layout->addWidget(m_directEdit);
    layout->addWidget(m_nodeSelect);
    layout->addSpacing(6);
    layout->addWidget(makeSeparator());
    layout->addSpacing(6);
    layout->addWidget(hexBtn);
    layout->addWidget(parsedBtn);
    layout->addStretch();

    connect(m_modeGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (checked) emit insertModeChanged(id == 1);
    });
    connect(m_directEdit, &QPushButton::toggled, this, &Toolbar::directEditChanged);
    connect(m_nodeSelect, &QPushButton::toggled, this, &Toolbar::nodeSelectModeChanged);
    connect(m_viewGroup, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (checked) emit parsedViewChanged(id == 1);
    });
}

bool Toolbar::isInsertMode()     const { return m_modeGroup->checkedId() == 1; }
bool Toolbar::isDirectEdit()     const { return m_directEdit && m_directEdit->isChecked(); }
bool Toolbar::isNodeSelectMode() const { return m_nodeSelect && m_nodeSelect->isChecked(); }
bool Toolbar::isParsedView()     const { return m_viewGroup->checkedId() == 1; }

void Toolbar::setParsedView(bool parsed)
{
    auto *btn = m_viewGroup->button(parsed ? 1 : 0);
    if (btn && !btn->isChecked())
        btn->click();
}
