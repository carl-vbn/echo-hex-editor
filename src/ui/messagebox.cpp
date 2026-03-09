#include "messagebox.h"
#include "theme/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

void MessageBox::critical(QWidget *parent, const QString &title, const QString &text)
{
    MessageBox dlg(parent, title, text, QMessageBox::Ok);
    dlg.exec();
}

void MessageBox::warning(QWidget *parent, const QString &title, const QString &text)
{
    MessageBox dlg(parent, title, text, QMessageBox::Ok);
    dlg.exec();
}

QMessageBox::StandardButton MessageBox::question(
    QWidget *parent, const QString &title, const QString &text,
    QMessageBox::StandardButtons buttons)
{
    MessageBox dlg(parent, title, text, buttons);
    dlg.exec();
    return dlg.m_result;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MessageBox::MessageBox(QWidget *parent, const QString &title, const QString &text,
                       QMessageBox::StandardButtons buttons)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setModal(true);
    setMinimumWidth(340);

    // Outer widget is BORDER_MID; 1px margin creates the border effect
    setStyleSheet(QString("MessageBox { background: %1; }").arg(Theme::Color::BORDER_MID));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    auto *content = new QWidget;
    content->setObjectName("msgContent");
    content->setStyleSheet(
        QString("QWidget#msgContent { background: %1; }").arg(Theme::Color::BG_PANEL));
    outer->addWidget(content);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // -- Title bar -----------------------------------------------------------
    auto *titleBar = new QLabel(title.toUpper());
    titleBar->setFixedHeight(22);
    titleBar->setContentsMargins(10, 0, 10, 0);
    titleBar->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    titleBar->setStyleSheet(QString(
        "background: %1; color: %2; border-bottom: 1px solid %3; font-size: 7pt;")
        .arg(Theme::Color::BG_HEADER, Theme::Color::TEXT_LABEL, Theme::Color::BORDER_MID));
    layout->addWidget(titleBar);

    // -- Message -------------------------------------------------------------
    auto *msgLabel = new QLabel(text);
    msgLabel->setWordWrap(true);
    msgLabel->setContentsMargins(14, 14, 14, 14);
    msgLabel->setStyleSheet(QString("color: %1; font-size: 9pt; background: transparent;")
        .arg(Theme::Color::TEXT));
    layout->addWidget(msgLabel);

    // -- Button row ----------------------------------------------------------
    auto *btnBar = new QWidget;
    btnBar->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;")
            .arg(Theme::Color::BG_HEADER, Theme::Color::BORDER_MID));

    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(10, 6, 10, 6);
    btnLayout->setSpacing(6);
    btnLayout->addStretch();

    const QString btnStyle = QString(R"(
        QPushButton {
            background: transparent; color: %1;
            border: 1px solid %2; font-size: 8pt; padding: 2px 14px;
            min-width: 48px;
        }
        QPushButton:hover   { background: %3; color: %4; border-color: %5; }
        QPushButton:pressed { background: %6; }
    )").arg(Theme::Color::TEXT,       Theme::Color::BORDER_MID,
            Theme::Color::BG_HOVER,   Theme::Color::TEXT_BRIGHT,
            Theme::Color::TEXT_LABEL, Theme::Color::BG_ACTIVE);

    // Add buttons in a fixed role order: Cancel < Discard/No < Yes/Save < Ok
    struct BtnDef { QMessageBox::StandardButton id; const char *label; };
    const BtnDef order[] = {
        { QMessageBox::Cancel,  "CANCEL"  },
        { QMessageBox::Discard, "DISCARD" },
        { QMessageBox::No,      "NO"      },
        { QMessageBox::Yes,     "YES"     },
        { QMessageBox::Save,    "SAVE"    },
        { QMessageBox::Ok,      "OK"      },
    };
    for (const auto &def : order) {
        if (!(buttons & def.id)) continue;
        auto *btn = new QPushButton(def.label);
        btn->setFixedHeight(22);
        btn->setStyleSheet(btnStyle);
        const QMessageBox::StandardButton capture = def.id;
        connect(btn, &QPushButton::clicked, this, [this, capture] {
            m_result = capture;
            accept();
        });
        btnLayout->addWidget(btn);
    }

    layout->addWidget(btnBar);
}
