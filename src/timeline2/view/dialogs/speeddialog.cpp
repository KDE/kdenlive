#include "speeddialog.h"
#include "ui_clipspeed_ui.h"
#include <QPushButton>
#include <QDebug>
#include <QtMath>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, double minSpeed, double maxSpeed, bool reversed)
    : QDialog(parent)
    , ui(new Ui::ClipSpeed_UI)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->speedSpin->setDecimals(2);
    ui->speedSpin->setMinimum(minSpeed);
    ui->speedSpin->setMaximum(maxSpeed);
    ui->speedSlider->setMinimum(0);
    ui->speedSlider->setMaximum(100);
    ui->speedSlider->setTickInterval(10);
    ui->speedSpin->selectAll();
    ui->label_dest->setVisible(false);
    ui->kurlrequester->setVisible(false);
    ui->toolButton->setVisible(false);
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    ui->speedSpin->setValue(speed);
    ui->speedSlider->setValue(qLn(speed) * 12);
    connect(ui->speedSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&] (double value) {
        QSignalBlocker bk(ui->speedSlider);
        ui->speedSlider->setValue(qLn(value) * 12);
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
    connect(ui->speedSlider, &QSlider::valueChanged, [&] (int value) {
        double res = qExp(value / 12.);
        QSignalBlocker bk(ui->speedSpin);
        ui->speedSpin->setValue(res);
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(ui->speedSpin->value()));
    });
}

SpeedDialog::~SpeedDialog()
{
    delete ui;
}

double SpeedDialog::getValue() const
{
    double val = ui->speedSpin->value();
    if (ui->checkBox->checkState() == Qt::Checked) {
        val *= -1;
    }
    return val;
}
