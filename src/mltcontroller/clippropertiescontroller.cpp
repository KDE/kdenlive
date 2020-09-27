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
#include "bin/model/markerlistmodel.hpp"
#include "clipcontroller.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timecodedisplay.h"
#include <audio/audioStreamInfo.h>
#include "widgets/choosecolorwidget.h"

#include <KDualAction>
#include <KLocalizedString>

#ifdef KF5_USE_FILEMETADATA
#include <KFileMetaData/ExtractionResult>
#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#endif

#include <KIO/Global>
#include <KIO/OpenFileManagerWindowJob>
#include "kdenlive_debug.h"
#include <KMessageBox>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
#include <QScrollArea>
#include <QTextEdit>
#include <QToolBar>
#include <QUrl>
#include <QListWidgetItem>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QResizeEvent>

ElidedLinkLabel::ElidedLinkLabel(QWidget *parent)
    : QLabel(parent)
{
}

void ElidedLinkLabel::setLabelText(const QString &text, const QString &link)
{
    m_text = text;
    m_link = link;
    int width = currentWidth();
    updateText(width);
}

void ElidedLinkLabel::updateText(int width)
{
    if (m_link.isEmpty()) {
        setText(fontMetrics().elidedText(m_text, Qt::ElideLeft, width));
    } else {
        setText(QString("<a href=\"%1\">%2</a>").arg(m_link, fontMetrics().elidedText(m_text, Qt::ElideLeft, width)));
    }
}

int ElidedLinkLabel::currentWidth() const
{
    int width = 0;
    if (isVisible()) {
        width = contentsRect().width();
    } else {
        QMargins mrg = contentsMargins();
        width = sizeHint().width() - mrg.left() - mrg.right();
    }
    return width;
}

void ElidedLinkLabel::resizeEvent(QResizeEvent *event)
{
    int diff = event->size().width() - event->oldSize().width();
    updateText(currentWidth() + diff);
    QLabel::resizeEvent(event);
}

AnalysisTree::AnalysisTree(QWidget *parent)
    : QTreeWidget(parent)
{
    setRootIsDecorated(false);
    setColumnCount(2);
    setAlternatingRowColors(true);
    setHeaderHidden(true);
    setDragEnabled(true);
}

// virtual
QMimeData *AnalysisTree::mimeData(const QList<QTreeWidgetItem *> list) const
{
    QString mimeData;
    for (QTreeWidgetItem *item : list) {
        if ((item->flags() & Qt::ItemIsDragEnabled) != 0) {
            mimeData.append(item->text(1));
        }
    }
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("kdenlive/geometry"), mimeData.toUtf8());
    return mime;
}

#ifdef KF5_USE_FILEMETADATA
class ExtractionResult : public KFileMetaData::ExtractionResult
{
public:
    ExtractionResult(const QString &filename, const QString &mimetype, QTreeWidget *tree)
        : KFileMetaData::ExtractionResult(filename, mimetype, KFileMetaData::ExtractionResult::ExtractMetaData)
        , m_tree(tree)
    {
    }

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
                    new QTreeWidgetItem(m_tree, QStringList() << info.displayName()
                                                              << QString::number(val / 1000) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s"));
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

ClipPropertiesController::ClipPropertiesController(ClipController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_tc(Timecode(Timecode::HH_MM_SS_HH, pCore->getCurrentFps()))
    , m_id(controller->binId())
    , m_type(controller->clipType())
    , m_properties(new Mlt::Properties(controller->properties()))
    , m_audioStream(nullptr)
    , m_textEdit(nullptr)
    , m_audioStreamsView(nullptr)
    , m_activeAudioStreams(-1)
{
    m_controller->mirrorOriginalProperties(m_sourceProperties);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    m_clipLabel = new ElidedLinkLabel(this);
    
    if (m_type == ClipType::Color || controller->clipUrl().isEmpty()) {
        m_clipLabel->setLabelText(controller->clipName(), QString());
    } else {
        m_clipLabel->setLabelText(controller->clipUrl(), controller->clipUrl());
    }
    connect(m_clipLabel, &QLabel::linkActivated, [](const QString &link) {
        KIO::highlightInFileManager({QUrl::fromLocalFile(link)});
    });
    lay->addWidget(m_clipLabel);
    m_tabWidget = new QTabWidget(this);
    lay->addWidget(m_tabWidget);
    setLayout(lay);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabPosition(QTabWidget::East);
    auto *forcePage = new QScrollArea(this);
    auto *forceAudioPage = new QScrollArea(this);
    m_propertiesPage = new QWidget(this);
    m_markersPage = new QWidget(this);
    m_metaPage = new QWidget(this);
    m_analysisPage = new QWidget(this);

    // Clip properties
    auto *propsBox = new QVBoxLayout;
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
    auto *mBox = new QVBoxLayout;
    m_markerTree = new QTreeView;
    m_markerTree->setRootIsDecorated(false);
    m_markerTree->setAlternatingRowColors(true);
    m_markerTree->setHeaderHidden(true);
    m_markerTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_markerTree->setModel(controller->getMarkerModel().get());
    m_markerTree->setObjectName("markers_list");
    mBox->addWidget(m_markerTree);
    auto *bar = new QToolBar;
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Add marker"), this, SLOT(slotAddMarker()));
    bar->addAction(QIcon::fromTheme(QStringLiteral("trash-empty")), i18n("Delete marker"), this, SLOT(slotDeleteMarker()));
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Edit marker"), this, SLOT(slotEditMarker()));
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Export markers"), this, SLOT(slotSaveMarkers()));
    bar->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Import markers"), this, SLOT(slotLoadMarkers()));
    mBox->addWidget(bar);

    m_markersPage->setLayout(mBox);
    connect(m_markerTree, &QAbstractItemView::doubleClicked, this, &ClipPropertiesController::slotSeekToMarker);

    // metadata
    auto *m2Box = new QVBoxLayout;
    auto *metaTree = new QTreeWidget;
    metaTree->setRootIsDecorated(true);
    metaTree->setColumnCount(2);
    metaTree->setAlternatingRowColors(true);
    metaTree->setHeaderHidden(true);
    m2Box->addWidget(metaTree);
    slotFillMeta(metaTree);
    m_metaPage->setLayout(m2Box);

    // Clip analysis
    auto *aBox = new QVBoxLayout;
    m_analysisTree = new AnalysisTree(this);
    aBox->addWidget(new QLabel(i18n("Analysis data")));
    aBox->addWidget(m_analysisTree);
    auto *bar2 = new QToolBar;
    bar2->addAction(QIcon::fromTheme(QStringLiteral("trash-empty")), i18n("Delete analysis"), this, SLOT(slotDeleteAnalysis()));
    bar2->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")), i18n("Export analysis"), this, SLOT(slotSaveAnalysis()));
    bar2->addAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Import analysis"), this, SLOT(slotLoadAnalysis()));
    aBox->addWidget(bar2);

    slotFillAnalysisData();
    m_analysisPage->setLayout(aBox);

    // Force properties
    auto *vbox = new QVBoxLayout;
    vbox->setSpacing(0);

    // Force Audio properties
    auto *audioVbox = new QVBoxLayout;
    audioVbox->setSpacing(0);

    if (m_type == ClipType::Text || m_type == ClipType::SlideShow || m_type == ClipType::TextTemplate) {
        QPushButton *editButton = new QPushButton(i18n("Edit Clip"), this);
        connect(editButton, &QAbstractButton::clicked, this, &ClipPropertiesController::editClip);
        vbox->addWidget(editButton);
    }
    if (m_type == ClipType::Color || m_type == ClipType::Image || m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::TextTemplate) {
        // Edit duration widget
        m_originalProperties.insert(QStringLiteral("out"), m_properties->get("out"));
        int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
        if (kdenlive_length > 0) {
            m_originalProperties.insert(QStringLiteral("kdenlive:duration"), m_properties->get("kdenlive:duration"));
        }
        m_originalProperties.insert(QStringLiteral("length"), m_properties->get("length"));
        auto *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Duration"), this);
        box->setObjectName(QStringLiteral("force_duration"));
        hlay->addWidget(box);
        auto *timePos = new TimecodeDisplay(m_tc, this);
        timePos->setObjectName(QStringLiteral("force_duration_value"));
        timePos->setValue(kdenlive_length > 0 ? kdenlive_length : m_properties->get_int("length"));
        int original_length = m_properties->get_int("kdenlive:original_length");
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
        
        // Autorotate
         if (m_type == ClipType::Image) {
            int autorotate = m_properties->get_int("disable_exif");
            m_originalProperties.insert(QStringLiteral("disable_exif"), QString::number(autorotate));
            hlay = new QHBoxLayout;
            box = new QCheckBox(i18n("Disable autorotate"), this);
            connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
            box->setObjectName(QStringLiteral("disable_exif"));
            box->setChecked(autorotate == 1);
            hlay->addWidget(box);
            vbox->addLayout(hlay);
        }
        // connect(this, static_cast<void(ClipPropertiesController::*)(int)>(&ClipPropertiesController::modified), timePos, &TimecodeDisplay::setValue);
    }
    if (m_type == ClipType::TextTemplate) {
        // Edit text widget
        QString currentText = m_properties->get("templatetext");
        m_originalProperties.insert(QStringLiteral("templatetext"), currentText);
        m_textEdit = new QTextEdit(this);
        m_textEdit->setAcceptRichText(false);
        m_textEdit->setPlainText(currentText);
        m_textEdit->setPlaceholderText(i18n("Enter template text here"));
        vbox->addWidget(m_textEdit);
        QPushButton *button = new QPushButton(i18n("Apply"), this);
        vbox->addWidget(button);
        connect(button, &QPushButton::clicked, this, &ClipPropertiesController::slotTextChanged);
    } else if (m_type == ClipType::Color) {
        // Edit color widget
        m_originalProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
        mlt_color color = m_properties->get_color("resource");
        ChooseColorWidget *choosecolor = new ChooseColorWidget(i18n("Color"), QColor::fromRgb(color.r, color.g, color.b).name(), "", false, this);
        vbox->addWidget(choosecolor);
        // connect(choosecolor, SIGNAL(displayMessage(QString,int)), this, SIGNAL(displayMessage(QString,int)));
        connect(choosecolor, &ChooseColorWidget::modified, this, &ClipPropertiesController::slotColorModified);
        connect(this, static_cast<void (ClipPropertiesController::*)(const QColor &)>(&ClipPropertiesController::modified), choosecolor,
                &ChooseColorWidget::slotColorModified);
    }
    if (m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::Image) {
        // Aspect ratio
        int force_ar_num = m_properties->get_int("force_aspect_num");
        int force_ar_den = m_properties->get_int("force_aspect_den");
        m_originalProperties.insert(QStringLiteral("force_aspect_den"), (force_ar_den == 0) ? QString() : QString::number(force_ar_den));
        m_originalProperties.insert(QStringLiteral("force_aspect_num"), (force_ar_num == 0) ? QString() : QString::number(force_ar_num));
        auto *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Aspect ratio"), this);
        box->setObjectName(QStringLiteral("force_ar"));
        vbox->addWidget(box);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        auto *spin1 = new QSpinBox(this);
        spin1->setMaximum(8000);
        spin1->setObjectName(QStringLiteral("force_aspect_num_value"));
        hlay->addWidget(spin1);
        hlay->addWidget(new QLabel(QStringLiteral(":")));
        auto *spin2 = new QSpinBox(this);
        spin2->setMinimum(1);
        spin2->setMaximum(8000);
        spin2->setObjectName(QStringLiteral("force_aspect_den_value"));
        hlay->addWidget(spin2);
        if (force_ar_num == 0) {
            // use current ratio
            int num = m_properties->get_int("meta.media.sample_aspect_num");
            int den = m_properties->get_int("meta.media.sample_aspect_den");
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
        connect(spin2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ClipPropertiesController::slotAspectValueChanged);
        connect(spin1, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &ClipPropertiesController::slotAspectValueChanged);
        connect(box, &QAbstractButton::toggled, spin1, &QWidget::setEnabled);
        connect(box, &QAbstractButton::toggled, spin2, &QWidget::setEnabled);
        vbox->addLayout(hlay);

        // Proxy
        QString proxy = m_properties->get("kdenlive:proxy");
        m_originalProperties.insert(QStringLiteral("kdenlive:proxy"), proxy);
        hlay = new QHBoxLayout;
        auto *bg = new QGroupBox(this);
        bg->setCheckable(false);
        bg->setFlat(true);
        auto *groupLay = new QHBoxLayout;
        groupLay->setContentsMargins(0, 0, 0, 0);
        auto *pbox = new QCheckBox(i18n("Proxy clip"), this);
        pbox->setTristate(true);
        // Proxy codec label
        QLabel *lab = new QLabel(this);
        pbox->setObjectName(QStringLiteral("kdenlive:proxy"));
        bool hasProxy = proxy.length() > 2;
        if (hasProxy) {
            bg->setToolTip(proxy);
            bool proxyReady = (QFileInfo(proxy).fileName() == QFileInfo(m_properties->get("resource")).fileName());
            if (proxyReady) {
                pbox->setCheckState(Qt::Checked);
                lab->setText(m_properties->get(QString("meta.media.%1.codec.name").arg(m_properties->get_int("video_index")).toUtf8().constData()));
            } else {
                pbox->setCheckState(Qt::PartiallyChecked);
            }
        } else {
            pbox->setCheckState(Qt::Unchecked);
        }
        pbox->setEnabled(pCore->projectManager()->current()->getDocumentProperty(QStringLiteral("enableproxy")).toInt() != 0);
        connect(pbox, &QCheckBox::stateChanged, this, [this, pbox](int state) {
            emit requestProxy(state == Qt::PartiallyChecked);
            if (state == Qt::Checked) {
                QSignalBlocker bk(pbox);
                pbox->setCheckState(Qt::Unchecked);
            }
        });
        connect(this, &ClipPropertiesController::enableProxy, pbox, &QCheckBox::setEnabled);
        connect(this, &ClipPropertiesController::proxyModified, this, [this, pbox, bg, lab](const QString &pxy) {
            bool hasProxyClip = pxy.length() > 2;
            QSignalBlocker bk(pbox);
            pbox->setCheckState(hasProxyClip ? Qt::Checked : Qt::Unchecked);
            bg->setEnabled(pbox->isChecked());
            bg->setToolTip(pxy);
            lab->setText(hasProxyClip ? m_properties->get(QString("meta.media.%1.codec.name").arg(m_properties->get_int("video_index")).toUtf8().constData())
                                      : QString());
        });
        hlay->addWidget(pbox);
        bg->setEnabled(pbox->checkState() == Qt::Checked);

        groupLay->addWidget(lab);

        // Delete button
        auto *tb = new QToolButton(this);
        tb->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
        tb->setAutoRaise(true);
        connect(tb, &QToolButton::clicked, this, [this, proxy]() { emit deleteProxy(); });
        tb->setToolTip(i18n("Delete proxy file"));
        groupLay->addWidget(tb);
        // Folder button
        tb = new QToolButton(this);
        auto *pMenu = new QMenu(this);
        tb->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
        tb->setToolTip(i18n("Proxy options"));
        tb->setMenu(pMenu);
        tb->setAutoRaise(true);
        tb->setPopupMode(QToolButton::InstantPopup);

        QAction *ac = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), i18n("Open folder"), this);
        connect(ac, &QAction::triggered, this, [this]() {
            QString pxy = m_properties->get("kdenlive:proxy");
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(pxy).path()));
        });
        pMenu->addAction(ac);
        ac = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Play proxy clip"), this);
        connect(ac, &QAction::triggered, this, [this]() {
            QString pxy = m_properties->get("kdenlive:proxy");
            QDesktopServices::openUrl(QUrl::fromLocalFile(pxy));
        });
        pMenu->addAction(ac);
        ac = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy file location to clipboard"), this);
        connect(ac, &QAction::triggered, this, [this]() {
            QString pxy = m_properties->get("kdenlive:proxy");
            QGuiApplication::clipboard()->setText(pxy);
        });
        pMenu->addAction(ac);
        groupLay->addWidget(tb);
        bg->setLayout(groupLay);
        hlay->addWidget(bg);
        vbox->addLayout(hlay);
    }

    if (m_type == ClipType::AV || m_type == ClipType::Video) {
        // Fps
        QString force_fps = m_properties->get("force_fps");
        m_originalProperties.insert(QStringLiteral("force_fps"), force_fps.isEmpty() ? QStringLiteral("-") : force_fps);
        auto *hlay = new QHBoxLayout;
        QCheckBox *box = new QCheckBox(i18n("Frame rate"), this);
        box->setObjectName(QStringLiteral("force_fps"));
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        auto *spin = new QDoubleSpinBox(this);
        spin->setMaximum(1000);
        connect(spin, SIGNAL(valueChanged(double)), this, SLOT(slotValueChanged(double)));
        // connect(spin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &ClipPropertiesController::slotValueChanged);
        spin->setObjectName(QStringLiteral("force_fps_value"));
        if (force_fps.isEmpty()) {
            spin->setValue(controller->originalFps());
        } else {
            spin->setValue(force_fps.toDouble());
        }
        connect(box, &QAbstractButton::toggled, spin, &QWidget::setEnabled);
        box->setChecked(!force_fps.isEmpty());
        spin->setEnabled(!force_fps.isEmpty());
        hlay->addWidget(box);
        hlay->addWidget(spin);
        vbox->addLayout(hlay);

        // Scanning
        QString force_prog = m_properties->get("force_progressive");
        m_originalProperties.insert(QStringLiteral("force_progressive"), force_prog.isEmpty() ? QStringLiteral("-") : force_prog);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Scanning"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("force_progressive"));
        auto *combo = new QComboBox(this);
        combo->addItem(i18n("Interlaced"), 0);
        combo->addItem(i18n("Progressive"), 1);
        connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ClipPropertiesController::slotComboValueChanged);
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
        QString force_tff = m_properties->get("force_tff");
        m_originalProperties.insert(QStringLiteral("force_tff"), force_tff.isEmpty() ? QStringLiteral("-") : force_tff);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Field order"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("force_tff"));
        combo = new QComboBox(this);
        combo->addItem(i18n("Bottom first"), 0);
        combo->addItem(i18n("Top first"), 1);
        connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ClipPropertiesController::slotComboValueChanged);
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

        // Autorotate
        QString autorotate = m_properties->get("autorotate");
        m_originalProperties.insert(QStringLiteral("autorotate"), autorotate);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Disable autorotate"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("autorotate"));
        box->setChecked(autorotate == QLatin1String("0"));
        hlay->addWidget(box);
        vbox->addLayout(hlay);

        // Decoding threads
        QString threads = m_properties->get("threads");
        m_originalProperties.insert(QStringLiteral("threads"), threads);
        hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Threads")));
        auto *spinI = new QSpinBox(this);
        spinI->setMaximum(4);
        spinI->setMinimum(1);
        spinI->setObjectName(QStringLiteral("threads_value"));
        if (!threads.isEmpty()) {
            spinI->setValue(threads.toInt());
        } else {
            spinI->setValue(1);
        }
        connect(spinI, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                static_cast<void (ClipPropertiesController::*)(int)>(&ClipPropertiesController::slotValueChanged));
        hlay->addWidget(spinI);
        vbox->addLayout(hlay);

        // Video index
        if (!m_videoStreams.isEmpty()) {
            QString vix = m_sourceProperties.get("video_index");
            m_originalProperties.insert(QStringLiteral("video_index"), vix);
            hlay = new QHBoxLayout;

            KDualAction *ac = new KDualAction(i18n("Disable video"), i18n("Enable video"), this);
            ac->setInactiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-video")));
            ac->setActiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-hide-video")));
            auto *tbv = new QToolButton(this);
            tbv->setToolButtonStyle(Qt::ToolButtonIconOnly);
            tbv->setDefaultAction(ac);
            tbv->setAutoRaise(true);
            hlay->addWidget(tbv);
            hlay->addWidget(new QLabel(i18n("Video stream")));
            auto *videoStream = new QComboBox(this);
            int ix = 1;
            for (int stream : qAsConst(m_videoStreams)) {
                videoStream->addItem(i18n("Video stream %1", ix), stream);
                ix++;
            }
            if (!vix.isEmpty() && vix.toInt() > -1) {
                videoStream->setCurrentIndex(videoStream->findData(QVariant(vix)));
            }
            ac->setActive(vix.toInt() == -1);
            videoStream->setEnabled(vix.toInt() > -1);
            videoStream->setVisible(m_videoStreams.size() > 1);
            connect(ac, &KDualAction::activeChanged, this, [this, videoStream](bool activated) {
                QMap<QString, QString> properties;
                int vindx = -1;
                if (activated) {
                    videoStream->setEnabled(false);
                } else {
                    videoStream->setEnabled(true);
                    vindx = videoStream->currentData().toInt();
                }
                properties.insert(QStringLiteral("video_index"), QString::number(vindx));
                properties.insert(QStringLiteral("set.test_image"), vindx > -1 ? QStringLiteral("0") : QStringLiteral("1"));
                emit updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            QObject::connect(videoStream, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, videoStream]() {
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("video_index"), QString::number(videoStream->currentData().toInt()));
                emit updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            hlay->addWidget(videoStream);
            vbox->addLayout(hlay);
        }

        // Audio index
        QMap<int, QString> audioStreamsInfo = m_controller->audioStreams();
        if (!audioStreamsInfo.isEmpty()) {
            QList <int> enabledStreams = m_controller->activeStreams().keys();
            QString vix = m_sourceProperties.get("audio_index");
            m_originalProperties.insert(QStringLiteral("audio_index"), vix);
            QStringList streamString;
            for (int streamIx : qAsConst(enabledStreams)) {
                streamString << QString::number(streamIx);
            }
            m_originalProperties.insert(QStringLiteral("kdenlive:active_streams"), streamString.join(QLatin1Char(';')));
            hlay = new QHBoxLayout;

            KDualAction *ac = new KDualAction(i18n("Disable audio"), i18n("Enable audio"), this);
            ac->setInactiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-show-audio")));
            ac->setActiveIcon(QIcon::fromTheme(QStringLiteral("kdenlive-hide-audio")));
            auto *tbv = new QToolButton(this);
            tbv->setToolButtonStyle(Qt::ToolButtonIconOnly);
            tbv->setDefaultAction(ac);
            tbv->setAutoRaise(true);
            hlay->addWidget(tbv);
            hlay->addWidget(new QLabel(i18n("Audio streams")));
            audioVbox->addLayout(hlay);
            m_audioStreamsView = new QListWidget(this);
            m_audioStreamsView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            audioVbox->addWidget(m_audioStreamsView);
            QMapIterator<int, QString> i(audioStreamsInfo);
            while (i.hasNext()) {
                i.next();
                QListWidgetItem *item = new QListWidgetItem(i.value(), m_audioStreamsView);
                // Store stream index
                item->setData(Qt::UserRole, i.key());
                // Store oringinal name
                item->setData(Qt::UserRole + 1, i.value());
                item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
                if (enabledStreams.contains(i.key())) {
                    item->setCheckState(Qt::Checked);
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
                updateStreamIcon(m_audioStreamsView->row(item), i.key());
            }
            if (audioStreamsInfo.count() > 1) {
                QListWidgetItem *item = new QListWidgetItem(i18n("Merge all streams"), m_audioStreamsView);
                item->setData(Qt::UserRole, INT_MAX);
                item->setData(Qt::UserRole + 1, item->text());
                item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                if (enabledStreams.contains(INT_MAX)) {
                    item->setCheckState(Qt::Checked);
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
            }
            connect(m_audioStreamsView, &QListWidget::currentRowChanged, this, [this] (int row) {
                if (row > -1) {
                    m_audioEffectGroup->setEnabled(true);
                    QListWidgetItem *item = m_audioStreamsView->item(row);
                    m_activeAudioStreams = item->data(Qt::UserRole).toInt();
                    QStringList effects = m_controller->getAudioStreamEffect(m_activeAudioStreams);
                    QSignalBlocker bk(m_swapChannels);
                    QSignalBlocker bk1(m_copyChannelGroup);
                    QSignalBlocker bk2(m_normalize);
                    m_swapChannels->setChecked(effects.contains(QLatin1String("channelswap")));
                    m_copyChannel1->setChecked(effects.contains(QStringLiteral("channelcopy from=0 to=1")));
                    m_copyChannel2->setChecked(effects.contains(QStringLiteral("channelcopy from=1 to=0")));
                    m_normalize->setChecked(effects.contains(QStringLiteral("dynamic_loudness")));
                    int gain = 0;
                    for (const QString &st : qAsConst(effects)) {
                        if (st.startsWith(QLatin1String("volume "))) {
                            QSignalBlocker bk3(m_gain);
                            gain = st.section(QLatin1Char('='), 1).toInt();
                            break;
                        }
                    }
                    QSignalBlocker bk3(m_gain);
                    m_gain->setValue(gain);
                } else {
                    m_activeAudioStreams = -1;
                    m_audioEffectGroup->setEnabled(false);
                }
            });
            connect(m_audioStreamsView, &QListWidget::itemChanged, this, [this] (QListWidgetItem *item) {
                if (!item) {
                    return;
                }
                bool checked = item->checkState() == Qt::Checked;
                int streamId = item->data(Qt::UserRole).toInt();
                bool streamModified = false;
                QString currentStreams = m_originalProperties.value(QStringLiteral("kdenlive:active_streams"));
                QStringList activeStreams = currentStreams.split(QLatin1Char(';'));
                if (activeStreams.contains(QString::number(streamId))) {
                    if (!checked) {
                        // Stream was unselected
                        activeStreams.removeAll(QString::number(streamId));
                        streamModified = true;
                    }
                } else if (checked) {
                    // Stream was selected
                    if (streamId == INT_MAX) {
                        // merge all streams should not have any other stream selected
                        activeStreams.clear();
                    } else {
                        activeStreams.removeAll(QString::number(INT_MAX));
                    }
                    activeStreams << QString::number(streamId);
                    activeStreams.sort();
                    streamModified = true;
                }
                if (streamModified) {
                    if (activeStreams.isEmpty()) {
                        activeStreams << QStringLiteral("-1");
                    }
                    QMap<QString, QString> properties;
                    properties.insert(QStringLiteral("kdenlive:active_streams"), activeStreams.join(QLatin1Char(';')));
                    emit updateClipProperties(m_id, m_originalProperties, properties);
                    m_originalProperties = properties;
                } else if (item->text() != item->data(Qt::UserRole + 1).toString()) {
                    // Rename event
                    QString txt = item->text();
                    int row = m_audioStreamsView->row(item) + 1;
                    if (!txt.startsWith(QString("%1|").arg(row))) {
                        txt.prepend(QString("%1|").arg(row));
                    }
                    m_controller->renameAudioStream(streamId, txt);
                    QSignalBlocker bk(m_audioStreamsView);
                    item->setText(txt);
                    item->setData(Qt::UserRole + 1, txt);
                }
            });
            ac->setActive(vix.toInt() == -1);
            connect(ac, &KDualAction::activeChanged, this, [this, audioStreamsInfo](bool activated) {
                QMap<QString, QString> properties;
                int vindx = -1;
                if (activated) {
                    properties.insert(QStringLiteral("kdenlive:active_streams"), QStringLiteral("-1"));
                } else {
                    properties.insert(QStringLiteral("kdenlive:active_streams"), QString());
                    vindx = audioStreamsInfo.firstKey();
                }
                properties.insert(QStringLiteral("audio_index"), QString::number(vindx));
                properties.insert(QStringLiteral("set.test_audio"), vindx > -1 ? QStringLiteral("0") : QStringLiteral("1"));
                emit updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            // Audio effects
            m_audioEffectGroup = new QGroupBox(this);
            m_audioEffectGroup->setEnabled(false);
            QVBoxLayout *vbox = new QVBoxLayout;
            // Normalize
            m_normalize = new QCheckBox(i18n("Normalize"), this);
            connect(m_normalize, &QCheckBox::stateChanged, this, [this] (int state) {
                if (m_activeAudioStreams == -1) {
                    // No stream selected, abort
                    return;
                }
                if (state == Qt::Checked) {
                    // Add swap channels effect
                    m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("dynamic_loudness"));
                } else {
                    // Remove swap channels effect
                    m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("dynamic_loudness"));
                }
                updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
            });
            vbox->addWidget(m_normalize);

            // Swap channels
            m_swapChannels = new QCheckBox(i18n("Swap Channels"), this);
            connect(m_swapChannels, &QCheckBox::stateChanged, this, [this] (int state) {
                if (m_activeAudioStreams == -1) {
                    // No stream selected, abort
                    return;
                }
                if (state == Qt::Checked) {
                    // Add swap channels effect
                    m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelswap"));
                } else {
                    // Remove swap channels effect
                    m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("channelswap"));
                }
                updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
            });
            vbox->addWidget(m_swapChannels);
            // Copy channel
            QHBoxLayout *copyLay = new QHBoxLayout;
            copyLay->addWidget(new QLabel(i18n("Copy Channel"), this));
            m_copyChannel1 = new QCheckBox(i18n("1"), this);
            m_copyChannel2 = new QCheckBox(i18n("2"), this);
            m_copyChannelGroup = new QButtonGroup(this);
            m_copyChannelGroup->addButton(m_copyChannel1);
            m_copyChannelGroup->addButton(m_copyChannel2);
            m_copyChannelGroup->setExclusive(false);
            copyLay->addWidget(m_copyChannel1);
            copyLay->addWidget(m_copyChannel2);
            copyLay->addStretch(1);
            vbox->addLayout(copyLay);
            connect(m_copyChannelGroup, QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled), this, [this] (QAbstractButton *but, bool) {
                if (but == m_copyChannel1) {
                    QSignalBlocker bk(m_copyChannelGroup);
                    m_copyChannel2->setChecked(false);
                } else {
                    QSignalBlocker bk(m_copyChannelGroup);
                    m_copyChannel1->setChecked(false);
                }
                if (m_copyChannel1->isChecked()) {
                    m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy from=0 to=1"));
                } else if (m_copyChannel2->isChecked()) {
                    m_controller->requestAddStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy from=1 to=0"));
                } else {
                    // Remove swap channels effect
                    m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("channelcopy"));
                }
                updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
            });
            // Gain
            QHBoxLayout *gainLay = new QHBoxLayout;
            gainLay->addWidget(new QLabel(i18n("Gain"), this));
            m_gain = new QSpinBox(this);
            m_gain->setRange(-100, 60);
            m_gain->setSuffix(i18n("dB"));
            connect(m_gain, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] (int value) {
                if (m_activeAudioStreams == -1) {
                    // No stream selected, abort
                    return;
                }
                if (value == 0) {
                    // Remove effect
                    m_controller->requestRemoveStreamEffect(m_activeAudioStreams, QStringLiteral("volume"));
                } else {
                    m_controller->requestAddStreamEffect(m_activeAudioStreams, QString("volume level=%1").arg(value));
                }
                updateStreamIcon(m_audioStreamsView->currentRow(), m_activeAudioStreams);
            });
            gainLay->addWidget(m_gain);
            gainLay->addStretch(1);
            vbox->addLayout(gainLay);

            vbox->addStretch(1);
            m_audioEffectGroup->setLayout(vbox);
            audioVbox->addWidget(m_audioEffectGroup);

            // Audio sync
            hlay = new QHBoxLayout;
            hlay->addWidget(new QLabel(i18n("Audio sync")));
            auto *spinSync = new QSpinBox(this);
            spinSync->setSuffix(i18n("ms"));
            spinSync->setRange(-1000, 1000);
            spinSync->setValue(qRound(1000 * m_sourceProperties.get_double("video_delay")));
            spinSync->setObjectName(QStringLiteral("video_delay"));
            if (spinSync->value() != 0) {
                m_originalProperties.insert(QStringLiteral("video_delay"), QString::number(m_sourceProperties.get_double("video_delay"), 'f'));
            }
            QObject::connect(spinSync, &QSpinBox::editingFinished, this, [this, spinSync]() {
                QMap<QString, QString> properties;
                properties.insert(QStringLiteral("video_delay"), QString::number(spinSync->value() / 1000., 'f'));
                emit updateClipProperties(m_id, m_originalProperties, properties);
                m_originalProperties = properties;
            });
            hlay->addWidget(spinSync);
            audioVbox->addLayout(hlay);
        }

        // Colorspace
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Colorspace"), this);
        box->setObjectName(QStringLiteral("force_colorspace"));
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        combo = new QComboBox(this);
        combo->setObjectName(QStringLiteral("force_colorspace_value"));
        combo->addItem(ProfileRepository::getColorspaceDescription(601), 601);
        combo->addItem(ProfileRepository::getColorspaceDescription(709), 709);
        combo->addItem(ProfileRepository::getColorspaceDescription(240), 240);
        int force_colorspace = m_properties->get_int("force_colorspace");
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
        connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ClipPropertiesController::slotComboValueChanged);
        hlay->addWidget(box);
        hlay->addWidget(combo);
        vbox->addLayout(hlay);

        // Full luma
        QString force_luma = m_properties->get("set.force_full_luma");
        m_originalProperties.insert(QStringLiteral("set.force_full_luma"), force_luma);
        hlay = new QHBoxLayout;
        box = new QCheckBox(i18n("Full luma range"), this);
        connect(box, &QCheckBox::stateChanged, this, &ClipPropertiesController::slotEnableForce);
        box->setObjectName(QStringLiteral("set.force_full_luma"));
        box->setChecked(!force_luma.isEmpty());
        hlay->addWidget(box);
        vbox->addLayout(hlay);
        hlay->addStretch(10);
    }
    // Force properties page
    QWidget *forceProp = new QWidget(this);
    forceProp->setLayout(vbox);
    forcePage->setWidget(forceProp);
    forcePage->setWidgetResizable(true);
    // Force audio properties page
    QWidget *forceAudioProp = new QWidget(this);
    forceAudioProp->setLayout(audioVbox);
    forceAudioPage->setWidget(forceAudioProp);
    forceAudioPage->setWidgetResizable(true);

    vbox->addStretch(10);
    m_tabWidget->addTab(m_propertiesPage, QString());
    m_tabWidget->addTab(forcePage, QString());
    m_tabWidget->addTab(forceAudioPage, QString());
    m_tabWidget->addTab(m_markersPage, QString());
    m_tabWidget->addTab(m_metaPage, QString());
    m_tabWidget->addTab(m_analysisPage, QString());
    m_tabWidget->setTabIcon(0, QIcon::fromTheme(QStringLiteral("edit-find")));
    m_tabWidget->setTabToolTip(0, i18n("File info"));
    m_tabWidget->setTabIcon(1, QIcon::fromTheme(QStringLiteral("document-edit")));
    m_tabWidget->setTabToolTip(1, i18n("Properties"));
    m_tabWidget->setTabIcon(2, QIcon::fromTheme(QStringLiteral("audio-volume-high")));
    m_tabWidget->setTabToolTip(2, i18n("Audio Properties"));
    m_tabWidget->setTabIcon(3, QIcon::fromTheme(QStringLiteral("bookmark-new")));
    m_tabWidget->setTabToolTip(3, i18n("Markers"));
    m_tabWidget->setTabIcon(4, QIcon::fromTheme(QStringLiteral("view-grid")));
    m_tabWidget->setTabToolTip(4, i18n("Metadata"));
    m_tabWidget->setTabIcon(5, QIcon::fromTheme(QStringLiteral("visibility")));
    m_tabWidget->setTabToolTip(5, i18n("Analysis"));
    m_tabWidget->setCurrentIndex(KdenliveSettings::properties_panel_page());
    if (m_type == ClipType::Color) {
        m_tabWidget->setTabEnabled(0, false);
    }
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ClipPropertiesController::updateTab);
}

ClipPropertiesController::~ClipPropertiesController() = default;

void ClipPropertiesController::updateStreamIcon(int row, int streamIndex)
{
    QStringList effects = m_controller->getAudioStreamEffect(streamIndex);
    QListWidgetItem *item = m_audioStreamsView->item(row);
    if (item) {
        item->setIcon(effects.isEmpty() ? QIcon() : QIcon::fromTheme(QStringLiteral("favorite")));
    }
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
    m_properties.reset(new Mlt::Properties(m_controller->properties()));
    m_clipLabel->setText(m_properties->get("kdenlive:clipname"));
    switch (m_type) {
    case ClipType::Color:
        m_originalProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
        m_originalProperties.insert(QStringLiteral("out"), m_properties->get("out"));
        m_originalProperties.insert(QStringLiteral("length"), m_properties->get("length"));
        emit modified(m_properties->get_int("length"));
        color = m_properties->get_color("resource");
        emit modified(QColor::fromRgb(color.r, color.g, color.b));
        break;
    case ClipType::TextTemplate:
        m_textEdit->setPlainText(m_properties->get("templatetext"));
        break;
    case ClipType::Image:
    case ClipType::AV:
    case ClipType::Video: {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy != m_originalProperties.value(QStringLiteral("kdenlive:proxy"))) {
            m_originalProperties.insert(QStringLiteral("kdenlive:proxy"), proxy);
            emit proxyModified(proxy);
        }
        if (m_audioStreamsView && m_audioStreamsView->count() > 0) {
            int audio_ix = m_properties->get_int("audio_index");
            m_originalProperties.insert(QStringLiteral("kdenlive:active_streams"), m_properties->get("kdenlive:active_streams"));
            if (audio_ix != m_originalProperties.value(QStringLiteral("audio_index")).toInt()) {
                QSignalBlocker bk(m_audioStream);
                m_originalProperties.insert(QStringLiteral("audio_index"), QString::number(audio_ix));
            }
            QList <int> enabledStreams = m_controller->activeStreams().keys();
            qDebug()<<"=== GOT ACTIVE STREAMS: "<<enabledStreams;
            QSignalBlocker bk(m_audioStreamsView);
            for (int ix = 0; ix < m_audioStreamsView->count(); ix++) {
                QListWidgetItem *item = m_audioStreamsView->item(ix);
                int stream = item->data(Qt::UserRole).toInt();
                item->setCheckState(enabledStreams.contains(stream) ? Qt::Checked : Qt::Unchecked);
            }
        }
        break;
    }
    default:
        break;
    }
}

void ClipPropertiesController::slotColorModified(const QColor &newcolor)
{
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("resource"), newcolor.name(QColor::HexArgb));
    QMap<QString, QString> oldProperties;
    oldProperties.insert(QStringLiteral("resource"), m_properties->get("resource"));
    emit updateClipProperties(m_id, oldProperties, properties);
}

void ClipPropertiesController::slotDurationChanged(int duration)
{
    QMap<QString, QString> properties;
    // kdenlive_length is the default duration for image / title clips
    int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
    int current_length = m_properties->get_int("length");
    if (kdenlive_length > 0) {
        // special case, image/title clips store default duration in kdenlive:duration property
        properties.insert(QStringLiteral("kdenlive:duration"), m_properties->frames_to_time(duration));
        if (duration > current_length) {
            properties.insert(QStringLiteral("length"), m_properties->frames_to_time(duration));
            properties.insert(QStringLiteral("out"), m_properties->frames_to_time(duration - 1));
        }
    } else {
        properties.insert(QStringLiteral("length"), m_properties->frames_to_time(duration));
        properties.insert(QStringLiteral("out"), m_properties->frames_to_time(duration - 1));
    }
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotEnableForce(int state)
{
    auto *box = qobject_cast<QCheckBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName();
    QMap<QString, QString> properties;
    if (state == Qt::Unchecked) {
        // The force property was disable, remove it / reset default if necessary
        if (param == QLatin1String("force_duration")) {
            // special case, reset original duration
            auto *timePos = findChild<TimecodeDisplay *>(param + QStringLiteral("_value"));
            timePos->setValue(m_properties->get_int("kdenlive:original_length"));
            int original = m_properties->get_int("kdenlive:original_length");
            m_properties->set("kdenlive:original_length", (char *)nullptr);
            slotDurationChanged(original);
            return;
        }
        if (param == QLatin1String("kdenlive:transparency")) {
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
            int original_length = m_properties->get_int("kdenlive:original_length");
            if (original_length == 0) {
                int kdenlive_length = m_properties->time_to_frames(m_properties->get("kdenlive:duration"));
                m_properties->set("kdenlive:original_length", kdenlive_length > 0 ? m_properties->get("kdenlive:duration") : m_properties->get("length"));
            }
        } else if (param == QLatin1String("force_fps")) {
            auto *spin = findChild<QDoubleSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, QString::number(spin->value(), 'f'));
        } else if (param == QLatin1String("threads")) {
            auto *spin = findChild<QSpinBox *>(param + QStringLiteral("_value"));
            if (!spin) {
                return;
            }
            properties.insert(param, QString::number(spin->value()));
        } else if (param == QLatin1String("force_colorspace") || param == QLatin1String("force_progressive") || param == QLatin1String("force_tff")) {
            auto *combo = findChild<QComboBox *>(param + QStringLiteral("_value"));
            if (!combo) {
                return;
            }
            properties.insert(param, QString::number(combo->currentData().toInt()));
        } else if (param == QLatin1String("set.force_full_luma")) {
            properties.insert(param, QStringLiteral("1"));
        } else if (param == QLatin1String("autorotate")) {
            properties.insert(QStringLiteral("autorotate"), QStringLiteral("0"));
        } else if (param == QLatin1String("force_ar")) {
            auto *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
            auto *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
            if ((spin == nullptr) || (spin2 == nullptr)) {
                return;
            }
            properties.insert(QStringLiteral("force_aspect_den"), QString::number(spin2->value()));
            properties.insert(QStringLiteral("force_aspect_num"), QString::number(spin->value()));
            properties.insert(QStringLiteral("force_aspect_ratio"), QString::number((double)spin->value() / spin2->value(), 'f'));
        } else if (param == QLatin1String("disable_exif")) {
            properties.insert(QStringLiteral("disable_exif"), QString::number(1));
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
    auto *box = qobject_cast<QDoubleSpinBox *>(sender());
    if (!box) {
        return;
    }
    QString param = box->objectName().section(QLatin1Char('_'), 0, -2);
    QMap<QString, QString> properties;
    properties.insert(param, QString::number(value, 'f'));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotValueChanged(int value)
{
    auto *box = qobject_cast<QSpinBox *>(sender());
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
    auto *spin = findChild<QSpinBox *>(QStringLiteral("force_aspect_num_value"));
    auto *spin2 = findChild<QSpinBox *>(QStringLiteral("force_aspect_den_value"));
    if ((spin == nullptr) || (spin2 == nullptr)) {
        return;
    }
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("force_aspect_den"), QString::number(spin2->value()));
    properties.insert(QStringLiteral("force_aspect_num"), QString::number(spin->value()));
    properties.insert(QStringLiteral("force_aspect_ratio"), QString::number((double)spin->value() / spin2->value(), 'f'));
    emit updateClipProperties(m_id, m_originalProperties, properties);
    m_originalProperties = properties;
}

void ClipPropertiesController::slotComboValueChanged()
{
    auto *box = qobject_cast<QComboBox *>(sender());
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
    for (KFileMetaData::Extractor *plugin : metaDataCollection.fetchExtractors(mimeType.name())) {
        ExtractionResult extractionResult(m_controller->clipUrl(), mimeType.name(), m_propertiesTree);
        plugin->extract(&extractionResult);
    }
#endif

    // Get MLT's metadata
    if (m_type == ClipType::Image) {
        int width = m_sourceProperties.get_int("meta.media.width");
        int height = m_sourceProperties.get_int("meta.media.height");
        propertyMap.append(QStringList() << i18n("Image size") << QString::number(width) + QLatin1Char('x') + QString::number(height));
    }
    if (m_type == ClipType::AV || m_type == ClipType::Video || m_type == ClipType::Audio) {
        int vindex = m_sourceProperties.get_int("video_index");
        int default_audio = m_sourceProperties.get_int("audio_index");

        // Find maximum stream index values
        m_videoStreams.clear();
        int aStreams = m_sourceProperties.get_int("meta.media.nb_streams");
        for (int ix = 0; ix < aStreams; ++ix) {
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
            QString type = m_sourceProperties.get(property);
            if (type == QLatin1String("video")) {
                m_videoStreams << ix;
            }
        }
        m_clipProperties.insert(QStringLiteral("default_video"), QString::number(vindex));
        m_clipProperties.insert(QStringLiteral("default_audio"), QString::number(default_audio));

        if (vindex > -1) {
            // We have a video stream
            QString codecInfo = QString("meta.media.%1.codec.").arg(vindex);
            QString streamInfo = QString("meta.media.%1.stream.").arg(vindex);
            QString property = codecInfo + QStringLiteral("long_name");
            QString codec = m_sourceProperties.get(property.toUtf8().constData());
            if (!codec.isEmpty()) {
                propertyMap.append({i18n("Video codec"), codec});
            }
            int width = m_sourceProperties.get_int("meta.media.width");
            int height = m_sourceProperties.get_int("meta.media.height");
            propertyMap.append({i18n("Frame size"), QString::number(width) + QLatin1Char('x') + QString::number(height)});

            property = streamInfo + QStringLiteral("frame_rate");
            QString fpsValue = m_sourceProperties.get(property.toUtf8().constData());
            if (!fpsValue.isEmpty()) {
                propertyMap.append({i18n("Frame rate"), fpsValue});
            } else {
                int rate_den = m_sourceProperties.get_int("meta.media.frame_rate_den");
                if (rate_den > 0) {
                    double fps = ((double)m_sourceProperties.get_int("meta.media.frame_rate_num")) / rate_den;
                    propertyMap.append({i18n("Frame rate"), QString::number(fps, 'f', 2)});
                }
            }
            property = codecInfo + QStringLiteral("bit_rate");
            int bitrate = m_sourceProperties.get_int(property.toUtf8().constData()) / 1000;
            if (bitrate > 0) {
                propertyMap.append({i18n("Video bitrate"), QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s")});
            }

            int scan = m_sourceProperties.get_int("meta.media.progressive");
            propertyMap.append({i18n("Scanning"), (scan == 1 ? i18n("Progressive") : i18n("Interlaced"))});

            property = codecInfo + QStringLiteral("sample_aspect_ratio");
            double par = m_sourceProperties.get_double(property.toUtf8().constData());
            if (qFuzzyIsNull(par)) {
                // Read media aspect ratio
                par = m_sourceProperties.get_double("aspect_ratio");
            }
            propertyMap.append({i18n("Pixel aspect ratio"), QString::number(par, 'f', 3)});
            property = codecInfo + QStringLiteral("pix_fmt");
            propertyMap.append({i18n("Pixel format"), m_sourceProperties.get(property.toUtf8().constData())});
            property = codecInfo + QStringLiteral("colorspace");
            int colorspace = m_sourceProperties.get_int(property.toUtf8().constData());
            propertyMap.append({i18n("Colorspace"), ProfileRepository::getColorspaceDescription(colorspace)});
        }
        if (default_audio > -1) {
            propertyMap.append({i18n("Audio streams"), QString::number(m_controller->audioStreamsCount())});

            QString codecInfo = QString("meta.media.%1.codec.").arg(default_audio);
            QString property = codecInfo + QStringLiteral("long_name");
            QString codec = m_sourceProperties.get(property.toUtf8().constData());
            if (!codec.isEmpty()) {
                propertyMap.append({i18n("Audio codec"), codec});
            }
            property = codecInfo + QStringLiteral("channels");
            int channels = m_sourceProperties.get_int(property.toUtf8().constData());
            propertyMap.append({i18n("Audio channels"), QString::number(channels)});

            property = codecInfo + QStringLiteral("sample_rate");
            int srate = m_sourceProperties.get_int(property.toUtf8().constData());
            propertyMap.append({i18n("Audio frequency"), QString::number(srate) + QLatin1Char(' ') + i18nc("Herz", "Hz")});

            property = codecInfo + QStringLiteral("bit_rate");
            int bitrate = m_sourceProperties.get_int(property.toUtf8().constData()) / 1000;
            if (bitrate > 0) {
                propertyMap.append({i18n("Audio bitrate"), QString::number(bitrate) + QLatin1Char(' ') + i18nc("Kilobytes per seconds", "kb/s")});
            }
        }
    }

    qint64 filesize = m_sourceProperties.get_int64("kdenlive:file_size");
    if (filesize > 0) {
        QLocale locale(QLocale::system()); // use the user's locale for getting proper separators!
        propertyMap.append({i18n("File size"), KIO::convertSize((size_t)filesize) + QStringLiteral(" (") + locale.toString(filesize) + QLatin1Char(')')});
    }

    for (int i = 0; i < propertyMap.count(); i++) {
        auto *item = new QTreeWidgetItem(m_propertiesTree, propertyMap.at(i));
        item->setToolTip(1, propertyMap.at(i).at(1));
    }
    m_propertiesTree->setSortingEnabled(true);
    m_propertiesTree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotSeekToMarker()
{
    auto markerModel = m_controller->getMarkerModel();
    auto current = m_markerTree->currentIndex();
    if (!current.isValid()) return;
    GenTime pos(markerModel->data(current, MarkerListModel::PosRole).toDouble());
    emit seekToFrame(pos.frames(pCore->getCurrentFps()));
}

void ClipPropertiesController::slotEditMarker()
{
    auto markerModel = m_controller->getMarkerModel();
    auto current = m_markerTree->currentIndex();
    if (!current.isValid()) return;
    GenTime pos(markerModel->data(current, MarkerListModel::PosRole).toDouble());
    markerModel->editMarkerGui(pos, this, false, m_controller);
}

void ClipPropertiesController::slotDeleteMarker()
{
    auto markerModel = m_controller->getMarkerModel();
    QModelIndexList indexes = m_markerTree->selectionModel()->selectedIndexes();
    QList <GenTime> positions;
    for (auto &ix : indexes) {
        if (ix.isValid()) {
            positions << GenTime(markerModel->data(ix, MarkerListModel::PosRole).toDouble());
        }
    }
    if (!positions.isEmpty()) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };

        for (GenTime pos : qAsConst(positions)) {
            markerModel->removeMarker(pos, undo, redo);
        }
        pCore->pushUndo(undo, redo, i18n("Delete marker"));
    }
}

void ClipPropertiesController::slotAddMarker()
{
    auto markerModel = m_controller->getMarkerModel();
    GenTime pos(m_controller->originalProducer()->position(), m_tc.fps());
    markerModel->editMarkerGui(pos, this, true, m_controller, true);
}

void ClipPropertiesController::slotSaveMarkers()
{
    QScopedPointer<QFileDialog> fd(new QFileDialog(this, i18n("Save Clip Markers"), pCore->projectManager()->current()->projectDataFolder()));
    fd->setMimeTypeFilters(QStringList() << QStringLiteral("application/json") << QStringLiteral("text/plain"));
    fd->setFileMode(QFileDialog::AnyFile);
    fd->setAcceptMode(QFileDialog::AcceptSave);
    if (fd->exec() != QDialog::Accepted) {
        return;
    }
    QStringList selection = fd->selectedFiles();
    QString url;
    if (!selection.isEmpty()) {
        url = selection.first();
    }
    if (url.isEmpty()) {
        return;
    }
    QFile file(url);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::error(this, i18n("Cannot open file %1", QUrl::fromLocalFile(url).fileName()));
        return;
    }
    file.write(m_controller->getMarkerModel()->toJson().toUtf8());
    file.close();
}

void ClipPropertiesController::slotLoadMarkers()
{
    QScopedPointer<QFileDialog> fd(new QFileDialog(this, i18n("Load Clip Markers"), pCore->projectManager()->current()->projectDataFolder()));
    fd->setMimeTypeFilters(QStringList() << QStringLiteral("application/json") << QStringLiteral("text/plain"));
    fd->setFileMode(QFileDialog::ExistingFile);
    if (fd->exec() != QDialog::Accepted) {
        return;
    }
    QStringList selection = fd->selectedFiles();
    QString url;
    if (!selection.isEmpty()) {
        url = selection.first();
    }
    if (url.isEmpty()) {
        return;
    }
    QFile file(url);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        KMessageBox::error(this, i18n("Cannot open file %1", QUrl::fromLocalFile(url).fileName()));
        return;
    }
    QString fileContent = QString::fromUtf8(file.readAll());
    file.close();
    bool res = m_controller->getMarkerModel()->importFromJson(fileContent, false);
    if (!res) {
        KMessageBox::error(this, i18n("An error occurred while parsing the marker file"));
    }
}

void ClipPropertiesController::slotFillMeta(QTreeWidget *tree)
{
    tree->clear();
    if (m_type != ClipType::AV && m_type != ClipType::Video && m_type != ClipType::Image) {
        // Currently, we only use exiftool on video files
        return;
    }
    int exifUsed = m_controller->getProducerIntProperty(QStringLiteral("kdenlive:exiftool"));
    if (exifUsed == 1) {
        Mlt::Properties subProperties;
        subProperties.pass_values(*m_properties, "kdenlive:meta.exiftool.");
        if (subProperties.count() > 0) {
            QTreeWidgetItem *exif = new QTreeWidgetItem(tree, QStringList() << i18n("Exif") << QString());
            exif->setExpanded(true);
            for (int i = 0; i < subProperties.count(); i++) {
                new QTreeWidgetItem(exif, QStringList() << subProperties.get_name(i) << subProperties.get(i));
            }
        }
    } else if (KdenliveSettings::use_exiftool()) {
        QString url = m_controller->clipUrl();
        // Check for Canon THM file
        url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".THM");
        if (QFile::exists(url)) {
            // Read the exif metadata embedded in the THM file
            QProcess p;
            QStringList args;
            args << QStringLiteral("-g") << QStringLiteral("-args") << url;
            p.start(QStringLiteral("exiftool"), args);
            p.waitForFinished();
            QString res = p.readAllStandardOutput();
            m_controller->setProducerProperty(QStringLiteral("kdenlive:exiftool"), 1);
            QTreeWidgetItem *exif = nullptr;
            QStringList list = res.split(QLatin1Char('\n'));
            for (const QString &tagline : qAsConst(list)) {
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
                    m_controller->setProducerProperty("kdenlive:meta.exiftool." + tag.section(QLatin1Char('='), 0, 0),
                                                      tag.section(QLatin1Char('='), 1).simplified());
                    new QTreeWidgetItem(exif, QStringList() << tag.section(QLatin1Char('='), 0, 0) << tag.section(QLatin1Char('='), 1).simplified());
                }
            }
        } else {
            if (m_type == ClipType::Image || m_controller->codec(false) == QLatin1String("h264")) {
                QProcess p;
                QStringList args;
                args << QStringLiteral("-g") << QStringLiteral("-args") << m_controller->clipUrl();
                p.start(QStringLiteral("exiftool"), args);
                p.waitForFinished();
                QString res = p.readAllStandardOutput();
                if (m_type != ClipType::Image) {
                    m_controller->setProducerProperty(QStringLiteral("kdenlive:exiftool"), 1);
                }
                QTreeWidgetItem *exif = nullptr;
                QStringList list = res.split(QLatin1Char('\n'));
                for (const QString &tagline : qAsConst(list)) {
                    if (m_type != ClipType::Image && !tagline.startsWith(QLatin1String("-H264"))) {
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
                    if (m_type != ClipType::Image) {
                        // Do not store image exif metadata in project file, would be too much noise
                        m_controller->setProducerProperty("kdenlive:meta.exiftool." + tag.section(QLatin1Char('='), 0, 0),
                                                          tag.section(QLatin1Char('='), 1).simplified());
                    }
                    new QTreeWidgetItem(exif, QStringList() << tag.section(QLatin1Char('='), 0, 0) << tag.section(QLatin1Char('='), 1).simplified());
                }
            }
        }
    }
    int magic = m_controller->getProducerIntProperty(QStringLiteral("kdenlive:magiclantern"));
    if (magic == 1) {
        Mlt::Properties subProperties;
        subProperties.pass_values(*m_properties, "kdenlive:meta.magiclantern.");
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
    } else if (m_type != ClipType::Image && KdenliveSettings::use_magicLantern()) {
        QString url = m_controller->clipUrl();
        url = url.section(QLatin1Char('.'), 0, -2) + QStringLiteral(".LOG");
        if (QFile::exists(url)) {
            QFile file(url);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_controller->setProducerProperty(QStringLiteral("kdenlive:magiclantern"), 1);
                QTreeWidgetItem *magicL = nullptr;
                while (!file.atEnd()) {
                    QString line = file.readLine().simplified();
                    if (line.startsWith('#') || line.isEmpty() || !line.contains(QLatin1Char(':'))) {
                        continue;
                    }
                    if (line.startsWith(QLatin1String("CSV data"))) {
                        break;
                    }
                    m_controller->setProducerProperty("kdenlive:meta.magiclantern." + line.section(QLatin1Char(':'), 0, 0).simplified(),
                                                      line.section(QLatin1Char(':'), 1).simplified());
                    if (!magicL) {
                        magicL = new QTreeWidgetItem(tree, QStringList() << i18n("Magic Lantern") << QString());
                        QIcon icon(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("meta_magiclantern.png")));
                        magicL->setIcon(0, icon);
                        magicL->setExpanded(true);
                    }
                    new QTreeWidgetItem(magicL, QStringList()
                                                    << line.section(QLatin1Char(':'), 0, 0).simplified() << line.section(QLatin1Char(':'), 1).simplified());
                }
            }
        }

        // if (!meta.isEmpty())
        // clip->setMetadata(meta, "Magic Lantern");
        // clip->setProperty("magiclantern", "1");
    }
    tree->resizeColumnToContents(0);
}

void ClipPropertiesController::slotFillAnalysisData()
{
    m_analysisTree->clear();
    Mlt::Properties subProperties;
    subProperties.pass_values(*m_properties, "kdenlive:clipanalysis.");
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
    const QString url =
        QFileDialog::getSaveFileName(this, i18n("Save Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(), i18n("Text File (*.txt)"));
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
    const QString url =
        QFileDialog::getOpenFileName(this, i18n("Open Analysis Data"), QFileInfo(m_controller->clipUrl()).absolutePath(), i18n("Text File (*.txt)"));
    if (url.isEmpty()) {
        return;
    }
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup transConfig(config, "Analysis");
    // read the entries
    QMap<QString, QString> profiles = transConfig.entryMap();
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

void ClipPropertiesController::activatePage(int ix)
{
    m_tabWidget->setCurrentIndex(ix);
}

void ClipPropertiesController::slotDeleteSelectedMarkers()
{
    if (m_tabWidget->currentIndex() == 3) {
        slotDeleteMarker();
    }
}

void ClipPropertiesController::slotSelectAllMarkers()
{
    if (m_tabWidget->currentIndex() == 3) {
        m_markerTree->selectAll();
    }
}

void ClipPropertiesController::updateStreamInfo(int streamIndex)
{
    QStringList effects = m_controller->getAudioStreamEffect(m_activeAudioStreams);
    QListWidgetItem *item = nullptr;
    for (int ix = 0; ix < m_audioStreamsView->count(); ix++) {
        QListWidgetItem *it = m_audioStreamsView->item(ix);
        int stream = it->data(Qt::UserRole).toInt();
        if (stream == m_activeAudioStreams) {
            item = it;
            break;
        }
    }
    if (item) {
        item->setIcon(effects.isEmpty() ? QIcon() : QIcon::fromTheme(QStringLiteral("favorite")));
    }
    if (streamIndex == m_activeAudioStreams) {
        QSignalBlocker bk(m_swapChannels);
        QSignalBlocker bk1(m_copyChannelGroup);
        QSignalBlocker bk2(m_normalize);
        m_swapChannels->setChecked(effects.contains(QLatin1String("channelswap")));
        m_copyChannel1->setChecked(effects.contains(QStringLiteral("channelcopy from=0 to=1")));
        m_copyChannel2->setChecked(effects.contains(QStringLiteral("channelcopy from=1 to=0")));
        m_normalize->setChecked(effects.contains(QStringLiteral("dynamic_loudness")));
        int gain = 0;
        for (const QString &st : qAsConst(effects)) {
            if (st.startsWith(QLatin1String("volume "))) {
                QSignalBlocker bk3(m_gain);
                gain = st.section(QLatin1Char('='), 1).toInt();
                break;
            }
        }
        QSignalBlocker bk3(m_gain);
        m_gain->setValue(gain);
    }
}
