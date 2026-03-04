#include "titlebar.h"
#include "theme/theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QAbstractButton>
#include <QPainter>
#include <QMouseEvent>
#include <QWindow>

// WinButton
// Custom-painted window control button. No Q_OBJECT needed — we only use
// the clicked() signal inherited from QAbstractButton.

class WinButton : public QAbstractButton
{
public:
    enum class Type { Minimize, Maximize, Close };

    explicit WinButton(Type type, QWidget *parent = nullptr)
        : QAbstractButton(parent), m_type(type)
    {
        setFixedSize(36, Theme::Metric::TITLE_BAR_H);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_Hover); // ensures repaint on mouse enter/leave
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const bool hov  = underMouse();
        const bool down = isDown();

        // Background
        QColor bg(Theme::Color::BG_HEADER); // used by restore icon fill regardless of state
        if (m_type == Type::Close) {
            if (down)       bg = QColor("#7A1515");
            else if (hov)   bg = QColor("#5C1010");
        } else {
            if (down)       bg = QColor(Theme::Color::BG_ACTIVE);
            else if (hov)   bg = QColor(Theme::Color::BG_HOVER);
        }

        QPainter p(this);
        // Only paint a background on hover/press — otherwise let the title bar
        // background show through so the buttons are flush with the bar.
        if (hov || down)
            p.fillRect(rect(), bg);

        // Icon
        const QColor ink = (hov || down)
            ? QColor(Theme::Color::TEXT_BRIGHT)
            : QColor(Theme::Color::TEXT_LABEL);

        QPen pen(ink, 1);
        pen.setCosmetic(true);       // always 1 physical pixel regardless of DPI
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.setRenderHint(QPainter::Antialiasing, false); // sharp, grid-aligned

        // All icons drawn within an implicit 10×8 bounding box, centred in
        // the button. Button is 36×28, so centre is (18, 14).
        const int cx = width()  / 2;
        const int cy = height() / 2;

        switch (m_type) {

        case Type::Minimize:
            // Horizontal rule
            p.drawLine(cx - 5, cy, cx + 5, cy);
            break;

        case Type::Maximize:
            if (window() && window()->isMaximized()) {
                // Restore: two overlapping squares, back square offset (+4, -4)
                // 1. Draw back square (will be partially occluded in step 3)
                p.drawRect(cx - 1, cy - 7, 8, 8);   // back: top-left (17,7)

                // 2. Fill front area with bg so back square is hidden underneath
                p.fillRect(cx - 6, cy - 2, 9, 9, bg);

                // 3. Draw front square on top
                p.setPen(pen);
                p.drawRect(cx - 6, cy - 2, 8, 8);   // front: top-left (12,12)
            } else {
                // Maximise: plain square
                p.drawRect(cx - 5, cy - 4, 10, 8);
            }
            break;

        case Type::Close:
            // × — two diagonals through a 10×8 box
            p.drawLine(cx - 5, cy - 4, cx + 5, cy + 4);
            p.drawLine(cx + 5, cy - 4, cx - 5, cy + 4);
            break;
        }
    }

private:
    Type m_type;
};

// TitleBar

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void TitleBar::setupUi()
{
    setFixedHeight(Theme::Metric::TITLE_BAR_H);
    setObjectName("TitleBar");
    setStyleSheet(QString(
        "QWidget#TitleBar { background: %1; border-bottom: 1px solid %2; }"
    ).arg(Theme::Color::BG_HEADER, Theme::Color::BORDER));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(0);

    m_title = new QLabel(this);
    m_title->setStyleSheet(
        QString("color: %1; font-size: 8pt;").arg(Theme::Color::TEXT_LABEL));

    m_minimize = new WinButton(WinButton::Type::Minimize, this);
    m_maximize = new WinButton(WinButton::Type::Maximize, this);
    m_close    = new WinButton(WinButton::Type::Close,    this);

    layout->addWidget(m_title);
    layout->addStretch();
    layout->addWidget(m_minimize);
    layout->addWidget(m_maximize);
    layout->addWidget(m_close);

    connect(m_minimize, &QAbstractButton::clicked, this, [this] {
        window()->showMinimized();
    });
    connect(m_maximize, &QAbstractButton::clicked, this, [this] {
        toggleMaximize();
    });
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        window()->close();
    });
}

void TitleBar::toggleMaximize()
{
    if (window()->isMaximized())
        window()->showNormal();
    else
        window()->showMaximized();

    // Repaint the maximize button so it switches between □ and restore icon
    m_maximize->update();
}

void TitleBar::setTitle(const QString &title)
{
    m_title->setText(title.toUpper());
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        toggleMaximize();
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        if (auto *handle = window()->windowHandle())
            handle->startSystemMove();
}
