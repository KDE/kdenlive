/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clippropertiescontroller.h"
#include "bincontroller.h"
#include "timecodedisplay.h"
#include "clipcontroller.h"
#include "effectstack/widgets/choosecolorwidget.h"
#include "dialogs/profilesdialog.h"

#include <KLocalizedString>

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QFontDatabase>

ClipPropertiesController::ClipPropertiesController(Timecode tc, ClipController *controller, QWidget *parent) : QWidget(parent)
    , m_id(controller->clipId())
    , m_type(controller->clipType())
    , m_properties(controller->properties())
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QVBoxLayout *vbox = new QVBoxLayout;
    if (m_type == Color || m_type == Image || m_type == AV || m_type == Video) {
        // Edit duration widget
        m_originalProperties.insert("out", m_properties.get("out"));
        m_originalProperties.insert("length", m_properties.get("length"));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Duration"), this);
        box->setObjectName("force_duration");
        hlay->addWidget(box);
        TimecodeDisplay *timePos = new TimecodeDisplay(tc, this);
        timePos->setObjectName("force_duration_value");
        timePos->setValue(m_properties.get_int("out") + 1);
        int original_length = m_properties.get_int("kdenlive:original_length");
        if (original_length > 0) {
            box->setChecked(true);
        }
        else timePos->setEnabled(false);
        hlay->addWidget(timePos);
        vbox->addLayout(hlay);
        connect(box, SIGNAL(toggled(bool)), timePos, SLOT(setEnabled(bool)));
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        connect(timePos, SIGNAL(timeCodeEditingFinished(int)), this, SLOT(slotDurationChanged(int)));
        connect(this, SIGNAL(updateTimeCodeFormat()), timePos, SLOT(slotUpdateTimeCodeFormat()));
        connect(this, SIGNAL(modified(int)), timePos, SLOT(setValue(int)));
    }
    if (m_type == Color) {
        // Edit color widget
        m_originalProperties.insert("resource", m_properties.get("resource"));
        mlt_color color = m_properties.get_color("resource");
        ChooseColorWidget *choosecolor = new ChooseColorWidget(i18n("Color"), QColor::fromRgb(color.r, color.g, color.b).name(), false, this);
        vbox->addWidget(choosecolor);
        //connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
        connect(choosecolor, SIGNAL(modified(QColor)), this, SLOT(slotColorModified(QColor)));
        connect(this, SIGNAL(modified(QColor)), choosecolor, SLOT(slotColorModified(QColor)));
    }
    if (m_type == AV || m_type == Video) {
        QLocale locale;
        QString force_fps = m_properties.get("force_fps");
        m_originalProperties.insert("force_fps", force_fps.isEmpty() ? "-" : force_fps);
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Frame rate"), this);
        box->setObjectName("force_fps");
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        QDoubleSpinBox *spin = new QDoubleSpinBox(this);
        connect(spin, SIGNAL(valueChanged(double)), this, SLOT(slotValueChanged(double)));
        spin->setObjectName("force_fps_value");
        if (force_fps.isEmpty()) {
            spin->setValue(controller->originalFps());
        }
        else {
            spin->setValue(locale.toDouble(force_fps));
        }
        connect(box, SIGNAL(toggled(bool)), spin, SLOT(setEnabled(bool)));
        box->setChecked(!force_fps.isEmpty());
        spin->setEnabled(!force_fps.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(spin);
        vbox->addLayout(hlay);

        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Colorspace"), this);
        box->setObjectName("force_colorspace");
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        QComboBox *combo = new QComboBox(this);
        combo->setObjectName("force_colorspace_value");
        combo->addItem(ProfilesDialog::getColorspaceDescription(601), 601);
        combo->addItem(ProfilesDialog::getColorspaceDescription(709), 709);
        combo->addItem(ProfilesDialog::getColorspaceDescription(240), 240);
        int force_colorspace = m_properties.get_int("force_colorspace");
        m_originalProperties.insert("force_colorspace", force_colorspace == 0 ? "-" : QString::number(force_colorspace));
        int colorspace = controller->videoCodecProperty("colorspace").toInt();
        if (force_colorspace > 0) {
            box->setChecked(true);
            combo->setEnabled(true);
            combo->setCurrentIndex(combo->findData(force_colorspace));
        } else if (colorspace > 0) {
            combo->setEnabled(false);
            combo->setCurrentIndex(combo->findData(colorspace));
        }
        else combo->setEnabled(false);
        connect(box, SIGNAL(toggled(bool)), combo, SLOT(setEnabled(bool)));
        hlay->addWidget(box);
        hlay->addWidget(combo);
        
        vbox->addLayout(hlay);
    }
    setLayout(vbox);
    vbox->addStretch(10);
}

ClipPropertiesController::~ClipPropertiesController()
{
}

void ClipPropertiesController::slotRefreshTimeCode()
{
    emit updateTimeCodeFormat();
}

void ClipPropertiesController::slotReloadProperties()
{
    if (m_type == Color) {
        m_originalProperties.insert("resource", m_properties.get("resource"));
        m_originalProperties.insert("out", m_properties.get("out"));
        m_originalProperties.insert("length", m_properties.get("length"));
        emit modified(m_properties.get_int("length"));
        mlt_color color = m_properties.get_color("resource");
        emit modified(QColor::fromRgb(color.r, color.g, color.b));
    }
}

void ClipPropertiesController::slotColorModified(QColor newcolor)
{
    QMap <QString, QString> properties;
    properties.insert("resource", newcolor.name(QColor::HexArgb));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotDurationChanged(int duration)
{
    QMap <QString, QString> properties;
    int original_length = m_properties.get_int("kdenlive:original_length");
    if (original_length == 0) {
        m_properties.set("kdenlive:original_length", m_properties.get_int("length"));
    }
    properties.insert("length", QString::number(duration));
    properties.insert("out", QString::number(duration - 1));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotEnableForce(int state)
{
    QCheckBox *box = qobject_cast<QCheckBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName();
    QMap <QString, QString> properties;
    QLocale locale;
    if (state == Qt::Unchecked) {
        // The force property was disable, remove it / reset default if necessary
        if (param == "force_duration") {
            // special case, reset original duration
            TimecodeDisplay *timePos = findChild<TimecodeDisplay *>(param + "_value");
            timePos->setValue(m_properties.get_int("kdenlive:original_length"));
            slotDurationChanged(m_properties.get_int("kdenlive:original_length"));
            m_properties.set("kdenlive:original_length", (char *) NULL);
            return;
        }
        else {
            properties.insert(param, "-");
        }
    } else {
        // A force property was set
        if (param == "force_duration") {
            int original_length = m_properties.get_int("kdenlive:original_length");
            if (original_length == 0) {
                m_properties.set("kdenlive:original_length", m_properties.get_int("length"));
            }
        }
        if (param == "force_fps") {
            QDoubleSpinBox *spin = findChild<QDoubleSpinBox *>(param + "_value");
            if (!spin) return;
            properties.insert(param, locale.toString(spin->value()));
        }
        if (param == "force_colorspace") {
            QComboBox *combo = findChild<QComboBox *>(param + "_value");
            if (!combo) return;
            properties.insert(param, QString::number(combo->currentData().toInt()));
        }
    }
    if (properties.isEmpty()) return;
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotValueChanged(double value)
{
    QDoubleSpinBox *box = qobject_cast<QDoubleSpinBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section("_", 0, -2);
    QMap <QString, QString> properties;
    QLocale locale;
    properties.insert(param, locale.toString(value));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

