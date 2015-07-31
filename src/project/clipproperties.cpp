/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "clipproperties.h"

#include "kdenlivesettings.h"
#include "doc/kthumb.h"
#include "timeline/markerdialog.h"
#include "dialogs/profilesdialog.h"

#include <KFileItem>
#include <KRun>
#include <klocalizedstring.h>

#include <QDebug>
#include <QFontDatabase>

#include <QDir>
#include <QPainter>
#include <QFileDialog>
#include <QStandardPaths>

static const int VIDEOTAB = 0;
static const int AUDIOTAB = 1;
static const int COLORTAB = 2;
static const int SLIDETAB = 3;
static const int IMAGETAB = 4;
static const int MARKERTAB = 5;
static const int METATAB = 6;


ClipProperties::ClipProperties(DocClipBase *clip, const Timecode &tc, double fps, QWidget * parent) :
    QDialog(parent)
  , m_clip(clip)
  , m_tc(tc)
  , m_fps(fps)
  , m_count(0)
  , m_clipNeedsRefresh(false)
  , m_clipNeedsReLoad(false)
  , m_proxyContainer(NULL)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    
    // force transparency is only for group properties, so hide it
    m_view.clip_force_transparency->setHidden(true);
    m_view.clip_transparency->setHidden(true);
    
    QUrl url = m_clip->fileURL();
    m_view.clip_path->setText(url.path());
    m_view.clip_description->setText(m_clip->description());
    connect(m_view.clip_description, SIGNAL(textChanged(QString)), this, SLOT(slotModified()));

    QMap <QString, QString> props = m_clip->properties();
    m_view.clip_force_out->setHidden(true);
    m_view.clip_out->setHidden(true);
    
    // New display aspect ratio support
    if (props.contains("force_aspect_num") && props.value("force_aspect_num").toInt() > 0 &&
            props.contains("force_aspect_den") && props.value("force_aspect_den").toInt() > 0) {
        m_view.clip_force_ar->setChecked(true);
        m_view.clip_ar_num->setEnabled(true);
        m_view.clip_ar_den->setEnabled(true);
        m_view.clip_ar_num->setValue(props.value("force_aspect_num").toInt());
        m_view.clip_ar_den->setValue(props.value("force_aspect_den").toInt());
    }
    // Legacy support for pixel aspect ratio
    else if (props.contains("force_aspect_ratio") && props.value("force_aspect_ratio").toDouble() > 0) {
        m_view.clip_force_ar->setChecked(true);
        m_view.clip_ar_num->setEnabled(true);
        m_view.clip_ar_den->setEnabled(true);
        if (props.contains("frame_size")) {
            int width = props.value("force_aspect_ratio").toDouble() * props.value("frame_size").section('x', 0, 0).toInt();
            int height = props.value("frame_size").section('x', 1, 1).toInt();
            if (width > 0 && height > 0) {
                if ((width / height * 100) == 133) {
                    width = 4;
                    height = 3;
                }
                else if (int(width / height * 100) == 177) {
                    width = 16;
                    height = 9;
                }
                m_view.clip_ar_num->setValue(width);
                m_view.clip_ar_den->setValue(height);
            }
        }
    }
    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_ar_num, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));
    connect(m_view.clip_ar_den, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));

    if (props.contains("force_fps") && props.value("force_fps").toDouble() > 0) {
        m_view.clip_force_framerate->setChecked(true);
        m_view.clip_framerate->setEnabled(true);
        m_view.clip_framerate->setValue(props.value("force_fps").toDouble());
    }
    connect(m_view.clip_force_framerate, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_framerate, SIGNAL(valueChanged(double)), this, SLOT(slotModified()));
    m_view.clip_progressive->addItem(i18n("Interlaced"), 0);
    m_view.clip_progressive->addItem(i18n("Progressive"), 1);

    if (props.contains("force_progressive")) {
        m_view.clip_force_progressive->setChecked(true);
        m_view.clip_progressive->setEnabled(true);
        m_view.clip_progressive->setCurrentIndex(props.value("force_progressive").toInt());
    }
    connect(m_view.clip_force_progressive, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_progressive, SIGNAL(currentIndexChanged(int)), this, SLOT(slotModified()));

    m_view.clip_fieldorder->addItem(i18n("Bottom first"), 0);
    m_view.clip_fieldorder->addItem(i18n("Top first"), 1);
    if (props.contains("force_tff")) {
        m_view.clip_force_fieldorder->setChecked(true);
        m_view.clip_fieldorder->setEnabled(true);
        m_view.clip_fieldorder->setCurrentIndex(props.value("force_tff").toInt());
    }
    connect(m_view.clip_force_fieldorder, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_fieldorder, SIGNAL(currentIndexChanged(int)), this, SLOT(slotModified()));
    
    if (props.contains("threads") && props.value("threads").toInt() != 1) {
        m_view.clip_force_threads->setChecked(true);
        m_view.clip_threads->setEnabled(true);
        m_view.clip_threads->setValue(props.value("threads").toInt());
    }
    connect(m_view.clip_force_threads, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_threads, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));

    if (props.contains("video_index") && props.value("video_index").toInt() != 0) {
        m_view.clip_force_vindex->setChecked(true);
        m_view.clip_vindex->setEnabled(true);
        m_view.clip_vindex->setValue(props.value("video_index").toInt());
    }
    connect(m_view.clip_force_vindex, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_vindex, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));

    if (props.contains("audio_index") && props.value("audio_index").toInt() != 0) {
        m_view.clip_force_aindex->setChecked(true);
        m_view.clip_aindex->setEnabled(true);
        m_view.clip_aindex->setValue(props.value("audio_index").toInt());
    }
    connect(m_view.clip_force_aindex, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_aindex, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));

    if (props.contains("audio_max")) {
        m_view.clip_aindex->setMaximum(props.value("audio_max").toInt());
    }

    if (props.contains("video_max")) {
        m_view.clip_vindex->setMaximum(props.value("video_max").toInt());
    }
    
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(601), 601);
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(709), 709);
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(240), 240);
    if (props.contains("force_colorspace")) {
        m_view.clip_force_colorspace->setChecked(true);
        m_view.clip_colorspace->setEnabled(true);
        m_view.clip_colorspace->setCurrentIndex(m_view.clip_colorspace->findData(props.value("force_colorspace").toInt()));
    } else if (props.contains("colorspace")) {
        m_view.clip_colorspace->setCurrentIndex(m_view.clip_colorspace->findData(props.value("colorspace").toInt()));
    }
    connect(m_view.clip_force_colorspace, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_colorspace, SIGNAL(currentIndexChanged(int)), this, SLOT(slotModified()));
    
    if (props.contains("full_luma")) {
        m_view.clip_full_luma->setChecked(true);
    }
    connect(m_view.clip_full_luma, SIGNAL(toggled(bool)), this, SLOT(slotModified()));

    // Check for Metadata
    QMap<QString, QStringList> meta = m_clip->metadata();
    QMap<QString, QStringList>::const_iterator i = meta.constBegin();
    while (i != meta.constEnd()) {
        QStringList values = i.value();
        QString parentName;
        QString iconName;
        if (values.count() > 1 && !values.at(1).isEmpty()) {
            parentName = values.at(1);
        } else {
            if (KdenliveSettings::ffmpegpath().endsWith(QLatin1String("avconv"))) {
                parentName = i18n("Libav");
                iconName = "meta_libav.png";
            }
            else {
                parentName = i18n("FFmpeg");
                iconName = "meta_ffmpeg.png";
            }
        }
        QTreeWidgetItem *parent = NULL;
        QList <QTreeWidgetItem *> matches = m_view.metadata_list->findItems(parentName, Qt::MatchExactly);
        if (!matches.isEmpty()) {
            parent = matches.at(0);
        } else {
            if (parentName == "Magic Lantern")
                iconName = "meta_magiclantern.png";
            parent = new QTreeWidgetItem(m_view.metadata_list, QStringList() << parentName);
            if (!iconName.isEmpty()) {
                QIcon icon(QStandardPaths::locate(QStandardPaths::DataLocation, iconName));
                parent->setIcon(0, icon);
            }
        }
        QTreeWidgetItem *metaitem = NULL;
        if (parent) {
            metaitem = new QTreeWidgetItem(parent);
            parent->setExpanded(true);
        }
        else metaitem = new QTreeWidgetItem(m_view.metadata_list);
        metaitem->setText(0, i.key()); //i18n(i.key().section('.', 2, 3).toUtf8().data()));
        metaitem->setText(1, values.at(0));
        ++i;
    }

    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), m_view.clip_ar_num, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), m_view.clip_ar_den, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_framerate, SIGNAL(toggled(bool)), m_view.clip_framerate, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_progressive, SIGNAL(toggled(bool)), m_view.clip_progressive, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_fieldorder, SIGNAL(toggled(bool)), m_view.clip_fieldorder, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_threads, SIGNAL(toggled(bool)), m_view.clip_threads, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_vindex, SIGNAL(toggled(bool)), m_view.clip_vindex, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_aindex, SIGNAL(toggled(bool)), m_view.clip_aindex, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_colorspace, SIGNAL(toggled(bool)), m_view.clip_colorspace, SLOT(setEnabled(bool)));

    if (props.contains("audiocodec"))
        new QTreeWidgetItem(m_view.clip_aproperties, QStringList() << i18n("Audio codec") << props.value("audiocodec"));

    if (props.contains("channels"))
        new QTreeWidgetItem(m_view.clip_aproperties, QStringList() << i18n("Channels") << props.value("channels"));

    if (props.contains("frequency"))
        new QTreeWidgetItem(m_view.clip_aproperties, QStringList() << i18n("Frequency") << props.value("frequency"));
    

    ClipType t = m_clip->clipType();
    
    if (props.contains("kdenlive:proxy") && props.value("kdenlive:proxy") != "-") {
        KFileItem f(QUrl(props.value("kdenlive:proxy")));
        f.setDelayedMimeTypes(true);
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        m_proxyContainer = new QFrame();
        m_proxyContainer->setFrameShape(QFrame::NoFrame);
        QHBoxLayout *l = new QHBoxLayout;
        l->addWidget(new QLabel(i18n("Proxy clip: %1 (%2)", f.name(), KIO::convertSize(f.size()))));
        l->addStretch(5);
        QPushButton *pb = new QPushButton(i18n("Delete proxy"));
        l->addWidget(pb);
        connect(pb, &QPushButton::clicked, this, &ClipProperties::slotDeleteProxy);
        m_proxyContainer->setLayout(l);
        if (t == Image) {
            m_view.tab_image->layout()->addWidget(line);
            m_view.tab_image->layout()->addWidget(m_proxyContainer);
        }
        else if (t == Audio) {
            m_view.tab_audio->layout()->addWidget(line);
            m_view.tab_audio->layout()->addWidget(m_proxyContainer);
        }
        else {
            m_view.tab_video->layout()->addWidget(line);
            m_view.tab_video->layout()->addWidget(m_proxyContainer);
        }
    }
    
    if (t != Audio && t != AV) {
        m_view.clip_force_aindex->setEnabled(false);
    }

    if (t != Video && t != AV) {
        m_view.clip_force_vindex->setEnabled(false);
    }

    if (t == Playlist)
        m_view.tabWidget->setTabText(VIDEOTAB, i18n("Playlist"));

    if (t == Image) {
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        if (props.contains("frame_size"))
            m_view.image_size->setText(props.value("frame_size"));
        if (props.contains("transparency"))
            m_view.image_transparency->setChecked(props.value("transparency").toInt());
        connect(m_view.image_transparency, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
        int width = 180.0 * KdenliveSettings::project_display_ratio();
        if (width % 2 == 1)
            width++;
        m_view.clip_thumb->setPixmap(QPixmap(url.path()).scaled(QSize(width, 180), Qt::KeepAspectRatio));
    } else if (t == Color) {
        m_view.clip_path->setEnabled(false);
        m_view.tabWidget->removeTab(METATAB);
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
        m_view.clip_color->setColor(QColor('#' + props.value("colour").right(8).left(6)));
        connect(m_view.clip_color, SIGNAL(changed(QColor)), this, SLOT(slotModified()));
    } else if (t == SlideShow) {
        if (url.fileName().startsWith(QLatin1String(".all."))) {
            // the image sequence is defined by mimetype
            m_view.clip_path->setText(url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).path());
        } else {
            // the image sequence is defined by pattern
            m_view.slide_type_label->setHidden(true);
            m_view.image_type->setHidden(true);
            m_view.clip_path->setText(url.path());
        }

        m_view.tabWidget->removeTab(METATAB);
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);

        //WARNING: Keep in sync with project/dialogs/slideshowclip.cpp
        m_view.image_type->addItem("JPG (*.jpg)", "jpg");
        m_view.image_type->addItem("JPEG (*.jpeg)", "jpeg");
        m_view.image_type->addItem("PNG (*.png)", "png");
        m_view.image_type->addItem("SVG (*.svg)", "svg");
        m_view.image_type->addItem("BMP (*.bmp)", "bmp");
        m_view.image_type->addItem("GIF (*.gif)", "gif");
        m_view.image_type->addItem("TGA (*.tga)", "tga");
        m_view.image_type->addItem("TIF (*.tif)", "tif");
        m_view.image_type->addItem("TIFF (*.tiff)", "tiff");
        m_view.image_type->addItem("Open EXR (*.exr)", "exr");
        m_view.animation->addItem(i18n("None"), QString());
        m_view.animation->addItem(i18n("Pan"), "Pan");
        m_view.animation->addItem(i18n("Pan, low-pass"), "Pan, low-pass");
        m_view.animation->addItem(i18n("Pan and zoom"), "Pan and zoom");
        m_view.animation->addItem(i18n("Pan and zoom, low-pass"), "Pan and zoom, low-pass");
        m_view.animation->addItem(i18n("Zoom"), "Zoom");
        m_view.animation->addItem(i18n("Zoom, low-pass"), "Zoom, low-pass");

        m_view.slide_loop->setChecked(props.value("loop").toInt());
        m_view.slide_crop->setChecked(props.value("crop").toInt());
        m_view.slide_fade->setChecked(props.value("fade").toInt());
        m_view.luma_softness->setValue(props.value("softness").toInt());
        if (!props.value("animation").isEmpty())
            m_view.animation->setCurrentItem(props.value("animation"));
        else
            m_view.animation->setCurrentIndex(0);
        QString path = props.value("resource");
        QString ext = path.section('.', -1);
        for (int i = 0; i < m_view.image_type->count(); ++i) {
            if (m_view.image_type->itemData(i).toString() == ext) {
                m_view.image_type->setCurrentIndex(i);
                break;
            }
        }
        m_view.slide_duration->setText(tc.getTimecodeFromFrames(props.value("ttl").toInt()));

        m_view.slide_duration_format->addItem(i18n("hh:mm:ss:ff"));
        m_view.slide_duration_format->addItem(i18n("Frames"));
        connect(m_view.slide_duration_format, SIGNAL(activated(int)), this, SLOT(slotUpdateDurationFormat(int)));
        m_view.slide_duration_frames->setHidden(true);
        m_view.luma_duration_frames->setHidden(true);

        parseFolder(false);

        m_view.luma_duration->setText(tc.getTimecodeFromFrames(props.value("luma_duration").toInt()));
        QString lumaFile = props.value("luma_file");

        // Check for Kdenlive installed luma files
        QStringList filters;
        filters << "*.pgm" << "*.png";

        QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::DataLocation, "lumas");
        foreach(const QString & folder, customLumas) {
            QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
            foreach(const QString & fname, filesnames) {
                QString filePath = QUrl(folder + QDir::separator() + fname).path();
                m_view.luma_file->addItem(QIcon::fromTheme(filePath), fname, filePath);
            }
        }

        // Check for MLT lumas
        QString profilePath = KdenliveSettings::mltpath();
        QString folder = profilePath.section('/', 0, -3);
        folder.append("/lumas/PAL"); // TODO: cleanup the PAL / NTSC mess in luma files
        QDir lumafolder(folder);
        QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            QString filePath = lumafolder.absoluteFilePath(fname);
            m_view.luma_file->addItem(QIcon::fromTheme(filePath), fname, filePath);
        }

        if (!lumaFile.isEmpty()) {
            m_view.slide_luma->setChecked(true);
            m_view.luma_file->setCurrentIndex(m_view.luma_file->findData(lumaFile));
        } else m_view.luma_file->setEnabled(false);
        slotEnableLuma(m_view.slide_fade->checkState());
        slotEnableLumaFile(m_view.slide_luma->checkState());

        connect(m_view.slide_fade, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
        connect(m_view.slide_luma, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
        connect(m_view.slide_loop, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
        connect(m_view.slide_crop, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
        connect(m_view.slide_duration, SIGNAL(textChanged(QString)), this, SLOT(slotModified()));
        connect(m_view.slide_duration_frames, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));
        connect(m_view.luma_duration, SIGNAL(textChanged(QString)), this, SLOT(slotModified()));
        connect(m_view.luma_softness, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));
        connect(m_view.luma_file, SIGNAL(currentIndexChanged(int)), this, SLOT(slotModified()));
        connect(m_view.animation, SIGNAL(currentIndexChanged(int)), this, SLOT(slotModified()));


        connect(m_view.slide_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLuma(int)));
        connect(m_view.slide_luma, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLumaFile(int)));
        connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));

    } else if (t != Audio) {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);

        PropertiesViewDelegate *del1 = new PropertiesViewDelegate(this);
        PropertiesViewDelegate *del2 = new PropertiesViewDelegate(this);
        m_view.clip_vproperties->setItemDelegate(del1);
        m_view.clip_aproperties->setItemDelegate(del2);
        m_view.clip_aproperties->setStyleSheet(QString("QTreeWidget { background-color: transparent;}"));
        m_view.clip_vproperties->setStyleSheet(QString("QTreeWidget { background-color: transparent;}"));
        loadVideoProperties(props);
        
        m_view.clip_thumb->setMinimumSize(180 * KdenliveSettings::project_display_ratio(), 180);
        
        if (t == Image || t == Video || t == Playlist)
            m_view.tabWidget->removeTab(AUDIOTAB);
    } else {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
    }

    if (t != SlideShow && t != Color) {
        KFileItem f(url);
        f.setDelayedMimeTypes(true);
        m_view.clip_filesize->setText(KIO::convertSize(f.size()));
    } else {
        m_view.clip_filesize->setHidden(true);
        m_view.label_size->setHidden(true);
    }
    m_view.clip_duration->setInputMask(tc.mask());
    m_view.clip_duration->setText(tc.getTimecode(m_clip->duration()));
    if (t != Image && t != Color && t != Text) {
        m_view.clip_duration->setReadOnly(true);
    } else {
        connect(m_view.clip_duration, SIGNAL(editingFinished()), this, SLOT(slotCheckMaxLength()));
        connect(m_view.clip_duration, SIGNAL(textChanged(QString)), this, SLOT(slotModified()));
    }

    // markers
    m_view.marker_new->setIcon(QIcon::fromTheme("document-new"));
    m_view.marker_new->setToolTip(i18n("Add marker"));
    m_view.marker_edit->setIcon(QIcon::fromTheme("document-properties"));
    m_view.marker_edit->setToolTip(i18n("Edit marker"));
    m_view.marker_delete->setIcon(QIcon::fromTheme("trash-empty"));
    m_view.marker_delete->setToolTip(i18n("Delete marker"));
    m_view.marker_save->setIcon(QIcon::fromTheme("document-save-as"));
    m_view.marker_save->setToolTip(i18n("Save markers"));
    m_view.marker_load->setIcon(QIcon::fromTheme("document-open"));
    m_view.marker_load->setToolTip(i18n("Load markers"));
    m_view.analysis_delete->setIcon(QIcon::fromTheme("trash-empty"));
    m_view.analysis_delete->setToolTip(i18n("Delete analysis data"));
    m_view.analysis_load->setIcon(QIcon::fromTheme("document-open"));
    m_view.analysis_load->setToolTip(i18n("Load analysis data"));
    m_view.analysis_save->setIcon(QIcon::fromTheme("document-save-as"));
    m_view.analysis_save->setToolTip(i18n("Save analysis data"));
    slotFillMarkersList(m_clip);
    slotUpdateAnalysisData(m_clip);
    
    connect(m_view.marker_new, SIGNAL(clicked()), this, SLOT(slotAddMarker()));
    connect(m_view.marker_edit, SIGNAL(clicked()), this, SLOT(slotEditMarker()));
    connect(m_view.marker_delete, SIGNAL(clicked()), this, SLOT(slotDeleteMarker()));
    connect(m_view.marker_save, SIGNAL(clicked()), this, SLOT(slotSaveMarkers()));
    connect(m_view.marker_load, SIGNAL(clicked()), this, SLOT(slotLoadMarkers()));
    connect(m_view.markers_list, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(slotEditMarker()));
    
    connect(m_view.analysis_delete, SIGNAL(clicked()), this, SLOT(slotDeleteAnalysis()));
    connect(m_view.analysis_save, SIGNAL(clicked()), this, SLOT(slotSaveAnalysis()));
    connect(m_view.analysis_load, SIGNAL(clicked()), this, SLOT(slotLoadAnalysis()));
    
    connect(this, &ClipProperties::accepted, this, &ClipProperties::slotApplyProperties);
    connect(m_view.buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &ClipProperties::slotApplyProperties);
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    
    m_view.metadata_list->resizeColumnToContents(0);
    m_view.clip_vproperties->resizeColumnToContents(0);
    m_view.clip_aproperties->resizeColumnToContents(0);
    adjustSize();
}


// Used for multiple clips editing
ClipProperties::ClipProperties(const QList <DocClipBase *> &cliplist, const Timecode &tc, const QMap <QString, QString> &commonproperties, QWidget * parent) :
    QDialog(parent),
    m_clip(NULL),
    m_tc(tc),
    m_fps(0),
    m_count(0),
    m_clipNeedsRefresh(false),
    m_clipNeedsReLoad(false)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    QString title = windowTitle();
    title.append(' ' + i18np("(%1 clip)", "(%1 clips)", cliplist.count()));
    setWindowTitle(title);
    QMap <QString, QString> props = cliplist.at(0)->properties();
    m_old_props = commonproperties;

    if (commonproperties.contains("force_aspect_num") && !commonproperties.value("force_aspect_num").isEmpty() && commonproperties.value("force_aspect_den").toInt() > 0) {
        m_view.clip_force_ar->setChecked(true);
        m_view.clip_ar_num->setEnabled(true);
        m_view.clip_ar_den->setEnabled(true);
        m_view.clip_ar_num->setValue(commonproperties.value("force_aspect_num").toInt());
        m_view.clip_ar_den->setValue(commonproperties.value("force_aspect_den").toInt());
    }

    if (commonproperties.contains("force_fps") && !commonproperties.value("force_fps").isEmpty() && commonproperties.value("force_fps").toDouble() > 0) {
        m_view.clip_force_framerate->setChecked(true);
        m_view.clip_framerate->setEnabled(true);
        m_view.clip_framerate->setValue(commonproperties.value("force_fps").toDouble());
    }

    if (commonproperties.contains("force_progressive") && !commonproperties.value("force_progressive").isEmpty()) {
        m_view.clip_force_progressive->setChecked(true);
        m_view.clip_progressive->setEnabled(true);
        m_view.clip_progressive->setCurrentIndex(commonproperties.value("force_progressive").toInt());
    }

    if (commonproperties.contains("force_tff") && !commonproperties.value("force_tff").isEmpty()) {
        m_view.clip_force_fieldorder->setChecked(true);
        m_view.clip_fieldorder->setEnabled(true);
        m_view.clip_fieldorder->setCurrentIndex(commonproperties.value("force_tff").toInt());
    }
    
    if (commonproperties.contains("threads") && !commonproperties.value("threads").isEmpty() && commonproperties.value("threads").toInt() != 1) {
        m_view.clip_force_threads->setChecked(true);
        m_view.clip_threads->setEnabled(true);
        m_view.clip_threads->setValue(commonproperties.value("threads").toInt());
    }

    if (commonproperties.contains("video_index") && !commonproperties.value("video_index").isEmpty() && commonproperties.value("video_index").toInt() != 0) {
        m_view.clip_force_vindex->setChecked(true);
        m_view.clip_vindex->setEnabled(true);
        m_view.clip_vindex->setValue(commonproperties.value("video_index").toInt());
    }

    if (commonproperties.contains("audio_index") && !commonproperties.value("audio_index").isEmpty() && commonproperties.value("audio_index").toInt() != 0) {
        m_view.clip_force_aindex->setChecked(true);
        m_view.clip_aindex->setEnabled(true);
        m_view.clip_aindex->setValue(commonproperties.value("audio_index").toInt());
    }

    if (props.contains("audio_max")) {
        m_view.clip_aindex->setMaximum(props.value("audio_max").toInt());
    }

    if (props.contains("video_max")) {
        m_view.clip_vindex->setMaximum(props.value("video_max").toInt());
    }
    
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(601), 601);
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(709), 709);
    m_view.clip_colorspace->addItem(ProfilesDialog::getColorspaceDescription(240), 240);
    
    if (commonproperties.contains("force_colorspace") && !commonproperties.value("force_colorspace").isEmpty() && commonproperties.value("force_colorspace").toInt() != 0) {
        m_view.clip_force_colorspace->setChecked(true);
        m_view.clip_colorspace->setEnabled(true);
        m_view.clip_colorspace->setCurrentIndex(m_view.clip_colorspace->findData(commonproperties.value("force_colorspace").toInt()));
    }
    
    if (commonproperties.contains("full_luma") && !commonproperties.value("full_luma").isEmpty()) {
        m_view.clip_full_luma->setChecked(true);
    }
    
    if (commonproperties.contains("transparency")) {
        // image transparency checkbox
        int transparency = commonproperties.value("transparency").toInt();
        if (transparency == 0) {
            m_view.clip_force_transparency->setChecked(true);
        }
        else if (transparency == 1) {
            m_view.clip_force_transparency->setChecked(true);
            m_view.clip_transparency->setCurrentIndex(1);
        }
    }
    else {
        m_view.clip_force_transparency->setHidden(true);
        m_view.clip_transparency->setHidden(true);
    }
    

    connect(m_view.clip_force_transparency, SIGNAL(toggled(bool)), m_view.clip_transparency, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), m_view.clip_ar_num, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), m_view.clip_ar_den, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_progressive, SIGNAL(toggled(bool)), m_view.clip_progressive, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_threads, SIGNAL(toggled(bool)), m_view.clip_threads, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_vindex, SIGNAL(toggled(bool)), m_view.clip_vindex, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_aindex, SIGNAL(toggled(bool)), m_view.clip_aindex, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_out, SIGNAL(toggled(bool)), m_view.clip_out, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_colorspace, SIGNAL(toggled(bool)), m_view.clip_colorspace, SLOT(setEnabled(bool)));

    m_view.tabWidget->removeTab(METATAB);
    m_view.tabWidget->removeTab(MARKERTAB);
    m_view.tabWidget->removeTab(IMAGETAB);
    m_view.tabWidget->removeTab(SLIDETAB);
    m_view.tabWidget->removeTab(COLORTAB);
    m_view.tabWidget->removeTab(AUDIOTAB);
    m_view.tabWidget->removeTab(VIDEOTAB);

    m_view.clip_path->setHidden(true);
    m_view.label_path->setHidden(true);
    m_view.label_description->setHidden(true);
    m_view.label_size->setHidden(true);
    m_view.clip_filesize->setHidden(true);
    m_view.clip_filesize->setHidden(true);
    m_view.clip_path->setHidden(true);
    m_view.clip_description->setHidden(true);
    m_view.clip_thumb->setHidden(true);
    m_view.label_duration->setHidden(true);
    m_view.clip_duration->setHidden(true);

    if (commonproperties.contains("out")) {
        if (commonproperties.value("out").toInt() > 0) {
            m_view.clip_force_out->setChecked(true);
            m_view.clip_out->setText(m_tc.getTimecodeFromFrames(commonproperties.value("out").toInt()));
        } else {
            m_view.clip_out->setText(KdenliveSettings::image_duration());
        }
    } else {
        m_view.clip_force_out->setHidden(true);
        m_view.clip_out->setHidden(true);
    }
}

ClipProperties::~ClipProperties()
{
    QAbstractItemDelegate *del1 = m_view.clip_vproperties->itemDelegate();
    delete del1;
    QAbstractItemDelegate *del2 = m_view.clip_aproperties->itemDelegate();
    delete del2;
}


void ClipProperties::loadVideoProperties(const QMap <QString, QString> &props)
{
    m_view.clip_vproperties->clear();
    if (props.contains("videocodec"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Video codec") << props.value("videocodec"));
    else if (props.contains("videocodecid"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Video codec") << props.value("videocodecid"));

    if (props.contains("frame_size"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Frame size") << props.value("frame_size"));

    if (props.contains("fps")) {
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Frame rate") << props.value("fps"));
        if (!m_view.clip_framerate->isEnabled()) m_view.clip_framerate->setValue(props.value("fps").toDouble());
    }

    if (props.contains("progressive")) {
        int scanning = props.value("progressive").toInt();
        QString txt = scanning == 1 ? i18n("Progressive") : i18n("Interlaced");
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Scanning") << txt);
    }

    if (props.contains("aspect_ratio"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Pixel aspect ratio") << props.value("aspect_ratio"));

    if (props.contains("pix_fmt"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Pixel format") << props.value("pix_fmt"));

    if (props.contains("colorspace"))
        new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Colorspace") << ProfilesDialog::getColorspaceDescription(props.value("colorspace").toInt()));
}

void ClipProperties::slotGotThumbnail(const QString &id, const QImage &img)
{
    if (id != m_clip->getId())
        return;
    QPixmap framedPix(img.width(), img.height());
    framedPix.fill(Qt::transparent);
    QPainter p(&framedPix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(0.5, 0.5, framedPix.width() - 1, framedPix.height() - 1, 4, 4);
    p.setClipPath(path);
    p.drawImage(0, 0, img);
    p.end();
    m_view.clip_thumb->setPixmap(framedPix);
}

void ClipProperties::slotApplyProperties()
{
    if (m_clip != NULL) {
        QMap <QString, QString> props = properties();
        emit applyNewClipProperties(m_clip->getId(), m_clip->currentProperties(props), props, needsTimelineRefresh(), needsTimelineReload());
        QTimer::singleShot(1000, this, SLOT(slotReloadVideoProperties()));
        if (props.contains("force_aspect_num"))
            QTimer::singleShot(1000, this, SLOT(slotReloadVideoThumb()));
    }
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void ClipProperties::slotReloadVideoProperties()
{
    if (m_clip == NULL)
        return;
    loadVideoProperties(m_clip->properties());
}

void ClipProperties::slotReloadVideoThumb()
{
    if (m_clip == NULL)
        return;
    emit requestThumb(QString('?' + m_clip->getId()), QList<int>() << m_clip->getClipThumbFrame());
}

void ClipProperties::slotModified()
{
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}


void ClipProperties::slotEnableLuma(int state)
{
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_duration->setEnabled(enable);
    m_view.luma_duration_frames->setEnabled(enable);
    m_view.slide_luma->setEnabled(enable);
    if (enable) {
        m_view.luma_file->setEnabled(m_view.slide_luma->isChecked());
    } else m_view.luma_file->setEnabled(false);
    m_view.label_softness->setEnabled(m_view.slide_luma->isChecked() && enable);
    m_view.luma_softness->setEnabled(m_view.label_softness->isEnabled());
}

void ClipProperties::slotEnableLumaFile(int state)
{
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_file->setEnabled(enable);
    m_view.luma_softness->setEnabled(enable);
    m_view.label_softness->setEnabled(enable);
}

void ClipProperties::slotUpdateAnalysisData(DocClipBase *clip)
{
    if (m_clip != clip) return;
    m_view.analysis_list->clear();
    QMap <QString, QString> analysis = clip->analysisData();
    m_view.analysis_box->setHidden(analysis.isEmpty());
    QMap<QString, QString>::const_iterator i = analysis.constBegin();
    while (i != analysis.constEnd()) {
        QStringList itemtext;
        itemtext << i.key() << i.value();
        (void) new QTreeWidgetItem(m_view.analysis_list, itemtext);
        ++i;
    }
}

void ClipProperties::slotFillMarkersList(DocClipBase *clip)
{
    if (m_clip != clip) return;
    m_view.markers_list->clear();
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    for (int count = 0; count < marks.count(); ++count) {
        QString time = m_tc.getTimecode(marks[count].time());
        QStringList itemtext;
        itemtext << time << marks.at(count).comment();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_view.markers_list, itemtext);
        item->setData(0, Qt::DecorationRole, CommentedTime::markerColor(marks.at(count).markerType()));
    }
}

void ClipProperties::slotAddMarker()
{
    CommentedTime marker(GenTime(), i18n("Marker"));
    QPointer<MarkerDialog> d = new MarkerDialog(m_clip, marker,
                                                m_tc, i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        QList <CommentedTime> markers;
        markers << d->newMarker();
        emit addMarkers(m_clip->getId(), markers);
    }
    delete d;
}

void ClipProperties::slotSaveMarkers()
{
    emit saveMarkers(m_clip->getId());
}

void ClipProperties::slotLoadMarkers()
{
    emit loadMarkers(m_clip->getId());
}

void ClipProperties::slotEditMarker()
{
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    int pos = m_view.markers_list->currentIndex().row();
    if (pos < 0 || pos > marks.count() - 1) return;
    QPointer<MarkerDialog> d = new MarkerDialog(m_clip, marks.at(pos), m_tc, i18n("Edit Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        QList <CommentedTime> markers;
        markers << d->newMarker();
        emit addMarkers(m_clip->getId(), markers);
    }
    delete d;
}

void ClipProperties::slotDeleteMarker()
{
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    QList < CommentedTime > toDelete;
    for (int i = 0; i < marks.count(); ++i) {
        if (m_view.markers_list->topLevelItem(i)->isSelected()) {
            CommentedTime marker = marks.at(i);
            marker.setMarkerType(-1);
            toDelete << marker;
        }
    }
    emit addMarkers(m_clip->getId(), toDelete);
}

void ClipProperties::slotDeleteAnalysis()
{
    QTreeWidgetItem *current = m_view.analysis_list->currentItem();
    if (current) emit editAnalysis(m_clip->getId(), current->text(0), QString());
}

void ClipProperties::slotSaveAnalysis()
{
    const QString url = QFileDialog::getSaveFileName(this, i18n("Save Analysis Data"), m_clip->fileURL().adjusted(QUrl::RemoveFilename).path(), i18n("Text File (*.txt)"));
    if (url.isEmpty())
        return;
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup analysisConfig(config, "Analysis");
    QTreeWidgetItem *current = m_view.analysis_list->currentItem();
    analysisConfig.writeEntry(current->text(0), current->text(1));
}

void ClipProperties::slotLoadAnalysis()
{
    const QString url = QFileDialog::getOpenFileName(this, i18n("Open Analysis Data"), m_clip->fileURL().adjusted(QUrl::RemoveFilename).path(), i18n("Text File (*.txt)"));
    if (url.isEmpty())
        return;
    KSharedConfigPtr config = KSharedConfig::openConfig(url, KConfig::SimpleConfig);
    KConfigGroup transConfig(config, "Analysis");
    // read the entries
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        emit editAnalysis(m_clip->getId(), i.key(), i.value());
    }
}

const QString &ClipProperties::clipId() const
{
    return m_clip->getId();
}

QMap <QString, QString> ClipProperties::properties()
{
    QMap <QString, QString> props;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    ClipType t = Unknown;
    if (m_clip != NULL) {
        t = m_clip->clipType();
        m_old_props = m_clip->properties();
    }

    int aspectNumerator = m_view.clip_ar_num->value();
    int aspectDenominator = m_view.clip_ar_den->value();
    if (m_view.clip_force_ar->isChecked()) {
        if (aspectNumerator != m_old_props.value("force_aspect_num").toInt() ||
                aspectDenominator != m_old_props.value("force_aspect_den").toInt()) {
            props["force_aspect_num"] = QString::number(aspectNumerator);
            props["force_aspect_den"] = QString::number(aspectDenominator);
            props["force_aspect_ratio"].clear();
            m_clipNeedsRefresh = true;
        }
    } else {
        if (m_old_props.contains("force_aspect_num") && !m_old_props.value("force_aspect_num").isEmpty()) {
            props["force_aspect_num"].clear();
            m_clipNeedsRefresh = true;
        }
        if (m_old_props.contains("force_aspect_den") && !m_old_props.value("force_aspect_den").isEmpty()) {
            props["force_aspect_den"].clear();
            m_clipNeedsRefresh = true;
        }
    }

    double fps = m_view.clip_framerate->value();
    if (m_view.clip_force_framerate->isChecked()) {
        if (fps != m_old_props.value("force_fps").toDouble()) {
            props["force_fps"] = locale.toString(fps);
            m_clipNeedsRefresh = true;
        }
    } else if (m_old_props.contains("force_fps") && !m_old_props.value("force_fps").isEmpty()) {
        props["force_fps"].clear();
        m_clipNeedsRefresh = true;
    }

    int progressive = m_view.clip_progressive->currentIndex();
    if (m_view.clip_force_progressive->isChecked()) {
        if (!m_old_props.contains("force_progressive") || progressive != m_old_props.value("force_progressive").toInt()) {
            props["force_progressive"] = QString::number(progressive);
        }
    } else if (m_old_props.contains("force_progressive") && !m_old_props.value("force_progressive").isEmpty()) {
        props["force_progressive"].clear();
    }

    int fieldOrder = m_view.clip_fieldorder->currentIndex();
    if (m_view.clip_force_fieldorder->isChecked()) {
        if (!m_old_props.contains("force_tff") || fieldOrder != m_old_props.value("force_tff").toInt()) {
            props["force_tff"] = QString::number(fieldOrder);
        }
    } else if (m_old_props.contains("force_tff") && !m_old_props.value("force_tff").isEmpty()) {
        props["force_tff"].clear();
    }

    int threads = m_view.clip_threads->value();
    if (m_view.clip_force_threads->isChecked()) {
        if (threads != m_old_props.value("threads").toInt()) {
            props["threads"] = QString::number(threads);
        }
    } else if (m_old_props.contains("threads") && !m_old_props.value("threads").isEmpty()) {
        props["threads"].clear();
    }

    int vindex = m_view.clip_vindex->value();
    if (m_view.clip_force_vindex->isChecked()) {
        if (vindex != m_old_props.value("video_index").toInt()) {
            props["video_index"] = QString::number(vindex);
        }
    } else if (m_old_props.contains("video_index") && !m_old_props.value("video_index").isEmpty()) {
        props["video_index"].clear();
    }

    int aindex = m_view.clip_aindex->value();
    if (m_view.clip_force_aindex->isChecked()) {
        if (aindex != m_old_props.value("audio_index").toInt()) {
            props["audio_index"] = QString::number(aindex);
        }
    } else if (m_old_props.contains("audio_index") && !m_old_props.value("audio_index").isEmpty()) {
        props["audio_index"].clear();
    }
    
    int colorspace = m_view.clip_colorspace->itemData(m_view.clip_colorspace->currentIndex()).toInt();
    if (m_view.clip_force_colorspace->isChecked()) {
        if (colorspace != m_old_props.value("force_colorspace").toInt()) {
            props["force_colorspace"] = QString::number(colorspace);
            m_clipNeedsRefresh = true;
        }
    } else if (m_old_props.contains("force_colorspace") && !m_old_props.value("force_colorspace").isEmpty()) {
        props["force_colorspace"].clear();
        m_clipNeedsRefresh = true;
    }

    if (m_view.clip_full_luma->isChecked()) {
        props["full_luma"] = QString::number(1);
        m_clipNeedsRefresh = true;
    } else if (m_old_props.contains("full_luma") && !m_old_props.value("full_luma").isEmpty()) {
        props["full_luma"].clear();
        m_clipNeedsRefresh = true;
    }
    
    if (m_view.clip_force_transparency->isChecked()) {
        QString transp = QString::number(m_view.clip_transparency->currentIndex());
        if (transp != m_old_props.value("transparency")) props["transparency"] = transp;
    }

    // If we adjust several clips, return now
    if (m_clip == NULL) {
        if (m_view.clip_out->isEnabled()) {
            int duration = m_tc.getFrameCount(m_view.clip_out->text());
            if (duration != m_old_props.value("out").toInt()) {
                props["out"] = QString::number(duration - 1);
            }
        }
        return props;
    }

    if (m_old_props.value("description") != m_view.clip_description->text())
        props["description"] = m_view.clip_description->text();

    if (t == Color) {
        QString new_color = m_view.clip_color->color().name();
        if (new_color != QString('#' + m_old_props.value("colour").right(8).left(6))) {
            m_clipNeedsRefresh = true;
            props["colour"] = "0x" + new_color.right(6) + "ff";
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text());
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration - 1);
        }
    } else if (t == Image) {
        if ((int) m_view.image_transparency->isChecked() != m_old_props.value("transparency").toInt()) {
            props["transparency"] = QString::number((int)m_view.image_transparency->isChecked());
            //m_clipNeedsRefresh = true;
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text());
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration - 1);
        }
    } else if (t == SlideShow) {
        QString value = QString::number((int) m_view.slide_loop->isChecked());
        if (m_old_props.value("loop") != value) props["loop"] = value;
        value = QString::number((int) m_view.slide_crop->isChecked());
        if (m_old_props.value("crop") != value) props["crop"] = value;
        value = QString::number((int) m_view.slide_fade->isChecked());
        if (m_old_props.value("fade") != value) props["fade"] = value;
        value = QString::number((int) m_view.luma_softness->value());
        if (m_old_props.value("softness") != value) props["softness"] = value;

        bool isMime = !(m_view.clip_path->text().contains('%'));
        if (isMime) {
            QString extension = "/.all." + m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
            QString new_path = m_view.clip_path->text() + extension;
            if (new_path != m_old_props.value("resource")) {
                m_clipNeedsReLoad = true;
                props["resource"] = new_path;
                //qDebug() << "////  SLIDE EDIT, NEW:" << new_path << ", OLD; " << m_old_props.value("resource");
            }
        }
        int duration;
        if (m_view.slide_duration_format->currentIndex() == 1) {
            // we are in frames mode
            duration = m_view.slide_duration_frames->value();
        } else duration = m_tc.getFrameCount(m_view.slide_duration->text());
        if (duration != m_old_props.value("ttl").toInt()) {
            m_clipNeedsRefresh = true;
            props["ttl"] = QString::number(duration);
            props["length"] = QString::number(duration * m_count);
        }

        if (duration * m_count - 1 != m_old_props.value("out").toInt()) {
            m_clipNeedsRefresh = true;
            props["out"] = QString::number(duration * m_count - 1);
        }
        if (m_view.slide_fade->isChecked()) {
            int luma_duration;
            if (m_view.slide_duration_format->currentIndex() == 1) {
                // we are in frames mode
                luma_duration = m_view.luma_duration_frames->value();
            } else luma_duration = m_tc.getFrameCount(m_view.luma_duration->text());
            if (luma_duration != m_old_props.value("luma_duration").toInt()) {
                m_clipNeedsRefresh = true;
                props["luma_duration"] = QString::number(luma_duration);
            }
            QString lumaFile;
            if (m_view.slide_luma->isChecked())
                lumaFile = m_view.luma_file->itemData(m_view.luma_file->currentIndex()).toString();
            if (lumaFile != m_old_props.value("luma_file")) {
                m_clipNeedsRefresh = true;
                props["luma_file"] = lumaFile;
            }
        } else {
            if (!m_old_props.value("luma_file").isEmpty()) {
                props["luma_file"].clear();
            }
        }

        QString animation = m_view.animation->itemData(m_view.animation->currentIndex()).toString();
        if (animation != m_old_props.value("animation")) {
            if (animation.isEmpty()) {
                props["animation"].clear();
            } else {
                props["animation"] = animation;
            }
            m_clipNeedsRefresh = true;
        }
    }
    return props;
}

bool ClipProperties::needsTimelineRefresh() const
{
    return m_clipNeedsRefresh;
}

bool ClipProperties::needsTimelineReload() const
{
    return m_clipNeedsReLoad;
}


void ClipProperties::parseFolder(bool reloadThumb)
{
    QString path = m_view.clip_path->text();
    bool isMime = !(path.contains('%'));
    if (!isMime) path = QUrl(path).adjusted(QUrl::RemoveFilename).path();
    QDir dir(path);

    QStringList filters;
    QString extension;

    if (isMime) {
        // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
        filters << "*." + m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
        extension = "/.all." + m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
        dir.setNameFilters(filters);
    }

    QStringList result = dir.entryList(QDir::Files);

    if (!isMime) {
        int offset = 0;
        QString path = m_view.clip_path->text();
        if (path.contains('?')) {
            // New MLT syntax
            offset = m_view.clip_path->text().section(':', -1).toInt();
            path = path.section('?', 0, 0);
        }
        QString filter = QUrl(path).fileName();
        QString ext = filter.section('.', -1);
        filter = filter.section('%', 0, -2);
        QString regexp = '^' + filter + "\\d+\\." + ext + '$';
        QRegExp rx(regexp);
        QStringList entries;
        int ix;
        foreach(const QString & path, result) {
            if (rx.exactMatch(path)) {
                if (offset > 0) {
                    // make sure our image is in the range we want (> begin)
                    ix = path.section(filter, 1).section('.', 0, 0).toInt();
                    if (ix < offset) continue;
                }
                entries << path;
            }
        }
        result = entries;
    }

    m_count = result.count();
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(m_count > 0);
    if (m_count == 0) {
        // no images, do not accept that
        m_view.slide_info->setText(i18n("No image found"));
        m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }
    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    m_view.slide_info->setText(i18np("1 image found", "%1 images found", m_count));
    QMap <QString, QString> props = m_clip->properties();
    m_view.clip_duration->setText(m_tc.getTimecodeFromFrames(props.value("ttl").toInt() * m_count));
    if (reloadThumb) {
        int width = 180.0 * KdenliveSettings::project_display_ratio();
        if (width % 2 == 1) width++;
        QString filePath = m_view.clip_path->text();
        if (isMime) filePath.append(extension);
        QPixmap pix = m_clip->thumbProducer()->getImage(QUrl(filePath), 1, width, 180);
        m_view.clip_thumb->setPixmap(pix);
    }
}

void ClipProperties::slotCheckMaxLength()
{
    if (m_clip->maxDuration() == GenTime())
        return;
    const int duration = m_tc.getFrameCount(m_view.clip_duration->text());
    if (duration > m_clip->maxDuration().frames(m_fps)) {
        m_view.clip_duration->setText(m_tc.getTimecode(m_clip->maxDuration()));
    }
}

void ClipProperties::slotUpdateDurationFormat(int ix)
{
    bool framesFormat = (ix == 1);
    if (framesFormat) {
        // switching to frames count, update widget
        m_view.slide_duration_frames->setValue(m_tc.getFrameCount(m_view.slide_duration->text()));
        m_view.luma_duration_frames->setValue(m_tc.getFrameCount(m_view.luma_duration->text()));
        m_view.slide_duration->setHidden(true);
        m_view.luma_duration->setHidden(true);
        m_view.slide_duration_frames->setHidden(false);
        m_view.luma_duration_frames->setHidden(false);
    } else {
        // switching to timecode format
        m_view.slide_duration->setText(m_tc.getTimecodeFromFrames(m_view.slide_duration_frames->value()));
        m_view.luma_duration->setText(m_tc.getTimecodeFromFrames(m_view.luma_duration_frames->value()));
        m_view.slide_duration_frames->setHidden(true);
        m_view.luma_duration_frames->setHidden(true);
        m_view.slide_duration->setHidden(false);
        m_view.luma_duration->setHidden(false);
    }
}

void ClipProperties::slotDeleteProxy()
{
    const QString proxy = m_clip->getProperty("kdenlive:proxy");
    if (proxy.isEmpty())
        return;
    emit deleteProxy(proxy);
    delete m_proxyContainer;
}

void ClipProperties::slotOpenUrl(const QString &url)
{
    new KRun(QUrl(url), this);
}





