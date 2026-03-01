#include "rightpanel.h"
#include "core/document.h"
#include "theme/theme.h"

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
    v->setStyleSheet(QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_DIM));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->setWordWrap(false);

    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
    return v;
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
    addFieldRow(selGrid, 2, "NODES");   // placeholder — node system not yet implemented

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

    const char *typeLabels[TYPE_COUNT] = {
        "U8",  "I8",
        "U16", "I16",
        "U32", "I32",
        "U64", "I64",
        "F32", "F64",
    };
    for (int i = 0; i < TYPE_COUNT; ++i)
        m_typeValues[i] = addFieldRow(typesGrid, i, typeLabels[i]);

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
    outerLayout->addWidget(createBtn);
}

// ---------------------------------------------------------------------------
// Document wiring
// ---------------------------------------------------------------------------

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
    const QString dash = "\xe2\x80\x94";  // —

    if (m_selStart < 0 || !m_document) {
        m_offsetLabel->setText(dash);
        m_lengthLabel->setText(dash);
        for (auto *lbl : m_typeValues) lbl->setText(dash);
        return;
    }

    const qint64 len = m_selEnd - m_selStart + 1;
    m_offsetLabel->setText(QString("0x%1")
        .arg(m_selStart, 8, 16, QLatin1Char('0')).toUpper());
    m_lengthLabel->setText(QString::number(len));

    // Read up to 8 bytes starting at the selection anchor
    const QByteArray bytes = m_document->read(m_selStart, qMin(len, 8LL));
    const bool le = (m_endianGroup->checkedId() == 0);

    // Assemble an unsigned integer from `size` bytes respecting endianness.
    // Returns 0 and is safe to call even if bytes.size() < size because
    // setInt() checks the size before using the result — but readU() is
    // evaluated eagerly as a function argument, so we guard here too.
    auto readU = [&](int size) -> quint64 {
        if (bytes.size() < size) return 0;
        quint64 result = 0;
        if (le) {
            for (int i = size - 1; i >= 0; --i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        } else {
            for (int i = 0; i < size; ++i)
                result = (result << 8) | static_cast<uint8_t>(bytes.at(i));
        }
        return result;
    };

    // Helper: set label to formatted value if enough bytes, else dash
    auto setInt = [&](int idx, int size, auto value) {
        if (bytes.size() >= size)
            m_typeValues[idx]->setText(QString::number(value));
        else
            m_typeValues[idx]->setText(dash);
    };

    setInt(0, 1, static_cast<quint8>(readU(1)));   // U8
    setInt(1, 1, static_cast<qint8>(readU(1)));    // I8
    setInt(2, 2, static_cast<quint16>(readU(2)));  // U16
    setInt(3, 2, static_cast<qint16>(readU(2)));   // I16
    setInt(4, 4, static_cast<quint32>(readU(4)));  // U32
    setInt(5, 4, static_cast<qint32>(readU(4)));   // I32
    setInt(6, 8, static_cast<quint64>(readU(8)));  // U64
    setInt(7, 8, static_cast<qint64>(readU(8)));   // I64

    // F32
    if (bytes.size() >= 4) {
        quint32 bits = static_cast<quint32>(readU(4));
        float f;
        std::memcpy(&f, &bits, sizeof f);
        m_typeValues[8]->setText(QString::number(static_cast<double>(f), 'g', 7));
    } else {
        m_typeValues[8]->setText(dash);
    }

    // F64
    if (bytes.size() >= 8) {
        quint64 bits = readU(8);
        double d;
        std::memcpy(&d, &bits, sizeof d);
        m_typeValues[9]->setText(QString::number(d, 'g', 15));
    } else {
        m_typeValues[9]->setText(dash);
    }
}
