#ifndef SPEEDDIALOG_H
#define SPEEDDIALOG_H

#include <QDialog>

namespace Ui {
class SpeedDialog;
}

class SpeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeedDialog(QWidget *parent, double speed, double minSpeed, double maxSpeed, bool reversed);
    ~SpeedDialog();

    double getValue() const;

private:
    Ui::SpeedDialog *ui;
};

#endif // SPEEDDIALOG_H
