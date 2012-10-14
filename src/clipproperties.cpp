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
#include "kthumb.h"
#include "markerdialog.h"
#include "profilesdialog.h"

#include <KStandardDirs>
#include <KDebug>
#include <KFileItem>
#include <kdeversion.h>
#include <KUrlLabel>
#include <KRun>

#ifdef USE_NEPOMUK
#if KDE_IS_VERSION(4,6,0)
#include <Nepomuk/Variant>
#include <Nepomuk/Resource>
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Vocabulary/NIE>
#endif
#endif


#include <QDir>
#include <QPainter>


static const int VIDEOTAB = 0;
static const int AUDIOTAB = 1;
static const int COLORTAB = 2;
static const int SLIDETAB = 3;
static const int IMAGETAB = 4;
static const int MARKERTAB = 5;
static const int METATAB = 6;
static const int ADVANCEDTAB = 7;


ClipProperties::ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent) :
    QDialog(parent),
    m_clip(clip),
    m_tc(tc),
    m_fps(fps),
    m_count(0),
    m_clipNeedsRefresh(false),
    m_clipNeedsReLoad(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    
    // force transparency is only for group properties, so hide it
    m_view.clip_force_transparency->setHidden(true);
    m_view.clip_transparency->setHidden(true);
    
    KUrl url = m_clip->fileURL();
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

    if (props.contains("force_progressive")) {
        m_view.clip_force_progressive->setChecked(true);
        m_view.clip_progressive->setEnabled(true);
        m_view.clip_progressive->setValue(props.value("force_progressive").toInt());
    }
    connect(m_view.clip_force_progressive, SIGNAL(toggled(bool)), this, SLOT(slotModified()));
    connect(m_view.clip_progressive, SIGNAL(valueChanged(int)), this, SLOT(slotModified()));

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
    QMap<QString, QString> meta = m_clip->metadata();
    QMap<QString, QString>::const_iterator i = meta.constBegin();
    while (i != meta.constEnd()) {
        QTreeWidgetItem *metaitem = new QTreeWidgetItem(m_view.metadata_list);
        metaitem->setText(0, i.key()); //i18n(i.key().section('.', 2, 3).toUtf8().data()));
        metaitem->setText(1, i.value());
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
    

    CLIPTYPE t = m_clip->clipType();
    
    if (props.contains("proxy") && props.value("proxy") != "-") {
        KFileItem f(KFileItem::Unknown, KFileItem::Unknown, KUrl(props.value("proxy")), true);
        QFrame* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        m_proxyContainer = new QFrame();
        m_proxyContainer->setFrameShape(QFrame::NoFrame);
        QHBoxLayout *l = new QHBoxLayout;
        l->addWidget(new QLabel(i18n("Proxy clip: %1", KIO::convertSize(f.size()))));
        l->addStretch(5);
        QPushButton *pb = new QPushButton(i18n("Delete proxy"));
        l->addWidget(pb);
        connect(pb, SIGNAL(clicked()), this, SLOT(slotDeleteProxy()));
        m_proxyContainer->setLayout(l);
        if (t == IMAGE) {
            m_view.tab_image->layout()->addWidget(line);
            m_view.tab_image->layout()->addWidget(m_proxyContainer);
        }
        else if (t == AUDIO) {
            m_view.tab_audio->layout()->addWidget(line);
            m_view.tab_audio->layout()->addWidget(m_proxyContainer);
        }
        else {
            m_view.tab_video->layout()->addWidget(line);
            m_view.tab_video->layout()->addWidget(m_proxyContainer);
        }
    }
    
    if (t != AUDIO && t != AV) {
        m_view.clip_force_aindex->setEnabled(false);
    }

    if (t != VIDEO && t != AV) {
        m_view.clip_force_vindex->setEnabled(false);
    }

    if (t == PLAYLIST)
	m_view.tabWidget->setTabText(VIDEOTAB, i18n("Playlist"));

    if (t == IMAGE) {
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
        if (width % 2 == 1) width++;
        m_view.clip_thumb->setPixmap(QPixmap(url.path()).scaled(QSize(width, 180), Qt::KeepAspectRatio));
    } else if (t == COLOR) {
        m_view.clip_path->setEnabled(false);
        m_view.tabWidget->removeTab(METATAB);
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
        m_view.clip_color->setColor(QColor('#' + props.value("colour").right(8).left(6)));
        connect(m_view.clip_color, SIGNAL(changed(QColor)), this, SLOT(slotModified()));
    } else if (t == SLIDESHOW) {
        if (url.fileName().startsWith(".all.")) {
            // the image sequence is defined by mimetype
            m_view.clip_path->setText(url.directory());
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

        //WARNING: Keep in sync with slideshowclip.cpp
        m_view.image_type->addItem("JPG (*.jpg)", "jpg");
        m_view.image_type->addItem("JPEG (*.jpeg)", "jpeg");
        m_view.image_type->addItem("PNG (*.png)", "png");
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
        for (int i = 0; i < m_view.image_type->count(); i++) {
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

        parseFolder();

        m_view.luma_duration->setText(tc.getTimecodeFromFrames(props.value("luma_duration").toInt()));
        QString lumaFile = props.value("luma_file");

        // Check for Kdenlive installed luma files
        QStringList filters;
        filters << "*.pgm" << "*.png";

        QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
        foreach(const QString & folder, customLumas) {
            QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
            foreach(const QString & fname, filesnames) {
                QString filePath = KUrl(folder).path(KUrl::AddTrailingSlash) + fname;
                m_view.luma_file->addItem(KIcon(filePath), fname, filePath);
            }
        }

        // Check for MLT lumas
        QString profilePath = KdenliveSettings::mltpath();
        QString folder = profilePath.section('/', 0, -3);
        folder.append("/lumas/PAL"); // TODO: cleanup the PAL / NTSC mess in luma files
        QDir lumafolder(folder);
        QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            QString filePath = KUrl(folder).path(KUrl::AddTrailingSlash) + fname;
            m_view.luma_file->addItem(KIcon(filePath), fname, filePath);
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

    } else if (t != AUDIO) {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);

        PropertiesViewDelegate *del1 = new PropertiesViewDelegate(this);
        PropertiesViewDelegate *del2 = new PropertiesViewDelegate(this);
        m_view.clip_vproperties->setItemDelegate(del1);
        m_view.clip_aproperties->setItemDelegate(del2);
        m_view.clip_aproperties->setStyleSheet(QString("QTreeWidget { background-color: transparent;}"));
        m_view.clip_vproperties->setStyleSheet(QString("QTreeWidget { background-color: transparent;}"));

        if (props.contains("videocodec"))
            new QTreeWidgetItem(m_view.clip_vproperties, QStringList() << i18n("Video codec") << props.value("videocodec"));

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
        

        int width = 180.0 * KdenliveSettings::project_display_ratio();
        if (width % 2 == 1) width++;
        QPixmap pix = m_clip->thumbProducer()->getImage(url, m_clip->getClipThumbFrame(), width, 180);
	QPixmap framedPix(pix.width(), pix.height());
	framedPix.fill(Qt::transparent);
	QPainter p(&framedPix);
	p.setRenderHint(QPainter::Antialiasing, true);
	QPainterPath path;
	path.addRoundedRect(0.5, 0.5, framedPix.width() - 1, framedPix.height() - 1, 4, 4);
	p.setClipPath(path);
	p.drawPixmap(0, 0, pix);
	p.end();
	
        m_view.clip_thumb->setPixmap(framedPix);
        if (t == IMAGE || t == VIDEO || t == PLAYLIST) m_view.tabWidget->removeTab(AUDIOTAB);
    } else {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
    }

    if (t != SLIDESHOW && t != COLOR) {
        KFileItem f(KFileItem::Unknown, KFileItem::Unknown, url, true);
        m_view.clip_filesize->setText(KIO::convertSize(f.size()));
    } else {
        m_view.clip_filesize->setHidden(true);
        m_view.label_size->setHidden(true);
    }
    m_view.clip_duration->setInputMask(tc.mask());
    m_view.clip_duration->setText(tc.getTimecode(m_clip->duration()));
    if (t != IMAGE && t != COLOR && t != TEXT) m_view.clip_duration->setReadOnly(true);
    else {
        connect(m_view.clip_duration, SIGNAL(editingFinished()), this, SLOT(slotCheckMaxLength()));
        connect(m_view.clip_duration, SIGNAL(textChanged(QString)), this, SLOT(slotModified()));
    }

    // markers
    m_view.marker_new->setIcon(KIcon("document-new"));
    m_view.marker_new->setToolTip(i18n("Add marker"));
    m_view.marker_edit->setIcon(KIcon("document-properties"));
    m_view.marker_edit->setToolTip(i18n("Edit marker"));
    m_view.marker_delete->setIcon(KIcon("trash-empty"));
    m_view.marker_delete->setToolTip(i18n("Delete marker"));
    m_view.marker_save->setIcon(KIcon("document-save-as"));
    m_view.marker_save->setToolTip(i18n("Save markers"));
    m_view.marker_load->setIcon(KIcon("document-open"));
    m_view.marker_load->setToolTip(i18n("Load markers"));

        // Check for Nepomuk metadata
#ifdef USE_NEPOMUK
#if KDE_IS_VERSION(4,6,0)
    if (!url.isEmpty()) {
        Nepomuk::ResourceManager::instance()->init();
        Nepomuk::Resource res( url.path() );
        // Check if file has a license
        if (res.hasProperty(Nepomuk::Vocabulary::NIE::license())) {
            QString ltype = res.property(Nepomuk::Vocabulary::NIE::licenseType()).toString();
            m_view.clip_license->setText(i18n("License: %1", res.property(Nepomuk::Vocabulary::NIE::license()).toString()));
            if (ltype.startsWith("http")) {
                m_view.clip_license->setUrl(ltype);
                connect(m_view.clip_license, SIGNAL(leftClickedUrl(const QString &)), this, SLOT(slotOpenUrl(const QString &)));
            }
        }
        else m_view.clip_license->setHidden(true);
    }
    else m_view.clip_license->setHidden(true);
#else
    m_view.clip_license->setHidden(true);
#endif
#else
    m_view.clip_license->setHidden(true);
#endif
    
    slotFillMarkersList(m_clip);
    connect(m_view.marker_new, SIGNAL(clicked()), this, SLOT(slotAddMarker()));
    connect(m_view.marker_edit, SIGNAL(clicked()), this, SLOT(slotEditMarker()));
    connect(m_view.marker_delete, SIGNAL(clicked()), this, SLOT(slotDeleteMarker()));
    connect(m_view.marker_save, SIGNAL(clicked()), this, SLOT(slotSaveMarkers()));
    connect(m_view.marker_load, SIGNAL(clicked()), this, SLOT(slotLoadMarkers()));
    connect(m_view.markers_list, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(slotEditMarker()));

    connect(this, SIGNAL(accepted()), this, SLOT(slotApplyProperties()));
    connect(m_view.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotApplyProperties()));
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    adjustSize();
}


// Used for multiple clips editing
ClipProperties::ClipProperties(QList <DocClipBase *>cliplist, Timecode tc, QMap <QString, QString> commonproperties, QWidget * parent) :
    QDialog(parent),
    m_clip(NULL),
    m_tc(tc),
    m_fps(0),
    m_count(0),
    m_clipNeedsRefresh(false),
    m_clipNeedsReLoad(false)
{
    setFont(KGlobalSettings::toolBarFont());
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
        m_view.clip_progressive->setValue(commonproperties.value("force_progressive").toInt());
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
        } else m_view.clip_out->setText(KdenliveSettings::image_duration());
    } else {
        m_view.clip_force_out->setHidden(true);
        m_view.clip_out->setHidden(true);
    }
}

ClipProperties::~ClipProperties()
{
    QAbstractItemDelegate *del1 = m_view.clip_vproperties->itemDelegate();
    if (del1) delete del1;
    QAbstractItemDelegate *del2 = m_view.clip_aproperties->itemDelegate();
    if (del2) delete del2;
}

void ClipProperties::slotApplyProperties()
{
    if (m_clip != NULL) {
        QMap <QString, QString> props = properties();
        emit applyNewClipProperties(m_clip->getId(), m_clip->currentProperties(props), props, needsTimelineRefresh(), needsTimelineReload());
    }
    m_view.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void ClipProperties::disableClipId(const QString &id)
{
    if (m_clip && m_view.buttonBox->button(QDialogButtonBox::Ok)->isEnabled()) {
        if (m_clip->getId() == id) {
            // clip was removed from project, close this properties dialog
            close();
        }
    }
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

void ClipProperties::slotFillMarkersList(DocClipBase *clip)
{
    if (m_clip != clip) return;
    m_view.markers_list->clear();
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    for (int count = 0; count < marks.count(); ++count) {
        QString time = m_tc.getTimecode(marks[count].time());
        QStringList itemtext;
        itemtext << time << marks[count].comment();
        (void) new QTreeWidgetItem(m_view.markers_list, itemtext);
    }
}

void ClipProperties::slotAddMarker()
{
    CommentedTime marker(GenTime(), i18n("Marker"));
    QPointer<MarkerDialog> d = new MarkerDialog(m_clip, marker,
                                          m_tc, i18n("Add Marker"), this);
    if (d->exec() == QDialog::Accepted) {
        emit addMarker(m_clip->getId(), d->newMarker().time(), d->newMarker().comment());
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
    MarkerDialog d(m_clip, marks.at(pos), m_tc, i18n("Edit Marker"), this);
    if (d.exec() == QDialog::Accepted) {
        emit addMarker(m_clip->getId(), d.newMarker().time(), d.newMarker().comment());
    }
}

void ClipProperties::slotDeleteMarker()
{
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    int pos = m_view.markers_list->currentIndex().row();
    if (pos < 0 || pos > marks.count() - 1) return;
    emit addMarker(m_clip->getId(), marks.at(pos).time(), QString());
}

const QString &ClipProperties::clipId() const
{
    return m_clip->getId();
}


QMap <QString, QString> ClipProperties::properties()
{
    QMap <QString, QString> props;
    QLocale locale;
    CLIPTYPE t = UNKNOWN;
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

    int progressive = m_view.clip_progressive->value();
    if (m_view.clip_force_progressive->isChecked()) {
        if (progressive != m_old_props.value("force_progressive").toInt()) {
            props["force_progressive"] = QString::number(progressive);
        }
    } else if (m_old_props.contains("force_progressive") && !m_old_props.value("force_progressive").isEmpty()) {
        props["force_progressive"].clear();
    }

    int fieldOrder = m_view.clip_fieldorder->currentIndex();
    if (m_view.clip_force_fieldorder->isChecked()) {
        if (fieldOrder != m_old_props.value("force_tff").toInt()) {
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

    if (t == COLOR) {
        QString new_color = m_view.clip_color->color().name();
        if (new_color != QString('#' + m_old_props.value("colour").right(8).left(6))) {
            m_clipNeedsRefresh = true;
            props["colour"] = "0x" + new_color.right(6) + "ff";
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text());
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration - 1);
        }
    } else if (t == IMAGE) {
        if ((int) m_view.image_transparency->isChecked() != m_old_props.value("transparency").toInt()) {
            props["transparency"] = QString::number((int)m_view.image_transparency->isChecked());
            //m_clipNeedsRefresh = true;
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text());
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration - 1);
        }
    } else if (t == SLIDESHOW) {
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
                kDebug() << "////  SLIDE EDIT, NEW:" << new_path << ", OLD; " << m_old_props.value("resource");
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


void ClipProperties::parseFolder()
{
    QString path = m_view.clip_path->text();
    bool isMime = !(path.contains('%'));
    if (!isMime) path = KUrl(path).directory();
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
        // find pattern
        QString filter = KUrl(m_view.clip_path->text()).fileName();
        QString ext = filter.section('.', -1);
        filter = filter.section('%', 0, -2);
        QString regexp = '^' + filter + "\\d+\\." + ext + '$';
        QRegExp rx(regexp);
        QStringList entries;
        foreach(const QString & path, result) {
            if (rx.exactMatch(path)) entries << path;
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
    QDomElement xml = m_clip->toXML();
    xml.setAttribute("resource", m_view.clip_path->text() + extension);
    int width = 180.0 * KdenliveSettings::project_display_ratio();
    if (width % 2 == 1) width++;
    QString filePath = m_view.clip_path->text();
    if (isMime) filePath.append(extension);
    QPixmap pix = m_clip->thumbProducer()->getImage(KUrl(filePath), 1, width, 180);
    QMap <QString, QString> props = m_clip->properties();
    m_view.clip_duration->setText(m_tc.getTimecodeFromFrames(props.value("ttl").toInt() * m_count));
    m_view.clip_thumb->setPixmap(pix);
}

void ClipProperties::slotCheckMaxLength()
{
    if (m_clip->maxDuration() == GenTime()) return;
    int duration = m_tc.getFrameCount(m_view.clip_duration->text());
    if (duration > m_clip->maxDuration().frames(m_fps)) {
        m_view.clip_duration->setText(m_tc.getTimecode(m_clip->maxDuration()));
    }
}

void ClipProperties::slotUpdateDurationFormat(int ix)
{
    bool framesFormat = ix == 1;
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
      QString proxy = m_clip->getProperty("proxy");
      if (proxy.isEmpty()) return;
      emit deleteProxy(proxy);
      if (m_proxyContainer) delete m_proxyContainer;
}

void ClipProperties::slotOpenUrl(const QString &url)
{
    new KRun(KUrl(url), this);
}

#include "clipproperties.moc"



