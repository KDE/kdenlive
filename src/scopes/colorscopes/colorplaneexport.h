/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/**
  Exports color planes (e.g. YUV-UV-planes) to a file.
  Basically just for fun, but also for comparing color models.
 */

#ifndef COLORPLANEEXPORT_H
#define COLORPLANEEXPORT_H

#include "colortools.h"
#include "ui_colorplaneexport_ui.h"
#include <QDialog>

class ColorPlaneExport_UI;

class ColorPlaneExport : public QDialog, public Ui::ColorPlaneExport_UI
{
    Q_OBJECT
public:
    explicit ColorPlaneExport(QWidget *parent = nullptr);
    ~ColorPlaneExport() override;

    enum COLOR_EXPORT_MODE { CPE_YUV, CPE_YUV_Y, CPE_YUV_MOD, CPE_RGB_CURVE, CPE_YPbPr, CPE_HSV_HUESHIFT, CPE_HSV_SATURATION };

private:
    ColorTools *m_colorTools;
    float m_scaling;
    void enableSliderScaling(bool enable);
    void enableSliderColor(bool enable);
    void enableCbVariant(bool enable);

private slots:
    void slotValidate();
    void slotExportPlane();
    void slotColormodeChanged();
    void slotUpdateDisplays();
};

#endif // COLORPLANEEXPORT_H
