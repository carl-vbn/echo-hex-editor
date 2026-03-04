#include <QApplication>
#include <QFontDatabase>
#include <QStyleFactory>
#include "ui/mainwindow.h"
#include "theme/theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Echo");
    app.setApplicationVersion("0.1");
    app.setStyle(QStyleFactory::create("Fusion"));

    int fontId = QFontDatabase::addApplicationFont(":/fonts/OCR-B.ttf");
    QString fallbackFont = "Monospace";
    if (fontId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            fallbackFont = families.first();
    }

    QFont defaultFont(fallbackFont, 9);
    app.setFont(defaultFont);
    app.setStyleSheet(Theme::appStyleSheet(fallbackFont));
    
    MainWindow window;
    window.show();

    return app.exec();
}
