#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <QWidget>

#include "ui_waveform_ui.h"

class Waveform_UI;

class Waveform : public QWidget, public Ui::Waveform_UI {
    Q_OBJECT

public:
    Waveform(QWidget *parent = 0);
    ~Waveform();

};

#endif // WAVEFORM_H
