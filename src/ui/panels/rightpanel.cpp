#include "rightpanel.h"
#include "core/document.h"
#include "core/node.h"
#include "core/nodemodel.h"
#include "theme/theme.h"

#include <functional>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QButtonGroup>

#include <cstring>  // memcpy
#include <cmath>    // std::isnan, std::isinf

// ---------------------------------------------------------------------------
// UI helpers
// ---------------------------------------------------------------------------

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

// Adds a key/value row to the grid and returns the value label for later updates.
QLabel *RightPanel::addFieldRow(QGridLayout *grid, int row, const QString &label)
{
    auto *k = new QLabel(label);
    k->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_LABEL));

    auto *v = new QLabel("\xe2\x80\x94");  // em dash
    v->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->setWordWrap(false);

    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
    return v;
}

std::pair<QLabel *, QLabel *> RightPanel::addFieldRowPair(QGridLayout *grid, int row, const QString &label)
{
    auto *k = new QLabel(label);
    k->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_LABEL));

    auto *v = new QLabel("\xe2\x80\x94");  // em dash
    v->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->setWordWrap(false);

    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
    return {k, v};
}

static QFrame *makeHRule(QWidget *parent = nullptr)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QString("background: %1;").arg(Theme::Color::BORDER));
    return line;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

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

    // -- Selection info ------------------------------------------------------
    outerLayout->addWidget(makePanelHeader("SELECTION"));

    auto *selWidget = new QWidget;
    selWidget->setStyleSheet("background: transparent;");
    auto *selGrid = new QGridLayout(selWidget);
    selGrid->setContentsMargins(8, 8, 8, 8);
    selGrid->setHorizontalSpacing(10);
    selGrid->setVerticalSpacing(5);
    selGrid->setColumnStretch(1, 1);

    m_offsetLabel = addFieldRow(selGrid, 0, "OFFSET");
    m_lengthLabel = addFieldRow(selGrid, 1, "LENGTH");
    addFieldRow(selGrid, 2, "NODES");

    outerLayout->addWidget(selWidget);
    outerLayout->addWidget(makeHRule());

    // -- INTERPRET AS --------------------------------------------------------
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
        QPushButton:hover  { color: %2; }
        QPushButton:checked { color: %3; }
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
    m_endianGroup->addButton(leBtn, 0);  // 0 = LE
    m_endianGroup->addButton(beBtn, 1);  // 1 = BE

    connect(m_endianGroup, &QButtonGroup::idClicked,
            this, [this](int) { updateInterpretations(); });

    interpHeaderLayout->addWidget(interpLabel);
    interpHeaderLayout->addStretch();
    interpHeaderLayout->addWidget(leBtn);
    interpHeaderLayout->addWidget(beBtn);

    outerLayout->addWidget(interpHeader);

    auto *typesWidget = new QWidget;
    typesWidget->setStyleSheet("background: transparent;");
    auto *typesGrid = new QGridLayout(typesWidget);
    typesGrid->setContentsMargins(8, 8, 8, 8);
    typesGrid->setHorizontalSpacing(10);
    typesGrid->setVerticalSpacing(5);
    typesGrid->setColumnStretch(1, 1);

    const char *typeLabels[TYPE_COUNT] = { "INT", "UINT", "FLOAT" };
    for (int i = 0; i < TYPE_COUNT; ++i) {
        auto [k, v] = addFieldRowPair(typesGrid, i, typeLabels[i]);
        m_typeKeys[i]   = k;
        m_typeValues[i] = v;
    }

    // Reference row (hidden until a node starts at the interpreted address)
    {
        m_refKey = new QLabel("REFERENCE");
        m_refKey->setStyleSheet(QString("color: %1; font-size: 8pt;")
            .arg(Theme::Color::TEXT_LABEL));
        m_refKey->hide();

        m_refValue = new QLabel;
        m_refValue->setStyleSheet(QString("font-size: 8pt;"));
        m_refValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_refValue->setTextFormat(Qt::RichText);
        m_refValue->setTextInteractionFlags(Qt::TextBrowserInteraction);
        m_refValue->setOpenExternalLinks(false);
        m_refValue->hide();
        connect(m_refValue, &QLabel::linkActivated, this, [this](const QString &href) {
            bool ok;
            const quint64 id = href.toULongLong(&ok);
            if (ok) emit navigateToNodeRequested(id);
        });

        typesGrid->addWidget(m_refKey,   TYPE_COUNT, 0);
        typesGrid->addWidget(m_refValue, TYPE_COUNT, 1);
    }

    outerLayout->addWidget(typesWidget);
    outerLayout->addStretch();
    outerLayout->addWidget(makeHRule());

    // -- Create Node button --------------------------------------------------
    auto *createBtn = new QPushButton("CREATE NODE");
    createBtn->setFixedHeight(28);
    createBtn->setStyleSheet(QString(R"(
        QPushButton {
            background: transparent;
            color: %1;
            border: none;
            font-size: 8pt;
        }
        QPushButton:hover   { background: %2; color: %3; }
        QPushButton:pressed { background: %4; }
    )").arg(
        Theme::Color::TEXT_DIM,
        Theme::Color::BG_HOVER,
        Theme::Color::TEXT,
        Theme::Color::BG_ACTIVE
    ));
    connect(createBtn, &QPushButton::clicked, this, [this] {
        if (m_selStart < 0) return;
        emit createNodeRequested(m_selStart, m_selEnd);
    });
    outerLayout->addWidget(createBtn);
}

// ---------------------------------------------------------------------------
// Document wiring
// ---------------------------------------------------------------------------

void RightPanel::setNodeModel(NodeModel *model)
{
    if (m_nodeModel)
        disconnect(m_nodeModel, nullptr, this, nullptr);

    m_nodeModel = model;

    if (m_nodeModel) {
        connect(m_nodeModel, &NodeModel::nodeCreated, this, [this](Node *) { updateInterpretations(); });
        connect(m_nodeModel, &NodeModel::nodeRemoved, this, [this](quint64, Node *) { updateInterpretations(); });
        connect(m_nodeModel, &NodeModel::nodeChanged, this, [this](Node *) { updateInterpretations(); });
        connect(m_nodeModel, &NodeModel::modelReset,  this, [this]() { updateInterpretations(); });
    }

    updateInterpretations();
}

void RightPanel::setDocument(Document *doc)
{
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);

    m_document = doc;
    m_selStart  = -1;
    m_selEnd    = -1;

    if (m_document) {
        // Reinterpret when bytes change in place (e.g. after a keystroke or undo)
        connect(m_document, &Document::dataChanged,
                this, [this](qint64 offset, qint64 length) {
                    if (m_selStart >= 0 &&
                        offset <= m_selEnd && offset + length > m_selStart)
                        updateInterpretations();
                });
        // Reset when the buffer is replaced entirely
        connect(m_document, &Document::sizeChanged, this, [this]() {
            m_selStart = -1;
            m_selEnd   = -1;
            updateInterpretations();
        });
    }

    updateInterpretations();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void RightPanel::onSelectionChanged(qint64 start, qint64 end)
{
    m_selStart = start;
    m_selEnd   = end;
    updateInterpretations();
}

// ---------------------------------------------------------------------------
// Interpretation
// ---------------------------------------------------------------------------

void RightPanel::updateInterpretations()
{
    const QString dash = "\xe2\x80\x94";  // em dash

    // Helper to show/hide a type row
    auto setRowVisible = [&](int idx, bool visible) {
        m_typeKeys[idx]->setVisible(visible);
        m_typeValues[idx]->setVisible(visible);
    };

    auto setRefVisible = [&](bool visible) {
        m_refKey->setVisible(visible);
        m_refValue->setVisible(visible);
    };

    if (m_selStart < 0 || !m_document) {
        m_offsetLabel->setText(dash);
        m_lengthLabel->setText(dash);
        for (int i = 0; i < TYPE_COUNT; ++i)
            setRowVisible(i, false);
        setRefVisible(false);
        return;
    }

    const qint64 len = m_selEnd - m_selStart + 1;
    m_offsetLabel->setText(QString("0x%1")
        .arg(m_selStart, 8, 16, QLatin1Char('0')));
    m_lengthLabel->setText(QString::number(len));

    const int bits = static_cast<int>(len * 8);
    const bool intSupported   = (len >= 1 && len <= 8);
    const bool floatSupported = (len == 4 || len == 8);

    setRowVisible(0, intSupported);    // INT
    setRowVisible(1, intSupported);    // UINT
    setRowVisible(2, floatSupported);  // FLOAT
    if (!intSupported) setRefVisible(false);

    if (intSupported) {
        m_typeKeys[0]->setText(QString("INT%1").arg(bits));
        m_typeKeys[1]->setText(QString("UINT%1").arg(bits));

        const int size = static_cast<int>(len);
        const QByteArray bytes = m_document->read(m_selStart, size);
        const bool le = (m_endianGroup->checkedId() == 0);

        quint64 raw = 0;
        if (le) {
            for (int i = size - 1; i >= 0; --i)
                raw = (raw << 8) | static_cast<uint8_t>(bytes.at(i));
        } else {
            for (int i = 0; i < size; ++i)
                raw = (raw << 8) | static_cast<uint8_t>(bytes.at(i));
        }

        // Signed integer: sign-extend from the actual size
        qint64 signedVal = static_cast<qint64>(raw);
        if (size < 8) {
            const quint64 signBit = quint64(1) << (size * 8 - 1);
            if (raw & signBit)
                signedVal = static_cast<qint64>(raw | ~(signBit - 1));
        }

        m_typeValues[0]->setText(QString::number(signedVal));
        m_typeValues[1]->setText(QString::number(raw));

        // Reference: treat raw as an absolute file offset, find a node starting there
        Node *refTarget = nullptr;
        if (m_nodeModel && m_nodeModel->root() && raw < static_cast<quint64>(m_document->size())) {
            const qint64 targetOffset = static_cast<qint64>(raw);
            std::function<Node*(Node*)> findStarting = [&](Node *node) -> Node* {
                for (Node *child : node->children()) {
                    if (child->absoluteStart() == targetOffset) return child;
                    Node *found = findStarting(child);
                    if (found) return found;
                }
                return nullptr;
            };
            refTarget = findStarting(m_nodeModel->root());
        }
        if (refTarget) {
            const QColor nodeColor = refTarget->color();
            const QString linkColor = nodeColor.isValid() ? nodeColor.name() : Theme::Color::TEXT;
            m_refValue->setText(
                QString("<a href='%1' style='color: %2; text-decoration: underline;'>%3</a>")
                    .arg(refTarget->id())
                    .arg(linkColor)
                    .arg(refTarget->name().toHtmlEscaped()));
            setRefVisible(true);
        } else {
            setRefVisible(false);
        }

        if (floatSupported) {
            m_typeKeys[2]->setText(QString("FLOAT%1").arg(bits));
            if (size == 4) {
                quint32 bits32 = static_cast<quint32>(raw);
                float f;
                std::memcpy(&f, &bits32, sizeof f);
                m_typeValues[2]->setText(QString::number(static_cast<double>(f), 'g', 7));
            } else {
                double d;
                std::memcpy(&d, &raw, sizeof d);
                m_typeValues[2]->setText(QString::number(d, 'g', 15));
            }
        }
    }
}
