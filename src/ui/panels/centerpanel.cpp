#include "centerpanel.h"
#include "theme/theme.h"

#include <QVBoxLayout>
#include <QLabel>

CenterPanel::CenterPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void CenterPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *placeholder = new QLabel("NO FILE OPEN");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QString("color: %1; font-size: 9pt;")
        .arg(Theme::Color::TEXT_DIM));

    layout->addWidget(placeholder);
}
