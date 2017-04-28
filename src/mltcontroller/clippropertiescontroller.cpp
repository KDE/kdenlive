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

#ifdef KF5_USE_FILEMETADATA
#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractionResult>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/ExtractorCollection>
#endif

#include <KIO/Global>

#include <QMimeDatabase>
#include <QUrl>
#include "kdenlive_debug.h"
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
#include <QMimeData>
#include <QTextEdit>

AnalysisTree::AnalysisTree(QWidget *parent) : QTreeWidget(parent)
{
    setRootIsDecorated(false);
    setColumnCount(2);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setDragEnabled(true);
}

//virtual
QMimeData *AnalysisTree::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QString data;
    for (QTreeWidgetItem *item : list) {
        if (item->flags() & Qt::ItemIsDragEnabled) {
            data.append(item->text(1));
        }
    }
    QMimeData *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/geometry"), data.toUtf8());
    return mime;
}

#ifdef KF5_USE_FILEMETADATA
class ExtractionResult : public KFileMetaData::ExtractionResult
{
public:
    ExtractionResult(const QString &filename, const QString &mimetype, QTreeWidget *tree)
        : KFileMetaData::ExtractionResult(filename, mimetype, KFileMetaData::ExtractionResult::ExtractMetaData),
          m_tree(tree) {}

    void append(const QString & /*text*/) override {}

    void addType(KFileMetaData::Type::Type /*type*/) override {}

    void add(KFileMetaData::Property::Property property, const QVariant &value) override
    {
        bool decode = false;
        switch (property) {
        case KFileMetaData::Property::ImageMake:
        case KFileMetaData::Property::ImageModel:
        case KFileMetaData::Property::ImageDateTime:
        case KFileMetaData::Property::BitRate:
        case KFileMetaData::Property::TrackNumber:
        case KFileMetaData::Property::ReleaseYear:
        case KFileMetaData::Property::Composer:
        case KFileMetaData::Property::Genre:
        case KFileMetaData::Property::Artist:
        case KFileMetaData::Property::Album:
        case KFileMetaData::Property::Title:
        case KFileMetaData::Property::Comment:
        case KFileMetaData::Property::Copyright:
        case KFileMetaData::Property::PhotoFocalLength:
        case KFileMetaData::Property::PhotoExposureTime:
        case KFileMetaData::Property::PhotoFNumber:
        case KFileMetaData::Property::PhotoApertureValue:
        case KFileMetaData::Property::PhotoWhiteBalance:
        case KFileMetaData::Property::PhotoGpsLatitude:
        case KFileMetaData::Property::PhotoGpsLongitude:
            decode = true;
            break;
        default:
            break;
        }
        if (decode) {
            KFileMetaData::PropertyInfo info(property);
            if (info.valueType() == QVariant::DateTime) {
                new QTreeWidgetItem(m_tree, QStringList() << info.displayName() << value.toDateTime().toString(Qt::DefaultLocaleShortDate));
            } else if (info.valueType() == QVariant::Int) {
                int val = value.toInt();
                if (property == KFileMetaData::Property::BitRate) {
                    // Adjust unit for bitrate
                    new QTreeWidgetItem(m_tree, QStringList() << info.displayName() << QString::number(val / 1000) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s"));
                } else {
                    new QTreeWidgetItem(m_tree, QStringList() << info.displayName() << QString::number(val));
                }
            } else if (info.valueType() == QVariant::Double) {
                new QTreeWidgetItem(m_tree, QStringList() << info.displayName() << QString::number(value.toDouble()));
            } else {
                new QTreeWidgetItem(m_tree, QStringList() << info.displayName() << value.toString());
            }
        }
    }
private:
    QTreeWidget *m_tree;
};
#endif

ClipPropertiesController::ClipPropertiesController(const Timecode &tc, ClipController *controller, QWidget *parent) : QWidget(parent)
    , m_controller(controller)
    , m_tc(tc)
    , m_id(controller->clipId())
    , m_type(controller->clipType())
    , m_properties(controller->properties())
    , m_textEdit(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QVBoxLayout *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    m_clipLabel = new QLabel(controller->clipName());
    lay->addWidget(m_clipLabel);
    m_tabWidget = new QTabWidget(this);
    lay->addWidget(m_tabWidget);
    setLayout(lay);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabPosition(QTabWidget::East);
    m_forcePage = new QWidget(this);
    m_propertiesPage = new QWidget(this);
    m_markersPage = new QWidget(this);
    m_metaPage = new QWidget(this);
    m_analysisPage = new QWidget(this);

    // Clip properties
    QVBoxLayout *propsBox = new QVBoxLayout;
    m_propertiesTree = new QTreeWidget(this);
    m_propertiesTree->setRootIsDecorated(false);
    m_propertiesTree->setColumnCount(2);
    m_propertiesTree->setAlternatingRowColors(true);
    m_propertiesTree->sortByColumn(0, Qt::AscendingOrder);
    m_propertiesTree->setHeaderHidden(true);
    propsBox->addWidget(m_propertiesTree);
    fillProperties();
    m_propertiesPage->setLayout(propsBox);

    // Clip markers
    QVBoxLayout *mBox = new QVBoxLayout;
    m_markerTree = new QTreeWidget;
    m_markerTree->setRootIsDecorated(false);
    m_markerTree->setColumnCount(2);
    m_markerTree->setAlternatingRowColors(true);
    m_markerTree->setHeaderHidden(true);
    m_markerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mBox->addWidget(m_markerTree);
    QToolBar *bar = new QToolBar;
    bar->addAction(KoIconUtils::themedIcon(QStringLiteral("document-new")), i18n("Add marker"), this, SLOT(slotAddMarker()));
    bar->addAction(KoIconUtils::themedIcon(QStringLiteral("trash-empty")), i18n("Delete marker"), this, SLOT(slotDeleteMarker()));
    bar->addAction(KoIconUtils::themedIcon(QStringLiteral("document-edit")), i18n("Edit marker"), this, SLOT(slotEditMarker()));
    bar->addAction(KoIconUtils::themedIcon(QStringLiteral("document-save-as")), i18n("Export markers"), this, SLOT(slotSaveMarkers()));
    bar->addAction(KoIconUtils::themedIcon(QStringLiteral("document-open")), i18n("Import markers"), this, SLOT(slotLoadMarkers()));
    mBox->addWidget(bar);

    slotFillMarkers();
    m_markersPage->setLayout(mBox);
    connect(m_markerTree, &QAbstractItemView::doubleClicked, this, &ClipPropertiesController::slotSeekToMarker);

    // metadata
    QVBoxLayout *m2Box = new QVBoxLayout;
    QTreeWidget *metaTree = new QTreeWidget;
    metaTree->setRootIsDecorated(true);
    metaTree->setColumnCount(2);
    metaTree->setAlternatingRowColors(true);
    metaTree->setHeaderHidden(true);
    m2Box->addWidget(metaTree);
    slotFillMeta(metaTree);
    m_metaPage->setLayout(m2Box);

    // Clip analysis
    QVBoxLayout *aBox = new QVBoxLayout;
    m_analysisTree = new AnalysisTree(this);
    aBox ->addWidget(new QLabel(i18n("Analysis data")));
    aBox ->addWidget(m_analysisTree);
    QToolBar *bar2 = new QToolBar;
    bar2->addAction(KoIconUtils::themedIcon(QStringLiteral("trash-empty")), i18n("Delete analysis"), this, SLOT(slotDeleteAnalysis()));
    bar2->addAction(KoIconUtils::themedIcon(QStringLiteral("document-save-as")), i18n("Export analysis"), this, SLOT(slotSaveAnalysis()));
    bar2->addAction(KoIconUtils::themedIcon(QStringLiteral("document-open")), i18n("Import analysis"), this, SLOT(slotLoadAnalysis()));
    aBox->addWidget(bar2);

    slotFillAnalysisData();
    m_analysisPage->setLayout(aBox);

    // Force properties
    QVBoxLayout *vbox = new QVBoxLayout;
    if (m_type == Text || m_type == SlideShow || m_type == TextTemplate) {
        QPushButton *editButton = new QPushButton(i18n("Edit Clip"), this);
        connect(editButton, &QAbstractButton::clicked, this, &ClipPropertiesController::editClip);
        vbox->addWidget(editButton);
    }
    if (m_type == Color || m_type == Image || m_type == AV || m_type == Video || m_type == TextTemplate) {
        // Edit duration widget
        m_originalProperties.insert(QStringLiteral("out"), m_properties.get("out"));
        int kdenlive_length = m_properties.get_int("kdenlive:duration");
        if (kdenlive_length > 0) {
            m_originalProperties.insert(QStringLiteral("kdenlive:duration"), QString::number(kdenlive_length));
        }
        m_originalProperties.insert(QStringLiteral("length"), m_properties.get("length"));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Duration"), this);
        box->setObjectName(QStringLiteral("force_duration"));
        hlay->addWidget(box);
        TimecodeDisplay *timePos = new TimecodeDisplay(tc, this);
        timePos->setObjectName(QStringLiteral("force_duration_value"));
        timePos->setValue(kdenlive_length > 0 ? kdenlive_length : m_properties.get_int("length"));
        int original_length = m_properties.get_int("kdenlive:original_length");
        if (original_length > 0) {
            box->setChecked(true);
        } else {
            timePos->setEnabled(false);
        }
        hlay->addWidget(timePos);
        vbox->addLayout(hlay);
        connect(box, &QAbstractButton::toggled, timePos, &QWidget::setEnabled);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        connect(timePos, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipPropertiesController::slotDurationChanged);
        connect(this, &ClipPropertiesController::updateTimeCodeFormat, timePos, &TimecodeDisplay::slotUpdateTimeCodeFormat);
        connect(this, SIGNAL(modified(int)), timePos, SLOT(setValue(int)));
    }
    if (m_type == TextTemplate) {
        // Edit text widget
        QString currentText = m_properties.get("templatetext");
        m_originalProperties.insert(QStringLiteral("templatetext"), currentText);
        m_textEdit = new QTextEdit(this);
        m_textEdit->setAcceptRichText(false);
        m_textEdit->setPlainText(currentText);
        m_textEdit->setPlaceholderText(i18n("Enter template text here"));
        vbox->addWidget(m_textEdit);
        QPushButton *button = new QPushButton(i18n("Apply"), this);
        vbox->addWidget(button);
        connect(button, &QPushButton::clicked, this, &ClipPropertiesController::slotTextChanged);
    } else if (m_type == Color) {
        // Edit color widget
        m_originalProperties.insert(QStringLiteral("resource"), m_properties.get("resource"));
        mlt_color color = m_properties.get_color("resource");
        ChooseColorWidget *choosecolor = new ChooseColorWidget(i18n("Color"), QColor::fromRgb(color.r, color.g, color.b).name(), "", false, this);
        vbox->addWidget(choosecolor);
        //connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
        connect(choosecolor, &ChooseColorWidget::modified, this, &ClipPropertiesController::slotColorModified);
        connect(this, SIGNAL(modified(QColor)), choosecolor, SLOT(slotColorModified(QColor)));
    } else if (m_type == Image) {
        int transparency = m_properties.get_int("kdenlive:transparency");
        m_originalProperties.insert(QStringLiteral("kdenlive:transparency"), QString::number(transparency));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Transparent"), this);
        box->setObjectName(QStringLiteral("kdenlive:transparency"));
        box->setChecked(transparency == 1);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        hlay->addWidget(box);
        vbox->addLayout(hlay);
    }
    if (m_type == AV || m_type == Video || m_type == Image) {
        // Aspect ratio
        int force_ar_num = m_properties.get_int("force_aspect_num");
        int force_ar_den  = m_properties.get_int("force_aspect_den");
        m_originalProperties.insert(QStringLiteral("force_aspect_den"), (force_ar_den == 0) ? QString() : QString::number(force_ar_den));
        m_originalProperties.insert(QStringLiteral("force_aspect_num"), (force_ar_num == 0) ? QString() : QString::number(force_ar_num));
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Aspect Ratio"), this);
        box->setObjectName(QStringLiteral("force_ar"));
        vbox->addWidget(box);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        QSpinBox *spin1 = new QSpinBox(this);
        spin1->setMaximum(8000);
        spin1->setObjectName(QStringLiteral("force_aspect_num_value"));
        hlay->addWidget(spin1);
        hlay->addWidget(new QLabel(QStringLiteral(":")));
        QSpinBox *spin2 = new QSpinBox(this);
        spin2->setMinimum(1);
        spin2->setMaximum(8000);
        spin2->setObjectName(QStringLiteral("force_aspect_den_value"));
        hlay->addWidget(spin2);
        if (force_ar_num == 0) {
            // use current ratio
            int num = m_properties.get_int("meta.media.sample_aspect_num");
            int den = m_properties.get_int("meta.media.sample_aspect_den");
            if (den == 0) {
                num = 1;
                den = 1;
            }
            spin1->setEnabled(false);
            spin2->setEnabled(false);
            spin1->setValue(num);
            spin2->setValue(den);
        } else {
            box->setChecked(true);
            spin1->setEnabled(true);
            spin2->setEnabled(true);
            spin1->setValue(force_ar_num);
            spin2->setValue(force_ar_den);
        }
        connect(spin2, SIGNAL(valueChanged(int)), this, SLOT(slotAspectValueChanged(int)));
        connect(spin1, SIGNAL(valueChanged(int)), this, SLOT(slotAspectValueChanged(int)));
        connect(box, &QAbstractButton::toggled, spin1, &QWidget::setEnabled);
        connect(box, &QAbstractButton::toggled, spin2, &QWidget::setEnabled);
        vbox->addLayout(hlay);
    }

    if (m_type == AV || m_type == Video) {
        QLocale locale;

        // Fps
        QString force_fps = m_properties.get("force_fps");
        m_originalProperties.insert(QStringLiteral("force_fps"), force_fps.isEmpty() ? QStringLiteral("-") : force_fps);
        QHBoxLayout *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Frame rate"), this);
        box->setObjectName(QStringLiteral("force_fps"));
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        QDoubleSpinBox *spin = new QDoubleSpinBox(this);
        spin->setMaximum(1000);
        connect(spin, SIGNAL(valueChanged(double)), this, SLOT(slotValueChanged(double)));
        spin->setObjectName(QStringLiteral("force_fps_value"));
        if (force_fps.isEmpty()) {
            spin->setValue(controller->originalFps());
        } else {
            spin->setValue(locale.toDouble(force_fps));
        }
        connect(box, &QAbstractButton::toggled, spin, &QWidget::setEnabled);
        box->setChecked(!force_fps.isEmpty());
        spin->setEnabled(!force_fps.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(spin);
        vbox->addLayout(hlay);

        // Scanning
        QString force_prog = m_properties.get("force_progressive");
        m_originalProperties.insert(QStringLiteral("force_progressive"), force_prog.isEmpty() ? QStringLiteral("-") : force_prog);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Scanning"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("force_progressive"));
        QComboBox *combo = new QComboBox(this);
        combo->addItem(i18n("Interlaced"), 0);
        combo->addItem(i18n("Progressive"), 1);
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        combo->setObjectName(QStringLiteral("force_progressive_value"));
        if (!force_prog.isEmpty()) {
            combo->setCurrentIndex(force_prog.toInt());
        }
        connect(box, &QAbstractButton::toggled, combo, &QWidget::setEnabled);
        box->setChecked(!force_prog.isEmpty());
        combo->setEnabled(!force_prog.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);

        // Field order
        QString force_tff = m_properties.get("force_tff");
        m_originalProperties.insert(QStringLiteral("force_tff"), force_tff.isEmpty() ? QStringLiteral("-") : force_tff);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Field order"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("force_tff"));
        combo = new QComboBox(this);
        combo->addItem(i18n("Bottom first"), 0);
        combo->addItem(i18n("Top first"), 1);
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        combo->setObjectName(QStringLiteral("force_tff_value"));
        if (!force_tff.isEmpty()) {
            combo->setCurrentIndex(force_tff.toInt());
        }
        connect(box, &QAbstractButton::toggled, combo, &QWidget::setEnabled);
        box->setChecked(!force_tff.isEmpty());
        combo->setEnabled(!force_tff.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);

        //Autorotate
        QString autorotate = m_properties.get("autorotate");
        m_originalProperties.insert(QStringLiteral("autorotate"), autorotate);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Disable autorotate"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("autorotate"));
        box->setChecked(autorotate == QLatin1String("0"));
        hlay->addWidget(box);
        vbox->addLayout(hlay);

        //Decoding threads
        QString threads = m_properties.get("threads");
        m_originalProperties.insert(QStringLiteral("threads"), threads);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Threads")));
        QSpinBox *spinI = new QSpinBox(this);
        spinI->setMaximum(4);
        spinI->setObjectName(QStringLiteral("threads_value"));
        if (!threads.isEmpty()) {
            spinI->setValue(threads.toInt());
        } else {
            spinI->setValue(1);
        }
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        //Video index
        QString vix = m_properties.get("video_index");
        m_originalProperties.insert(QStringLiteral("video_index"), vix);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Video index")));
        spinI = new QSpinBox(this);
        spinI->setMaximum(m_clipProperties.value(QStringLiteral("video_max")).toInt());
        spinI->setObjectName(QStringLiteral("video_index_value"));
        if (vix.isEmpty()) {
            spinI->setValue(0);
        } else {
            spinI->setValue(vix.toInt());
        }
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        //Audio index
        QString aix = m_properties.get("audio_index");
        m_originalProperties.insert(QStringLiteral("audio_index"), aix);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Audio index")));
        spinI = new QSpinBox(this);
        spinI->setMaximum(m_clipProperties.value(QStringLiteral("audio_max")).toInt());
        spinI->setObjectName(QStringLiteral("audio_index_value"));
        if (aix.isEmpty()) {
            spinI->setValue(0);
        } else {
            spinI->setValue(aix.toInt());
        }
        connect(spinI, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged(int)));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        // Colorspace
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Colorspace"), this);
        box->setObjectName(QStringLiteral("force_colorspace"));
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        combo = new QComboBox(this);
        combo->setObjectName(QStringLiteral("force_colorspace_value"));
        combo->addItem(ProfilesDialog::getColorspaceDescription(601), 601);
        combo->addItem(ProfilesDialog::getColorspaceDescription(709), 709);
        combo->addItem(ProfilesDialog::getColorspaceDescription(240), 240);
        int force_colorspace = m_properties.get_int("force_colorspace");
        m_originalProperties.insert(QStringLiteral("force_colorspace"), force_colorspace == 0 ? QStringLiteral("-") : QString::number(force_colorspace));
        int colorspace = controller->videoCodecProperty(QStringLiteral("colorspace")).toInt();
        if (force_colorspace > 0) {
            box->setChecked(true);
            combo->setEnabled(true);
            combo->setCurrentIndex(combo->findData(force_colorspace));
        } else if (colorspace > 0) {
            combo->setEnabled(false);
            combo->setCurrentIndex(combo->findData(colorspace));
        } else {
            combo->setEnabled(false);
        }
        connect(box, &QAbstractButton::toggled, combo, &QWidget::setEnabled);
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotComboValueChanged()));
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);

        //Full luma
        QString force_luma = m_properties.get("set.force_full_luma");
        m_originalProperties.insert(QStringLiteral("set.force_full_luma"), force_luma);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Full luma range"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("set.force_full_luma"));
        box->setChecked(!force_luma.isEmpty());
        hlay->addWidget(box);
        vbox->addLayout(hlay);
    }
    m_forcePage->setLayout(vbox);
    vbox->addStretch(10);
    m_tabWidget->addTab(m_propertiesPage, QString());
    m_tabWidget->addTab(m_forcePage, QString());
    m_tabWidget->addTab(m_markersPage, QString());
    m_tabWidget->addTab(m_metaPage, QString());
    m_tabWidget->addTab(m_analysisPage, QString());
    m_tabWidget->setTabIcon(0, KoIconUtils::themedIcon(QStringLiteral("edit-find")));
    m_tabWidget->setTabToolTip(0, i18n("Properties"));
    m_tabWidget->setTabIcon(1, KoIconUtils::themedIcon(QStringLiteral("document-edit")));
    m_tabWidget->setTabToolTip(1, i18n("Force properties"));
    m_tabWidget->setTabIcon(2, KoIconUtils::themedIcon(QStringLiteral("bookmark-new")));
    m_tabWidget->setTabToolTip(2, i18n("Markers"));
    m_tabWidget->setTabIcon(3, KoIconUtils::themedIcon(QStringLiteral("view-grid")));
    m_tabWidget->setTabToolTip(3, i18n("Metadata"));
    m_tabWidget->setTabIcon(4, KoIconUtils::themedIcon(QStringLiteral("visibility")));
    m_tabWidget->setTabToolTip(4, i18n("Analysis"));
    m_tabWidget->setCurrentIndex(KdenliveSettings::properties_panel_page());
    if (m_type == Color) {
        m_tabWidget->setTabEnabled(0, false);
    }
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ClipPropertiesController::updateTab);
}

ClipPropertiesController::~ClipPropertiesController()
{
}

void ClipPropertiesController::updateTab(int ix)
{
    KdenliveSettings::setProperties_panel_page(ix);
}

void ClipPropertiesController::slotRefreshTimeCode()
{
    emit updateTimeCodeFormat();
}

void ClipPropertiesController::slotReloadProperties()
{
    mlt_color color;
    m_clipLabel->setText(m_properties.get("kdenlive:clipname"));
    switch (m_type) {
    case Color:
        m_originalProperties.insert(QStringLiteral("resource"), m_properties.get("resource"));
        m_originalProperties.insert(QStringLiteral("out"), m_properties.get("out"));
        m_originalProperties.insert(QStringLiteral("length"), m_properties.get("length"));
        emit modified(m_properties.get_int("length"));
        color = m_properties.get_color("resource");
        emit modified(QColor::fromRgb(color.r, color.g, color.b));
        break;
    case TextTemplate:
        m_textEdit->setPlainText(m_properties.get("templatetext"));
        break;
    default:
        break;
    }
}

void ClipPropertiesController::slotColorModified(QColor newcolor)
{
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("resource"), newcolor.name(QColor::HexArgb));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotDurationChanged(int duration)
{
    QMap<QString, QString> properties;
    int original_length = m_properties.get_int("kdenlive:original_length");
    // kdenlive_length is the default duration for image / title clips
    int kdenlive_length = m_properties.get_int("kdenlive:duration");
    int current_length = m_properties.get_int("length");
    if (original_length == 0) {
        m_properties.set("kdenlive:original_length", kdenlive_length > 0 ? kdenlive_length : current_length);
    }
    if (kdenlive_length > 0) {
        // special case, image/title clips store default duration in kdenlive:duration property
        properties.insert(QStringLiteral("kdenlive:duration"), QString::number(duration));
        if (duration > current_length) {
            properties.insert(QStringLiteral("length"), QString::number(duration));
            properties.insert(QStringLiteral("out"), QString::number(duration - 1));
        }
    } else {
        properties.insert(QStringLiteral("length"), QString::number(duration));
        properties.insert(QStringLiteral("out"), QString::number(duration - 1));
    }
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
    QMap<QString, QString> properties;
    QLocale locale;
    if (state == Qt::Unchecked) {
        // The force property was disable, remove it / reset default if necessary
        if (param == QLatin1String("force_duration")) {
            // special case, reset original duration
            TimecodeDisplay *timePos = findChild<TimecodeDisplay *>(param + QStringLiteral("_value"));
            timePos->setValue(m_properties.get_int("kdenlive:original_length"));
            slotDurationChanged(m_properties.get_int("kdenlive:original_length"));
            m_properties.set("kdenlive:original_length", (char *) nullptr);
            return;
        } else if (param == QLatin1String("kdenlive:transparency")) {
            properties.insert(param, QString());
        } else if (param == QLatin1String("force_ar")) {
            properties.insert(QStringLiteral("force_aspect_den"), QString());
            properties.insert(QStringLiteral("force_aspect_num"), QString());
            properties.insert(QStringLiteral("force_aspect_ratio"), QString());
        } else if (param == QLatin1String("autorotate")) {
            properties.insert(QStringLiteral("autorotate"), QString());
        } else {
            properties.insert(param, QString());
        }
    } else {
        // A force property was set
        if (param == QLatin1String("force_duration")) {
            int original_length = m_properties.get_int("kdenlive:original_length");
            if (original_length == 0) {
                int kdenlive_duration = m_properties.get_int("kdenlive:duration");
                m_properties.set("kdenlive:original_length", kdenlive_duration > 0 ? kdenlive_duration : m_properties.get_int("length"));
            }
        } else if (param == QLatin1String("force_fps")) {
            QDoubleSpinBox *spin = findChild<QDoubleSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, locale.toString(spin->value()));
        } else if (param == QLatin1String("threads")) {
            QSpinBox *spin = findChild<QSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, QString::number(spin->value()));
        } else if (param == QLatin1String("force_colorspace")  || param == QLatin1String("force_progressive") || param == QLatin1String("force_tff")) {
            QComboBox *combo = findChild<QComboBox *>(param + QStringLiteral("_value"));
            if (!combo) {
                return;
            }
            properties.insert(param, QString::number(combo->currentData().toInt()));
        } else if (param == QLatin1String("kdenlive:transparency") || param == QLatin1String("set.force_full_luma")) {
            properties.insert(param, QStringLiteral("1"));
        } else if (param == QLatin1String("autorotate")) {
            properties.insert(QStringLiteral("autorotate"), QStringLiteral("0"));
        } else if (param == QLatin1String("force_ar")) {
            QSpinBox *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
            QSpinBox *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
            if (!spin || !spin2) {
                return;
            }
            properties.insert(QStringLiteral("force_aspect_den"), QString::number(spin2->value()));
            properties.insert(QStringLiteral("force_aspect_num"), QString::number(spin->value()));
            properties.insert(QStringLiteral("force_aspect_ratio"), locale.toString((double) spin->value() / spin2->value()));
        }
    }
    if (properties.isEmpty()) {
        return;
    }
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotValueChanged(double value)
{
    QDoubleSpinBox *box = qobject_cast<QDoubleSpinBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section(QLatin1Char('_'), 0, -2);
    QMap<QString, QString> properties;
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
    QString param = box->objectName().section(QLatin1Char('_'), 0, -2);
    QMap<QString, QString> properties;
    properties.insert(param, QString::number(value));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotAspectValueChanged(int)
{
    QSpinBox *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
    QSpinBox *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
    if (!spin || !spin2) {
        return;
    }
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("force_aspect_den"), QString::number(spin2->value()));
    properties.insert(QStringLiteral("force_aspect_num"), QString::number(spin->value()));
    QLocale locale;
    properties.insert(QStringLiteral("force_aspect_ratio"), locale.toString((double) spin->value() / spin2->value()));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotComboValueChanged()
{
    QComboBox *box = qobject_cast<QComboBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section(QLatin1Char('_'), 0, -2);
    QMap<QString, QString> properties;
    properties.insert(param, QString::number(box->currentData().toInt()));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::fillProperties()
{
    m_clipProperties.clear();
    QList<QStringList> propertyMap;

    m_propertiesTree->setSortingEnabled(false);

#ifdef KF5_USE_FILEMETADATA
    // Read File Metadata through KDE's metadata system
    KFileMetaData::ExtractorCollection metaDataCollection;
    QMimeDatabase mimeDatabase;
    QMimeType mimeType;

    mimeType = mimeDatabase.mimeTypeForFile(m_controller->clipUrl());
    foreach (KFileMetaData::Extractor *plugin, metaDataCollection.fetchExtractors(mimeType.name())) {
        ExtractionResult extractionResult(m_controller->clipUrl(), mimeType.name(), m_propertiesTree);
        plugin->extract(&extractionResult);
    }
#endif

    // Get MLT's metadata
    if (m_type == Image) {
        int width = m_controller->int_property(QStringLiteral("meta.media.width"));
        int height = m_controller->int_property(QStringLiteral("meta.media.height"));
        propertyMap.append(QStringList() << i18n("Image size") << QString::number(width) + QLatin1Char('x') + QString::number(height));
    }
    if (m_type == AV || m_type == Video || m_type == Audio) {
        int vindex = m_controller->int_property(QStringLiteral("video_index"));
        int video_max = 0;
        int default_audio = m_controller->int_property(QStringLiteral("audio_index"));
        int audio_max = 0;

        // Find maximum stream index values
        for (int ix = 0; ix < m_controller->int_property(QStringLiteral("meta.media.nb_streams")); ++ix) {
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
            QString type = m_controller->property(property);
            if (type == QLatin1String("video")) {
                video_max = ix;
            } else if (type == QLatin1String("audio")) {
                audio_max = ix;
            }
        }
        m_clipProperties.insert(QStringLiteral("default_video"), QString::number(vindex));
        m_clipProperties.insert(QStringLiteral("video_max"), QString::number(video_max));
        m_clipProperties.insert(QStringLiteral("default_audio"), QString::number(default_audio));
        m_clipProperties.insert(QStringLiteral("audio_max"), QString::number(audio_max));

        if (vindex > -1) {
            // We have a video stream
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", vindex);
            QString codec = m_controller->property(property);
            if (!codec.isEmpty()) {
                propertyMap.append(QStringList() << i18n("Video codec") << codec);
            }
            int width = m_controller->int_property(QStringLiteral("meta.media.width"));
            int height = m_controller->int_property(QStringLiteral("meta.media.height"));
            propertyMap.append(QStringList() << i18n("Frame size") << QString::number(width) + QLatin1Char('x') + QString::number(height));

            snprintf(property, sizeof(property), "meta.media.%d.stream.frame_rate", vindex);
            QString fpsValue = m_controller->property(property);
            if (!fpsValue.isEmpty()) {
                propertyMap.append(QStringList() << i18n("Frame rate") << fpsValue);
            } else {
                int rate_den = m_controller->int_property(QStringLiteral("meta.media.frame_rate_den"));
                if (rate_den > 0) {
                    double fps = (double) m_controller->int_property(QStringLiteral("meta.media.frame_rate_num")) / rate_den;
                    propertyMap.append(QStringList() << i18n("Frame rate") << QString::number(fps, 'f', 2));
                }
            }

            snprintf(property, sizeof(property), "meta.media.%d.codec.bit_rate", vindex);
            int bitrate = m_controller->int_property(property) / 1000;
            if (bitrate > 0) {
                propertyMap.append(QStringList() << i18n("Video bitrate") << QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s"));
            }

            int scan = m_controller->int_property(QStringLiteral("meta.media.progressive"));
            propertyMap.append(QStringList() << i18n("Scanning") << (scan == 1 ? i18n("Progressive") : i18n("Interlaced")));
            snprintf(property, sizeof(property), "meta.media.%d.codec.sample_aspect_ratio", vindex);
            double par = m_controller->double_property(property);
            if (par == 0) {
                // Read media aspect ratio
                par = m_controller->double_property(QStringLiteral("aspect_ratio"));
            }
            propertyMap.append(QStringList() << i18n("Pixel aspect ratio") << QString::number(par, 'f', 3));
            propertyMap.append(QStringList() << i18n("Pixel format") << m_controller->videoCodecProperty(QStringLiteral("pix_fmt")));
            int colorspace = m_controller->videoCodecProperty(QStringLiteral("colorspace")).toInt();
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
            propertyMap.append(QStringList() << i18n("Audio frequency") << QString::number(srate) + QLatin1Char(' ') + i18nc("Herz", "Hz"));

            snprintf(property, sizeof(property), "meta.media.%d.codec.bit_rate", default_audio);
            int bitrate = m_controller->int_property(property) / 1000;
            if (bitrate > 0) {
                propertyMap.append(QStringList() << i18n("Audio bitrate") << QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s"));
            }
        }
    }

    qint64 filesize = m_controller->int64_property(QStringLiteral("kdenlive:file_size"));
    if (filesize > 0) {
        QLocale locale(QLocale::system()); // use the user's locale for getting proper separators!
        propertyMap.append(QStringList()
                           << i18n("File size")
                           << KIO::convertSize(filesize) + QStringLiteral(" (") + locale.toString(filesize) + QLatin1Char(')'));
    }

    for (int i = 0; i < propertyMap.count(); i++) {
        new QTreeWidgetItem(m_propertiesTree, propertyMap.at(i));
    }
    m_propertiesTree->setSortingEnabled(true);
    m_propertiesTree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotFillMarkers()
{
    m_markerTree->clear();
    QList<CommentedTime> markers = m_controller->commentedSnapMarkers();
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
    QList< CommentedTime > marks = m_controller->commentedSnapMarkers();
    int pos = m_markerTree->currentIndex().row();
    if (pos < 0 || pos > marks.count() - 1) {
        return;
    }
    QPointer<MarkerDialog> d = new MarkerDialog(m_controller, marks.at(pos), m_tc, i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        QList<CommentedTime> markers;
        markers << d->newMarker();
        emit addMarkers(m_id, markers);
    }
    delete d;
}

void ClipPropertiesController::slotDeleteMarker()
{
    QList< CommentedTime > marks = m_controller->commentedSnapMarkers();
    QList< CommentedTime > toDelete;
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
        QList<CommentedTime> markers;
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
    int exifUsed = m_controller->int_property(QStringLiteral("kdenlive:exiftool"));
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
    } else if (KdenliveSettings::use_exiftool()) {
        QString url = m_controller->clipUrl();
        //Check for Canon THM file
        url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".THM");
        if (QFile::exists(url)) {
            // Read the exif metadata embedded in the THM file
            QProcess p;
            QStringList args;
            args << QStringLiteral("-g") << QStringLiteral("-args") << url;
            p.start(QStringLiteral("exiftool"), args);
            p.waitForFinished();
            QString res = p.readAllStandardOutput();
            m_controller->setProperty(QStringLiteral("kdenlive:exiftool"), 1);
            QTreeWidgetItem *exif = nullptr;
            QStringList list = res.split(QLatin1Char('\n'));
            foreach (const QString &tagline, list) {
                if (tagline.startsWith(QLatin1String("-File")) || tagline.startsWith(QLatin1String("-ExifTool"))) {
                    continue;
                }
                QString tag = tagline.section(QLatin1Char(':'), 1).simplified();
                if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) {
                    continue;
                }
                if (!tag.section(QLatin1Char('='), 0, 0).isEmpty() && !tag.section(QLatin1Char('='), 1).simplified().isEmpty()) {
                    if (!exif) {
                        exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
                        exif->setExpanded(true);
                    }
                    m_controller->setProperty("kdenlive:meta.exiftool." + tag.section(QLatin1Char('='), 0, 0), tag.section(QLatin1Char('='), 1).simplified());
                    new QTreeWidgetItem(exif, QStringList() << tag.section(QLatin1Char('='), 0, 0) << tag.section(QLatin1Char('='), 1).simplified());
                }
            }
        } else {
            if (m_type == Image || m_controller->codec(false) == QLatin1String("h264")) {
                QProcess p;
                QStringList args;
                args << QStringLiteral("-g") << QStringLiteral("-args") << m_controller->clipUrl();
                p.start(QStringLiteral("exiftool"), args);
                p.waitForFinished();
                QString res = p.readAllStandardOutput();
                if (m_type != Image) {
                    m_controller->setProperty(QStringLiteral("kdenlive:exiftool"), 1);
                }
                QTreeWidgetItem *exif = nullptr;
                QStringList list = res.split(QLatin1Char('\n'));
                foreach (const QString &tagline, list) {
                    if (m_type != Image && !tagline.startsWith(QLatin1String("-H264"))) {
                        continue;
                    }
                    QString tag = tagline.section(QLatin1Char(':'), 1);
                    if (tag.startsWith(QLatin1String("ImageWidth")) || tag.startsWith(QLatin1String("ImageHeight"))) {
                        continue;
                    }
                    if (!exif) {
                        exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
                        exif->setExpanded(true);
                    }
                    if (m_type != Image) {
                        // Do not store image exif metadata in project file, would be too much noise
                        m_controller->setProperty("kdenlive:meta.exiftool." + tag.section(QLatin1Char('='), 0, 0), tag.section(QLatin1Char('='), 1).simplified());
                    }
                    new QTreeWidgetItem(exif, QStringList() << tag.section(QLatin1Char('='), 0, 0) << tag.section(QLatin1Char('='), 1).simplified());
                }
            }
        }
    }
    int magic = m_controller->int_property(QStringLiteral("kdenlive:magiclantern"));
    if (magic == 1) {
        Mlt::Properties subProperties;
        subProperties.pass_values(m_properties, "kdenlive:meta.magiclantern.");
        QTreeWidgetItem *magicL = nullptr;
        for (int i = 0; i < subProperties.count(); i++) {
            if (!magicL) {
                magicL = new QTreeWidgetItem(tree, QStringList() << i18n("Magic Lantern") << QString());
                QIcon icon(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("meta_magiclantern.png")));
                magicL->setIcon(0, icon);
                magicL->setExpanded(true);
            }
            new QTreeWidgetItem(magicL, QStringList() << subProperties.get_name(i) << subProperties.get(i));
        }
    } else if (m_type != Image && KdenliveSettings::use_magicLantern()) {
        QString url = m_controller->clipUrl();
        url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".LOG");
        if (QFile::exists(url)) {
            QFile file(url);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_controller->setProperty(QStringLiteral("kdenlive:magiclantern"), 1);
                QTreeWidgetItem *magicL = nullptr;
                while (!file.atEnd()) {
                    QString line = file.readLine().simplified();
                    if (line.startsWith('#') || line.isEmpty() || !line.contains(QLatin1Char(':'))) {
                        continue;
                    }
                    if (line.startsWith(QLatin1String("CSV data"))) {
                        break;
                    }
                    m_controller->setProperty("kdenlive:meta.magiclantern." + line.section(QLatin1Char(':'), 0, 0).simplified(), line.section(QLatin1Char(':'), 1).simplified());
                    if (!magicL) {
                        magicL = new QTreeWidgetItem(tree, QStringList() << i18n("Magic Lantern") << QString());
                        QIcon icon(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("meta_magiclantern.png")));
                        magicL->setIcon(0, icon);
                        magicL->setExpanded(true);
                    }
                    new QTreeWidgetItem(magicL, QStringList() << line.section(QLatin1Char(':'), 0, 0).simplified() << line.section(QLatin1Char(':'), 1).simplified());
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
    if (!current) {
        return;
    }
    emit editAnalysis(m_id, "kdenlive:clipanalysis." + current->text(0), QString());
}

void ClipPropertiesController::slotSaveAnalysis()
{
    const QString url = QFileDialog::getSaveFileName(this, i18n("Save Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(), i18n("Text File (*.txt)"));
    if (url.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup analysisConfig(config, "Analysis");
    QTreeWidgetItem *current = m_analysisTree->currentItem();
    analysisConfig.writeEntry(current->text(0), current->text(1));
}

void ClipPropertiesController::slotLoadAnalysis()
{
    const QString url = QFileDialog::getOpenFileName(this, i18n("Open Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(), i18n("Text File (*.txt)"));
    if (url.isEmpty()) {
        return;
    }
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

void ClipPropertiesController::slotTextChanged()
{
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("templatetext"), m_textEdit->toPlainText());
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

