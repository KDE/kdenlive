#include "speeddialog.h"
#include "ui_clipspeed_ui.h"
#include <QPushButton>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, double minSpeed, double maxSpeed, bool reversed)
    : QDialog(parent)
    , ui(new Ui::ClipSpeed_UI)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->speedSpin->setDecimals(2);
    ui->speedSpin->setMinimum(minSpeed);
    ui->speedSpin->setMaximum(maxSpeed);
    ui->speedSlider->setMinimum(100 * minSpeed);
    ui->speedSlider->setMaximum(qMin(100 * minSpeed + 100000, 100 *maxSpeed));
    ui->speedSpin->setValue(speed);
    ui->speedSlider->setValue(100 * speed);
    ui->speedSlider->setTickInterval((ui->speedSlider->maximum() - ui->speedSlider->minimum()) / 10);
    ui->speedSpin->selectAll();
    ui->label_dest->setVisible(false);
    ui->kurlrequester->setVisible(false);
    ui->toolButton->setVisible(false);
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    connect(ui->speedSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&] (double value) {
        ui->speedSlider->setValue(100 * value);
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
    connect(ui->speedSlider, &QSlider::valueChanged, [&] (int value) {
        ui->speedSpin->setValue(value / 100.);
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(value != 0);
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
