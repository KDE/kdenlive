#ifndef SPEEDDIALOG_H
#define SPEEDDIALOG_H

#include <QDialog>

namespace Ui {
class ClipSpeed_UI;
}

class SpeedDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeedDialog(QWidget *parent, double speed, double minSpeed, double maxSpeed, bool reversed);
    ~SpeedDialog();

    double getValue() const;

private:
    Ui::ClipSpeed_UI *ui;
};

#endif // SPEEDDIALOG_H
