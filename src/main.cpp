#include <QApplication>
#include <QFontDatabase>
#include "ui/mainwindow.h"
#include "theme/theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Echo");
    app.setApplicationVersion("0.1");

    // Load OCR-B font from embedded resources
    int fontId = QFontDatabase::addApplicationFont(":/fonts/OCR-B.ttf");
    QString fontFamily = "Monospace"; // fallback
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            fontFamily = families.first();
    }

    QFont defaultFont(fontFamily, 9);
    app.setFont(defaultFont);
    app.setStyleSheet(Theme::appStyleSheet(fontFamily));

    MainWindow window;
    window.show();

    return app.exec();
}
