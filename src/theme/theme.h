#pragma once
#include <QString>

class QComboBox;

namespace Theme {

namespace Color {
    constexpr char BG_WINDOW[]   = "#080808";
    constexpr char BG_PANEL[]    = "#0C0C0C";
    constexpr char BG_HEADER[]   = "#111111";
    constexpr char BG_HOVER[]    = "#181818";
    constexpr char BG_ACTIVE[]   = "#202020";
    constexpr char BORDER[]      = "#181818";
    constexpr char BORDER_MID[]  = "#282828";
    constexpr char TEXT[]        = "#B0B0B0";
    constexpr char TEXT_DIM[]    = "#363636";
    constexpr char TEXT_LABEL[]  = "#545454";
    constexpr char TEXT_BRIGHT[] = "#FFFFFF";
}

namespace Metric {
    constexpr int TITLE_BAR_H    = 28;
    constexpr int TOOLBAR_H      = 28;
    constexpr int PANEL_MIN_W    = 180;
    constexpr int RESIZE_MARGIN  = 5;
}

QString appStyleSheet(const QString &fontFamily);
void polishComboBox(QComboBox *combo);

} // namespace Theme
