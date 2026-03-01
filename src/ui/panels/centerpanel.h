#pragma once
#include <QWidget>

class CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CenterPanel(QWidget *parent = nullptr);

private:
    void setupUi();
};
