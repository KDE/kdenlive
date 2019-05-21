#include "speeddialog.h"
#include "ui_speeddialog_ui.h"
#include <QPushButton>

SpeedDialog::SpeedDialog(QWidget *parent, double speed, double minSpeed, double maxSpeed, bool reversed)
    : QDialog(parent)
    , ui(new Ui::SpeedDialog)
{
    ui->setupUi(this);
    setWindowTitle(i18n("Clip Speed"));
    ui->doubleSpinBox->setDecimals(2);
    ui->doubleSpinBox->setMinimum(minSpeed);
    ui->doubleSpinBox->setMaximum(maxSpeed);
    ui->doubleSpinBox->setValue(speed);
    ui->doubleSpinBox->selectAll();
    if (reversed) {
        ui->checkBox->setChecked(true);
    }
    connect(ui->doubleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&] (double value) {
        ui->buttonBox->button((QDialogButtonBox::Ok))->setEnabled(!qFuzzyIsNull(value));
    });
}

SpeedDialog::~SpeedDialog()
{
    delete ui;
}

double SpeedDialog::getValue() const
{
    double val = ui->doubleSpinBox->value();
    if (ui->checkBox->checkState() == Qt::Checked) {
        val *= -1;
    }
    return val;
}
