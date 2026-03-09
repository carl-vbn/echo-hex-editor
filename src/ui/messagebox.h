#pragma once
#include <QDialog>
#include <QMessageBox>

// Themed replacement for QMessageBox. Drop-in static API.
class MessageBox : public QDialog
{
    Q_OBJECT

public:
    static void critical(QWidget *parent, const QString &title, const QString &text);
    static void warning (QWidget *parent, const QString &title, const QString &text);
    static QMessageBox::StandardButton question(
        QWidget *parent,
        const QString &title,
        const QString &text,
        QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No);

private:
    explicit MessageBox(QWidget *parent, const QString &title, const QString &text,
                        QMessageBox::StandardButtons buttons);

    QMessageBox::StandardButton m_result = QMessageBox::NoButton;
};
