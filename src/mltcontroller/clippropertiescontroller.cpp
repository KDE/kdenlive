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
#include "timeline/markerdialog.h"
#include "timecodedisplay.h"
#include "clipcontroller.h"
#include "kdenlivesettings.h"
#include "effectstack/widgets/choosecolorwidget.h"
#include "dialogs/profilesdialog.h"
#include "utils/KoIconUtils.h"

#include <KLocalizedString>
#include <KIO/Global>

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFile>
#include <QProcess>
#include <QCheckBox>
#include <QFontDatabase>
#include <QToolBar>
#include <QFileDialog>

ClipPropertiesController::ClipPropertiesController(Timecode tc, ClipController *controller, QWidget *parent) : QTabWidget(parent)
    , m_controller(controller)
    , m_tc(tc)
    , m_id(controller->clipId())
    , m_type(controller->clipType())
    , m_properties(controller->properties())
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_forcePage = new QWidget(this);
    m_propertiesPage = new QWidget(this);
    m_markersPage = new QWidget(this);
    m_metaPage = new QWidget(this);
    m_analysisPage = new QWidget(this);

    // Clip properties
    QVBoxLayout *propsBox = new QVBoxLayout;
    QTreeWidget *propsTree = new QTreeWidget;
    propsTree->setRootIsDecorated(false);
    propsTree->setColumnCount(2);
    propsTree->setAlternatingRowColors(true);
    propsTree->setHeaderHidden(true);
    propsBox->addWidget(propsTree);
    fillProperties(propsTree);
    m_propertiesPage->setLayout(propsBox);

    // Clip markers
    QVBoxLayout *mBox = new QVBoxLayout;
    m_markerTree = new QTreeWidget;
    m_markerTree->setRootIsDecorated(false);
    m_markerTree->setColumnCount(2);
    m_markerTree->setAlternatingRowColors(true);
    m_markerTree->setHeaderHidden(true);
    mBox->addWidget(m_markerTree);
    QToolBar *bar = new QToolBar;
    bar->addAction(KoIconUtils::themedIcon("document-new"), i18n("Add marker"), this, SLOT(slotAddMarker()));
    bar->addAction(KoIconUtils::themedIcon("trash-empty"), i18n("Delete marker"), this, SLOT(slotDeleteMarker()));
    bar->addAction(KoIconUtils::themedIcon("document-properties"), i18n("Edit marker"), this, SLOT(slotEditMarker()));
    bar->addAction(KoIconUtils::themedIcon("document-save-as"), i18n("Export markers"), this, SLOT(slotSaveMarkers()));
    bar->addAction(KoIconUtils::themedIcon("document-open"), i18n("Import markers"), this, SLOT(slotLoadMarkers()));
    mBox->addWidget(bar);

    slotFillMarkers();
    m_markersPage->setLayout(mBox);
    connect(m_markerTree, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotSeekToMarker()));

    // metadata
    QVBoxLayout *m2Box = new QVBoxLayout;
    QTreeWidget *metaTree = new QTreeWidget;
    metaTree->setRootIsDecorated(true);
    metaTree->setColumnCount(2);
    metaTree->setAlternatingRowColors(true);
    metaTree->setHeaderHidden(true);
    m2Box->addWidget(metaTree);
    slotFillMeta(metaTree);
    m_metaPage->setLayout(m2Box );

    // Clip analysis
    QVBoxLayout *aBox = new QVBoxLayout;
    m_analysisTree = new QTreeWidget;
    m_analysisTree->setRootIsDecorated(false);
    m_analysisTree->setColumnCount(2);
    m_analysisTree->setAlternatingRowColors(true);
    m_analysisTree->setHeaderHidden(true);
    m_analysisTree->setDragEnabled(true);
    aBox ->addWidget(new QLabel(i18n("Analysis data")));
    aBox ->addWidget(m_analysisTree);
    QToolBar *bar2 = new QToolBar;
    bar2->addAction(KoIconUtils::themedIcon("trash-empty"), i18n("Delete analysis"), this, SLOT(slotDeleteAnalysis()));
    bar2->addAction(KoIconUtils::themedIcon("document-save-as"), i18n("Export analysis"), this, SLOT(slotSaveAnalysis()));
    bar2->addAction(KoIconUtils::themedIcon("document-open"), i18n("Import analysis"), this, SLOT(slotLoadAnalysis()));
    aBox->addWidget(bar2);

    slotFillAnalysisData();
    m_analysisPage->setLayout(aBox );

    // Force properties
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
    if (m_type == Image) {
        int transparency = m_properties.get_int("kdenlive:transparency");
        m_originalProperties.insert("kdenlive:transparency", QString::number(transparency));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Transparent"), this);
        box->setObjectName("kdenlive:transparency");
        box->setChecked(transparency == 1);
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        hlay->addWidget(box);
        vbox->addLayout(hlay);
    }
    
    if (m_type == AV || m_type == Video || m_type == Image) {
        // Aspect ratio
        int force_ar_num = m_properties.get_int("force_aspect_num");
        int force_ar_den  = m_properties.get_int("force_aspect_den");
        m_originalProperties.insert("force_aspect_den", (force_ar_den == 0) ? QString() : QString::number(force_ar_den));
        m_originalProperties.insert("force_aspect_num", (force_ar_num == 0) ? QString() : QString::number(force_ar_num));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Aspect Ratio"), this);
        box->setObjectName("force_ar");
        vbox->addWidget(box);
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        QSpinBox *spin1 = new QSpinBox(this);
        spin1->setMaximum(8000);
        spin1->setObjectName("force_aspect_num_value");
        hlay->addWidget(spin1);
        hlay->addWidget(new QLabel(":"));
        QSpinBox *spin2 = new QSpinBox(this);
        spin2->setMinimum(1);
        spin2->setMaximum(8000);
        spin2->setObjectName("force_aspect_den_value");
        hlay->addWidget(spin2);
        if (force_ar_num == 0) {
            // calculate current ratio
            box->setChecked(false);
            spin1->setEnabled(false);
            spin2->setEnabled(false);
            int width = m_controller->profile()->width();
            int height = m_controller->profile()->height();
            if (width > 0 && height > 0) {
                if (qAbs(100 * width / height - 133) < 3) {
                    width = 4;
                    height = 3;
                }
                else if (qAbs(100 * width / height - 177) < 3) {
                    width = 16;
                    height = 9;
                }
                spin1->setValue(width);
                spin2->setValue(height);
            }
        }
        else {
            box->setChecked(true);
            spin1->setEnabled(true);
            spin2->setEnabled(true);
            spin1->setValue(force_ar_num);
            spin2->setValue(force_ar_den);
        }
        connect(spin2, SIGNAL(valueChanged(int)), this, SLOT(slotAspectValueChanged(int)));
        connect(spin1, SIGNAL(valueChanged(int)), this, SLOT(slotAspectValueChanged(int)));
        connect(box, SIGNAL(toggled(bool)), spin1, SLOT(setEnabled(bool)));
        connect(box, SIGNAL(toggled(bool)), spin2, SLOT(setEnabled(bool)));
        vbox->addLayout(hlay);
        }
    
    if (m_type == AV || m_type == Video) {
        QLocale locale;

        // Fps
        QString force_fps = m_properties.get("force_fps");
        m_originalProperties.insert("force_fps", force_fps.isEmpty() ? "-" : force_fps);
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Frame rate"), this);
        box->setObjectName("force_fps");
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        QDoubleSpinBox *spin = new QDoubleSpinBox(this);
        spin->setMaximum(1000);
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


        // Scanning
        QString force_prog = m_properties.get("force_progressive");
        m_originalProperties.insert("force_progressive", force_prog.isEmpty() ? "-" : force_prog);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Scanning"), this);
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        box->setObjectName("force_progressive");
        QComboBox *combo = new QComboBox(this);
        combo->addItem(i18n("Interlaced"), 0);
        combo->addItem(i18n("Progressive"), 1);
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        combo->setObjectName("force_progressive_value");
        if (!force_prog.isEmpty()) {
            combo->setCurrentIndex(force_prog.toInt());
        }
        connect(box, SIGNAL(toggled(bool)), combo, SLOT(setEnabled(bool)));
        box->setChecked(!force_prog.isEmpty());
        combo->setEnabled(!force_prog.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);
        
        // Field order
        QString force_tff = m_properties.get("force_tff");
        m_originalProperties.insert("force_tff", force_tff.isEmpty() ? "-" : force_tff);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Field order"), this);
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        box->setObjectName("force_tff");
        combo = new QComboBox(this);
        combo->addItem(i18n("Bottom first"), 0);
        combo->addItem(i18n("Top first"), 1);
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        combo->setObjectName("force_tff_value");
        if (!force_tff.isEmpty()) {
            combo->setCurrentIndex(force_tff.toInt());
        }
        connect(box, SIGNAL(toggled(bool)), combo, SLOT(setEnabled(bool)));
        box->setChecked(!force_tff.isEmpty());
        combo->setEnabled(!force_tff.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);
        
        //Decoding threads
        QString threads = m_properties.get("threads");
        m_originalProperties.insert("threads", threads);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Threads")));
        QSpinBox *spinI = new QSpinBox(this);
        spinI->setMaximum(4);
        spinI->setObjectName("threads_value");
        if (!threads.isEmpty()) {
            spinI->setValue(threads.toInt());
        }
        else spinI->setValue(1);
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        //Video index
        QString vix = m_properties.get("video_index");
        m_originalProperties.insert("video_index", vix);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Video index")));
        spinI = new QSpinBox(this);
        spinI->setMaximum(m_clipProperties.value("video_max").toInt());
        spinI->setObjectName("video_index_value");
        if (vix.isEmpty()) {
            spinI->setValue(0);
        }
        else spinI->setValue(vix.toInt());
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);
        
        //Audio index
        QString aix = m_properties.get("audio_index");
        m_originalProperties.insert("audio_index", aix);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Audio index")));
        spinI = new QSpinBox(this);
        spinI->setMaximum(m_clipProperties.value("audio_max").toInt());
        spinI->setObjectName("audio_index_value");
        if (aix.isEmpty()) {
            spinI->setValue(0);
        }
        else spinI->setValue(aix.toInt());
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        // Colorspace
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Colorspace"), this);
        box->setObjectName("force_colorspace");
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        combo = new QComboBox(this);
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
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);
        
        //Full luma
        QString force_luma = m_properties.get("set.force_full_luma");
        m_originalProperties.insert("set.force_full_luma", force_luma);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Full luma range"), this);
        connect(box, SIGNAL(stateChanged(int)), this, SLOT(slotEnableForce(int)));
        box->setObjectName("set.force_full_luma");
        box->setChecked(!force_luma.isEmpty());
        hlay->addWidget(box);
        vbox->addLayout(hlay);
    }
    m_forcePage->setLayout(vbox);
    vbox->addStretch(10);
    addTab(m_propertiesPage, QString());
    addTab(m_markersPage, QString());
    addTab(m_forcePage, QString());
    addTab(m_metaPage, QString());
    addTab(m_analysisPage, QString());
    setTabIcon(0, KoIconUtils::themedIcon("edit-find"));
    setTabToolTip(0, i18n("Properties"));
    setTabIcon(1, KoIconUtils::themedIcon("bookmark-toolbar"));
    setTabToolTip(1, i18n("Markers"));
    setTabIcon(2, KoIconUtils::themedIcon("document-edit"));
    setTabToolTip(2, i18n("Force properties"));
    setTabIcon(3, KoIconUtils::themedIcon("view-list-details"));
    setTabToolTip(3, i18n("Metadata"));
    setTabIcon(4, KoIconUtils::themedIcon("archive-extract"));
    setTabToolTip(4, i18n("Analysis"));
    setCurrentIndex(KdenliveSettings::properties_panel_page());
    if (m_type == Color) setTabEnabled(0, false);
}

ClipPropertiesController::~ClipPropertiesController()
{
    KdenliveSettings::setProperties_panel_page(currentIndex());
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
        else if (param == "kdenlive:transparency") {
            properties.insert(param, QString());
        }
        else if (param == "force_ar") {
            properties.insert("force_aspect_den", QString());
            properties.insert("force_aspect_num", QString());
            properties.insert("force_aspect_ratio", QString());
        }
        else {
            properties.insert(param, QString());
        }
    } else {
        // A force property was set
        if (param == "force_duration") {
            int original_length = m_properties.get_int("kdenlive:original_length");
            if (original_length == 0) {
                m_properties.set("kdenlive:original_length", m_properties.get_int("length"));
            }
        }
        else if (param == "force_fps") {
            QDoubleSpinBox *spin = findChild<QDoubleSpinBox *>(param + "_value");
            if (!spin) return;
            properties.insert(param, locale.toString(spin->value()));
        }
        else if (param == "threads") {
            QSpinBox *spin = findChild<QSpinBox *>(param + "_value");
            if (!spin) return;
            properties.insert(param, QString::number(spin->value()));
        }
        else if (param == "force_colorspace"  || param == "force_progressive" || param == "force_tff") {
            QComboBox *combo = findChild<QComboBox *>(param + "_value");
            if (!combo) return;
            properties.insert(param, QString::number(combo->currentData().toInt()));
        }
        else if (param == "kdenlive:transparency" || param == "set.force_full_luma") {
            properties.insert(param, "1");
        }
        else if (param == "force_ar") {
            QSpinBox *spin = findChild<QSpinBox *>("force_aspect_num_value");
            QSpinBox *spin2 = findChild<QSpinBox *>("force_aspect_den_value");
            if (!spin || !spin2) return;
            properties.insert("force_aspect_den", QString::number(spin2->value()));
            properties.insert("force_aspect_num", QString::number(spin->value()));
            QLocale locale;
            properties.insert("force_aspect_ratio", locale.toString((double) spin->value() / spin2->value()));
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

void ClipPropertiesController::slotValueChanged(int value)
{
    QSpinBox *box = qobject_cast<QSpinBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section("_", 0, -2);
    QMap <QString, QString> properties;
    properties.insert(param, QString::number(value));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotAspectValueChanged(int)
{
    QSpinBox *spin = findChild<QSpinBox *>("force_aspect_num_value");
    QSpinBox *spin2 = findChild<QSpinBox *>("force_aspect_den_value");
    if (!spin || !spin2) return;
    QMap <QString, QString> properties;
    properties.insert("force_aspect_den", QString::number(spin2->value()));
    properties.insert("force_aspect_num", QString::number(spin->value()));
    QLocale locale;
    properties.insert("force_aspect_ratio", locale.toString((double) spin->value() / spin2->value()));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotComboValueChanged()
{
    QComboBox *box = qobject_cast<QComboBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section("_", 0, -2);
    QMap <QString, QString> properties;
    QLocale locale;
    properties.insert(param, QString::number(box->currentData().toInt()));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::fillProperties(QTreeWidget *tree)
{
    m_clipProperties.clear();
    QList <QStringList> propertyMap;
    // Get the video_index
    if (m_type == AV || m_type == Video || m_type == Audio) {
        int vindex = m_controller->int_property("video_index");
        int video_max = 0;
        int default_audio = m_controller->int_property("audio_index");
        int audio_max = 0;

        // Find maximum stream index values
        for (int ix = 0; ix < m_controller->int_property("meta.media.nb_streams"); ++ix) {
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
            QString type = m_controller->property(property);
            if (type == "video")
                video_max = ix;
            else if (type == "audio")
                audio_max = ix;
        }
        m_clipProperties.insert("default_video", QString::number(vindex));
        m_clipProperties.insert("video_max", QString::number(video_max));
        m_clipProperties.insert("default_audio", QString::number(default_audio));
        m_clipProperties.insert("audio_max", QString::number(audio_max));

        if (vindex > -1) {
            // We have a video stream
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", vindex);
            QString codec = m_controller->property(property);
            if (!codec.isEmpty()) {
                propertyMap.append(QStringList() << i18n("Video codec") << codec);
            }
            int width = m_controller->int_property("meta.media.width");
            int height = m_controller->int_property("meta.media.height");
            propertyMap.append(QStringList() << i18n("Frame size") << QString::number(width) + "x" + QString::number(height));

            snprintf(property, sizeof(property), "meta.media.%d.stream.frame_rate", vindex);
            double fps = m_controller->double_property(property);
            if (fps > 0) {
                    propertyMap.append(QStringList() << i18n("Frame rate") << QString::number(fps, 'g', 3));
            } else {
                int rate_den = m_controller->int_property("meta.media.frame_rate_den");
                if (rate_den > 0) {
                    double fps = (double) m_controller->int_property("meta.media.frame_rate_num") / rate_den;
                    propertyMap.append(QStringList() << i18n("Frame rate") << QString::number(fps, 'g', 3));
                }
            }

            snprintf(property, sizeof(property), "meta.media.%d.codec.bit_rate", vindex);
            int bitrate = m_controller->int_property(property);
            if (bitrate > 0) {
                propertyMap.append(QStringList() << i18n("Video bitrate") << KIO::convertSize(bitrate) + "/" + i18nc("seconds", "s"));
            }

            int scan = m_controller->int_property("meta.media.progressive");
            propertyMap.append(QStringList() << i18n("Scanning") << (scan == 1 ? i18n("Progressive") : i18n("Interlaced")));
            snprintf(property, sizeof(property), "meta.media.%d.codec.sample_aspect_ratio", vindex);
            double par = m_controller->double_property(property);
            if (par == 0) {
                // Read media aspect ratio
                par = m_controller->double_property("aspect_ratio");
            }
            propertyMap.append(QStringList() << i18n("Pixel aspect ratio") << QString::number(par, 'g', 3));
            propertyMap.append(QStringList() << i18n("Pixel format") << m_controller->videoCodecProperty("pix_fmt"));
            int colorspace = m_controller->videoCodecProperty("colorspace").toInt();
            propertyMap.append(QStringList() << i18n("Colorspace") << ProfilesDialog::getColorspaceDescription(colorspace));
        }
        if (default_audio > -1) {
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", default_audio);
            QString codec = m_controller->property(property);
            if (!codec.isEmpty()) {
                propertyMap.append(QStringList() << i18n("Audio codec") << codec);
            }
            snprintf(property, sizeof(property), "meta.media.%d.codec.channels", default_audio);
            int channels = m_controller->int_property(property);
            propertyMap.append(QStringList() << i18n("Audio channels") << QString::number(channels));

            snprintf(property, sizeof(property), "meta.media.%d.codec.sample_rate", default_audio);
            int srate = m_controller->int_property(property);
            propertyMap.append(QStringList() << i18n("Audio frequency") << QString::number(srate));

            snprintf(property, sizeof(property), "meta.media.%d.codec.bit_rate", default_audio);
            int bitrate = m_controller->int_property(property);
            if (bitrate > 0) {
                propertyMap.append(QStringList() << i18n("Audio bitrate") << KIO::convertSize(bitrate) + "/" + i18nc("seconds", "s"));
            }
        }
    }

    for (int i = 0; i < propertyMap.count(); i++) {
        new QTreeWidgetItem(tree, propertyMap.at(i));
    }
    tree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotFillMarkers()
{
    m_markerTree->clear();
    QList <CommentedTime> markers = m_controller->commentedSnapMarkers();
    for (int count = 0; count < markers.count(); ++count) {
        QString time = m_tc.getTimecode(markers.at(count).time());
        QStringList itemtext;
        itemtext << time << markers.at(count).comment();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_markerTree, itemtext);
        item->setData(0, Qt::DecorationRole, CommentedTime::markerColor(markers.at(count).markerType()));
        item->setData(0, Qt::UserRole, QVariant((int) markers.at(count).time().frames(m_tc.fps())));
    }
    m_markerTree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotSeekToMarker()
{
    QTreeWidgetItem *item = m_markerTree->currentItem();
    int time = item->data(0, Qt::UserRole).toInt();
    emit seekToFrame(time);
}

void ClipPropertiesController::slotEditMarker()
{
    QList < CommentedTime > marks = m_controller->commentedSnapMarkers();
    int pos = m_markerTree->currentIndex().row();
    if (pos < 0 || pos > marks.count() - 1) return;
    QPointer<MarkerDialog> d = new MarkerDialog(m_controller, marks.at(pos), m_tc, i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        QList <CommentedTime> markers;
        markers << d->newMarker();
        emit addMarkers(m_id, markers);
    }
    delete d;
}

void ClipPropertiesController::slotDeleteMarker()
{
    QList < CommentedTime > marks = m_controller->commentedSnapMarkers();
    QList < CommentedTime > toDelete;
    for (int i = 0; i < marks.count(); ++i) {
        if (m_markerTree->topLevelItem(i)->isSelected()) {
            CommentedTime marker = marks.at(i);
            marker.setMarkerType(-1);
            toDelete << marker;
        }
    }
    emit addMarkers(m_id, toDelete);
}

void ClipPropertiesController::slotAddMarker()
{
    CommentedTime marker(GenTime(m_controller->originalProducer().position(), m_tc.fps()), i18n("Marker"));
    QPointer<MarkerDialog> d = new MarkerDialog(m_controller, marker,
                                                m_tc, i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        QList <CommentedTime> markers;
        markers << d->newMarker();
        emit addMarkers(m_id, markers);
    }
    delete d;
}

void ClipPropertiesController::slotSaveMarkers()
{
    emit saveMarkers(m_id);
}

void ClipPropertiesController::slotLoadMarkers()
{
    emit loadMarkers(m_id);
}

void ClipPropertiesController::slotFillMeta(QTreeWidget *tree)
{
    tree->clear();
    if (m_type != AV && m_type != Video && m_type != Image) {
        // Currently, we only use exiftool on video files
        return;
    }
    int exifUsed = m_controller->int_property("kdenlive:exiftool");
    if (exifUsed == 1) {
          Mlt::Properties subProperties;
          subProperties.pass_values(m_properties, "kdenlive:meta.exiftool.");
          if (subProperties.count() > 0) {
              QTreeWidgetItem *exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
              exif->setExpanded(true);
              for (int i = 0; i < subProperties.count(); i++) {
                  new QTreeWidgetItem(exif, QStringList() << subProperties.get_name(i) << subProperties.get(i));
              }
          }
    }
    else if (KdenliveSettings::use_exiftool()) {
        QString url = m_controller->clipUrl().path();
        //Check for Canon THM file
        url = url.section('.', 0, -2) + ".THM";
        if (QFile::exists(url)) {
            // Read the exif metadata embedded in the THM file
            QProcess p;
            QStringList args;
            args << "-g" << "-args" << url;
            p.start("exiftool", args);
            p.waitForFinished();
            QString res = p.readAllStandardOutput();
            m_controller->setProperty("kdenlive:exiftool", 1);
            QTreeWidgetItem *exif = NULL;
            QStringList list = res.split('\n');
            foreach(const QString &tagline, list) {
                if (tagline.startsWith(QLatin1String("-File")) || tagline.startsWith(QLatin1String("-ExifTool"))) continue;
                QString tag = tagline.section(':', 1).simplified();
                if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) continue;
                if (!tag.section('=', 0, 0).isEmpty() && !tag.section('=', 1).simplified().isEmpty()) {
                    if (!exif) {
                        exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
                        exif->setExpanded(true);
                    }
                    m_controller->setProperty("kdenlive:meta.exiftool." + tag.section('=', 0, 0), tag.section('=', 1).simplified());
                    new QTreeWidgetItem(exif, QStringList() << tag.section('=', 0, 0) << tag.section('=', 1).simplified());
                }
            }
        } else {
            if (m_type == Image || m_controller->codec(false) == "h264") {
                QProcess p;
                QStringList args;
                args << "-g" << "-args" << m_controller->clipUrl().path();
                p.start("exiftool", args);
                p.waitForFinished();
                QString res = p.readAllStandardOutput();
                if (m_type != Image) {
                    m_controller->setProperty("kdenlive:exiftool", 1);
                }
                QTreeWidgetItem *exif = NULL;
                QStringList list = res.split('\n');
                foreach(const QString &tagline, list) {
                    if (m_type != Image && !tagline.startsWith(QLatin1String("-H264"))) continue;
                    QString tag = tagline.section(':', 1);
                    if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) continue;
                    if (!exif) {
                        exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
                        exif->setExpanded(true);
                    }
                    if (m_type != Image) {
                        // Do not store image exif metadata in project file, would be too much noise
                        m_controller->setProperty("kdenlive:meta.exiftool." + tag.section('=', 0, 0), tag.section('=', 1).simplified());
                    }
                    new QTreeWidgetItem(exif, QStringList() << tag.section('=', 0, 0) << tag.section('=', 1).simplified());
                }
            }
        }
    }
    int magic = m_controller->int_property("kdenlive:magiclantern");
    if (magic == 1) {
        Mlt::Properties subProperties;
        subProperties.pass_values(m_properties, "kdenlive:meta.magiclantern.");
        QTreeWidgetItem *magicL = NULL;
        for (int i = 0; i < subProperties.count(); i++) {
            if (!magicL) {
                magicL = new QTreeWidgetItem(tree, QStringList() << i18n("Magic Lantern") << QString());
                QIcon icon(QStandardPaths::locate(QStandardPaths::DataLocation, "meta_magiclantern.png"));
                magicL->setIcon(0, icon);
                magicL->setExpanded(true);
            }
            new QTreeWidgetItem(magicL, QStringList() << subProperties.get_name(i) << subProperties.get(i));
        }
    }
    else if (m_type != Image && KdenliveSettings::use_magicLantern()) {
        QString url = m_controller->clipUrl().path();
        url = url.section('.', 0, -2) + ".LOG";
        if (QFile::exists(url)) {
            QFile file(url);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_controller->setProperty("kdenlive:magiclantern", 1);
                QTreeWidgetItem *magicL = NULL;
                while (!file.atEnd()) {
                    QString line = file.readLine().simplified();
                    if (line.startsWith('#') || line.isEmpty() || !line.contains(':')) continue;
                    if (line.startsWith(QLatin1String("CSV data"))) break;
                    m_controller->setProperty("kdenlive:meta.magiclantern." + line.section(':', 0, 0).simplified(), line.section(':', 1).simplified());
                    if (!magicL) {
                        magicL = new QTreeWidgetItem(tree, QStringList() << i18n("Magic Lantern") << QString());
                        QIcon icon(QStandardPaths::locate(QStandardPaths::DataLocation, "meta_magiclantern.png"));
                        magicL->setIcon(0, icon);
                        magicL->setExpanded(true);
                    }
                    new QTreeWidgetItem(magicL, QStringList() << line.section(':', 0, 0).simplified() << line.section(':', 1).simplified());
                }
            }
        }

        //if (!meta.isEmpty())
        //clip->setMetadata(meta, "Magic Lantern");
        //clip->setProperty("magiclantern", "1");
    }
    tree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotFillAnalysisData()
{
    m_analysisTree->clear();
    Mlt::Properties subProperties;
    subProperties.pass_values(m_properties, "kdenlive:clipanalysis.");
    if (subProperties.count() > 0) {
        for (int i = 0; i < subProperties.count(); i++) {
            new QTreeWidgetItem(m_analysisTree, QStringList() << subProperties.get_name(i) << subProperties.get(i));
        }
    }
    m_analysisTree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotDeleteAnalysis()
{
    QTreeWidgetItem *current = m_analysisTree->currentItem();
    if (!current) return;
    emit editAnalysis(m_id, "kdenlive:clipanalysis." + current->text(0), QString());
}

void ClipPropertiesController::slotSaveAnalysis()
{
    const QString url = QFileDialog::getSaveFileName(this, i18n("Save Analysis Data"), m_controller->clipUrl().adjusted(QUrl::RemoveFilename).path(), i18n("Text File (*.txt)"));
    if (url.isEmpty())
        return;
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup analysisConfig(config, "Analysis");
    QTreeWidgetItem *current = m_analysisTree->currentItem();
    analysisConfig.writeEntry(current->text(0), current->text(1));
}

void ClipPropertiesController::slotLoadAnalysis()
{
    const QString url = QFileDialog::getOpenFileName(this, i18n("Open Analysis Data"), m_controller->clipUrl().adjusted(QUrl::RemoveFilename).path(), i18n("Text File (*.txt)"));
    if (url.isEmpty())
        return;
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup transConfig(config, "Analysis");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        emit editAnalysis(m_id, "kdenlive:clipanalysis." + i.key(), i.value());
    }
}
