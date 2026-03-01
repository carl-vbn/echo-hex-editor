#pragma once
#include <QWidget>

class QLabel;
class QAbstractButton;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel          *m_title    = nullptr;
    QAbstractButton *m_minimize = nullptr;
    QAbstractButton *m_maximize = nullptr;
    QAbstractButton *m_close    = nullptr;

    void setupUi();
    void toggleMaximize();
};
