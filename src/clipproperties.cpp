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

#include <KStandardDirs>
#include <KDebug>
#include <KFileItem>

#include <QDir>

static const int VIDEOTAB = 0;
static const int AUDIOTAB = 1;
static const int COLORTAB = 2;
static const int SLIDETAB = 3;
static const int IMAGETAB = 4;
static const int MARKERTAB = 5;
static const int METATAB = 6;
static const int ADVANCEDTAB = 7;

static const int TYPE_JPEG = 0;
static const int TYPE_PNG = 1;
static const int TYPE_BMP = 2;
static const int TYPE_GIF = 3;

ClipProperties::ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent) :
        QDialog(parent),
        m_clip(clip),
        m_tc(tc),
        m_fps(fps),
        m_count(0),
        m_clipNeedsRefresh(false)
{
    setFont(KGlobalSettings::toolBarFont());
    m_view.setupUi(this);
    KUrl url = m_clip->fileURL();
    m_view.clip_path->setText(url.path());
    m_view.clip_description->setText(m_clip->description());
    QMap <QString, QString> props = m_clip->properties();

    if (props.contains("force_aspect_ratio") && props.value("force_aspect_ratio").toDouble() > 0) {
        m_view.clip_force_ar->setChecked(true);
        m_view.clip_ar->setEnabled(true);
        m_view.clip_ar->setValue(props.value("force_aspect_ratio").toDouble());
    }

    if (props.contains("threads") && props.value("threads").toInt() != 1) {
        m_view.clip_force_threads->setChecked(true);
        m_view.clip_threads->setEnabled(true);
        m_view.clip_threads->setValue(props.value("threads").toInt());
    }

    if (props.contains("video_index") && props.value("video_index").toInt() != 0) {
        m_view.clip_force_vindex->setChecked(true);
        m_view.clip_vindex->setEnabled(true);
        m_view.clip_vindex->setValue(props.value("video_index").toInt());
    }

    if (props.contains("audio_index") && props.value("audio_index").toInt() != 0) {
        m_view.clip_force_aindex->setChecked(true);
        m_view.clip_aindex->setEnabled(true);
        m_view.clip_aindex->setValue(props.value("audio_index").toInt());
    }

    if (props.contains("audio_max")) {
        m_view.clip_aindex->setMaximum(props.value("audio_max").toInt());
    }

    if (props.contains("video_max")) {
        m_view.clip_vindex->setMaximum(props.value("video_max").toInt());
    }

    // Check for Metadata
    QMap<QString, QString> meta = m_clip->metadata();
    QMap<QString, QString>::const_iterator i = meta.constBegin();
    while (i != meta.constEnd()) {
        QTreeWidgetItem *metaitem = new QTreeWidgetItem(m_view.metadata_list);
        metaitem->setText(0, i.key()); //i18n(i.key().section('.', 2, 3).toUtf8().data()));
        metaitem->setText(1, i.value());
        ++i;
    }

    connect(m_view.clip_force_ar, SIGNAL(toggled(bool)), m_view.clip_ar, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_threads, SIGNAL(toggled(bool)), m_view.clip_threads, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_vindex, SIGNAL(toggled(bool)), m_view.clip_vindex, SLOT(setEnabled(bool)));
    connect(m_view.clip_force_aindex, SIGNAL(toggled(bool)), m_view.clip_aindex, SLOT(setEnabled(bool)));

    if (props.contains("audiocodec"))
        m_view.clip_acodec->setText(props.value("audiocodec"));
    if (props.contains("frequency"))
        m_view.clip_frequency->setText(props.value("frequency"));
    if (props.contains("channels"))
        m_view.clip_channels->setText(props.value("channels"));

    CLIPTYPE t = m_clip->clipType();
    if (t != AUDIO && t != AV) {
        m_view.clip_force_aindex->setEnabled(false);
    }

    if (t != VIDEO && t != AV) {
        m_view.clip_force_vindex->setEnabled(false);
    }

    if (t == IMAGE) {
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        if (props.contains("frame_size"))
            m_view.image_size->setText(props.value("frame_size"));
        if (props.contains("transparency"))
            m_view.image_transparency->setChecked(props.value("transparency").toInt());
    } else if (t == COLOR) {
        m_view.clip_path->setEnabled(false);
        m_view.tabWidget->removeTab(METATAB);
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
        m_view.clip_color->setColor(QColor('#' + props.value("colour").right(8).left(6)));
    } else if (t == SLIDESHOW) {
        m_view.clip_path->setText(url.directory());
        m_view.tabWidget->removeTab(METATAB);
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(AUDIOTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        QStringList types;
        types << "JPG" << "PNG" << "BMP" << "GIF";
        m_view.image_type->addItems(types);
        m_view.slide_loop->setChecked(props.value("loop").toInt());
        m_view.slide_fade->setChecked(props.value("fade").toInt());
        m_view.luma_softness->setValue(props.value("softness").toInt());
        QString path = props.value("resource");
        if (path.endsWith("png")) m_view.image_type->setCurrentIndex(TYPE_PNG);
        else if (path.endsWith("bmp")) m_view.image_type->setCurrentIndex(TYPE_BMP);
        else if (path.endsWith("gif")) m_view.image_type->setCurrentIndex(TYPE_GIF);
        m_view.slide_duration->setText(tc.getTimecodeFromFrames(props.value("ttl").toInt()));

        m_view.slide_duration_format->addItem(i18n("hh:mm:ss::ff"));
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
        foreach(const QString &folder, customLumas) {
            QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
            foreach(const QString &fname, filesnames) {
                m_view.luma_file->addItem(KIcon(folder + '/' + fname), fname, folder + '/' + fname);
            }
        }

        // Check for MLT lumas
        QString profilePath = KdenliveSettings::mltpath();
        QString folder = profilePath.section('/', 0, -3);
        folder.append("/lumas/PAL"); // TODO: cleanup the PAL / NTSC mess in luma files
        QDir lumafolder(folder);
        QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
        foreach(const QString &fname, filesnames) {
            m_view.luma_file->addItem(KIcon(folder + '/' + fname), fname, folder + '/' + fname);
        }

        slotEnableLuma(m_view.slide_fade->checkState());
        slotEnableLumaFile(m_view.slide_luma->checkState());

        if (!lumaFile.isEmpty()) {
            m_view.slide_luma->setChecked(true);
            m_view.luma_file->setCurrentIndex(m_view.luma_file->findData(lumaFile));
        } else m_view.luma_file->setEnabled(false);
        connect(m_view.slide_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLuma(int)));
        connect(m_view.slide_luma, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLumaFile(int)));

        connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));
    } else if (t != AUDIO) {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        if (props.contains("frame_size"))
            m_view.clip_size->setText(props.value("frame_size"));
        if (props.contains("videocodec"))
            m_view.clip_vcodec->setText(props.value("videocodec"));
        if (props.contains("fps"))
            m_view.clip_fps->setText(props.value("fps"));
        if (props.contains("aspect_ratio"))
            m_view.clip_ratio->setText(props.value("aspect_ratio"));

        QPixmap pix = m_clip->thumbProducer()->getImage(url, m_clip->getClipThumbFrame(), 240, 180);
        m_view.clip_thumb->setPixmap(pix);
        if (t == IMAGE || t == VIDEO) m_view.tabWidget->removeTab(AUDIOTAB);
    } else {
        m_view.tabWidget->removeTab(IMAGETAB);
        m_view.tabWidget->removeTab(SLIDETAB);
        m_view.tabWidget->removeTab(COLORTAB);
        m_view.tabWidget->removeTab(VIDEOTAB);
        m_view.clip_thumb->setHidden(true);
    }

    KFileItem f(KFileItem::Unknown, KFileItem::Unknown, url, true);
    m_view.clip_filesize->setText(KIO::convertSize(f.size()));
    m_view.clip_duration->setText(tc.getTimecode(m_clip->duration(), m_fps));
    if (t != IMAGE && t != COLOR && t != TEXT) m_view.clip_duration->setReadOnly(true);
    else connect(m_view.clip_duration, SIGNAL(editingFinished()), this, SLOT(slotCheckMaxLength()));

    // markers
    m_view.marker_new->setIcon(KIcon("document-new"));
    m_view.marker_new->setToolTip(i18n("Add marker"));
    m_view.marker_edit->setIcon(KIcon("document-properties"));
    m_view.marker_edit->setToolTip(i18n("Edit marker"));
    m_view.marker_delete->setIcon(KIcon("trash-empty"));
    m_view.marker_delete->setToolTip(i18n("Delete marker"));

    slotFillMarkersList();
    connect(m_view.marker_new, SIGNAL(clicked()), this, SLOT(slotAddMarker()));
    connect(m_view.marker_edit, SIGNAL(clicked()), this, SLOT(slotEditMarker()));
    connect(m_view.marker_delete, SIGNAL(clicked()), this, SLOT(slotDeleteMarker()));
    connect(m_view.markers_list, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(slotEditMarker()));

    adjustSize();
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

void ClipProperties::slotFillMarkersList()
{
    m_view.markers_list->clear();
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    for (int count = 0; count < marks.count(); ++count) {
        QString time = m_tc.getTimecode(marks[count].time(), m_tc.fps());
        QStringList itemtext;
        itemtext << time << marks[count].comment();
        (void) new QTreeWidgetItem(m_view.markers_list, itemtext);
    }
}

void ClipProperties::slotAddMarker()
{
    CommentedTime marker(GenTime(), i18n("Marker"));
    MarkerDialog d(m_clip, marker, m_tc, i18n("Add Marker"), this);
    if (d.exec() == QDialog::Accepted) {
        emit addMarker(m_clip->getId(), d.newMarker().time(), d.newMarker().comment());
    }
    QTimer::singleShot(500, this, SLOT(slotFillMarkersList()));
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
    QTimer::singleShot(500, this, SLOT(slotFillMarkersList()));
}

void ClipProperties::slotDeleteMarker()
{
    QList < CommentedTime > marks = m_clip->commentedSnapMarkers();
    int pos = m_view.markers_list->currentIndex().row();
    if (pos < 0 || pos > marks.count() - 1) return;
    emit addMarker(m_clip->getId(), marks.at(pos).time(), QString());

    QTimer::singleShot(500, this, SLOT(slotFillMarkersList()));
}

const QString &ClipProperties::clipId() const
{
    return m_clip->getId();
}


QMap <QString, QString> ClipProperties::properties()
{
    QMap <QString, QString> props;
    CLIPTYPE t = m_clip->clipType();
    QMap <QString, QString> old_props = m_clip->properties();

    if (old_props.value("description") != m_view.clip_description->text())
        props["description"] = m_view.clip_description->text();

    double aspect = m_view.clip_ar->value();
    if (m_view.clip_force_ar->isChecked()) {
        if (aspect != old_props.value("force_aspect_ratio").toDouble()) {
            props["force_aspect_ratio"] = QString::number(aspect);
            m_clipNeedsRefresh = true;
        }
    } else if (old_props.contains("force_aspect_ratio")) {
        props["force_aspect_ratio"].clear();
        m_clipNeedsRefresh = true;
    }

    int threads = m_view.clip_threads->value();
    if (m_view.clip_force_threads->isChecked()) {
        if (threads != old_props.value("threads").toInt()) {
            props["threads"] = QString::number(threads);
        }
    } else if (old_props.contains("threads")) {
        props["threads"].clear();
    }

    int vindex = m_view.clip_vindex->value();
    if (m_view.clip_force_vindex->isChecked()) {
        if (vindex != old_props.value("video_index").toInt()) {
            props["video_index"] = QString::number(vindex);
        }
    } else if (old_props.contains("video_index")) {
        props["video_index"].clear();
    }

    int aindex = m_view.clip_aindex->value();
    if (m_view.clip_force_aindex->isChecked()) {
        if (aindex != old_props.value("audio_index").toInt()) {
            props["audio_index"] = QString::number(aindex);
        }
    } else if (old_props.contains("audio_index")) {
        props["audio_index"].clear();
    }

    if (t == COLOR) {
        QString new_color = m_view.clip_color->color().name();
        if (new_color != QString('#' + old_props.value("colour").right(8).left(6))) {
            m_clipNeedsRefresh = true;
            props["colour"] = "0x" + new_color.right(6) + "ff";
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text(), m_fps);
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration);
        }
    } else if (t == IMAGE) {
        if ((int) m_view.image_transparency->isChecked() != old_props.value("transparency").toInt()) {
            props["transparency"] = QString::number((int)m_view.image_transparency->isChecked());
            m_clipNeedsRefresh = true;
        }
        int duration = m_tc.getFrameCount(m_view.clip_duration->text(), m_fps);
        if (duration != m_clip->duration().frames(m_fps)) {
            props["out"] = QString::number(duration);
        }
    } else if (t == SLIDESHOW) {
        QString value = QString::number((int) m_view.slide_loop->isChecked());
        if (old_props.value("loop") != value) props["loop"] = value;
        value = QString::number((int) m_view.slide_fade->isChecked());
        if (old_props.value("fade") != value) props["fade"] = value;
        value = QString::number((int) m_view.luma_softness->value());
        if (old_props.value("softness") != value) props["softness"] = value;

        QString extension;
        switch (m_view.image_type->currentIndex()) {
        case TYPE_PNG:
            extension = "/.all.png";
            break;
        case TYPE_BMP:
            extension = "/.all.bmp";
            break;
        case TYPE_GIF:
            extension = "/.all.gif";
            break;
        default:
            extension = "/.all.jpg";
            break;
        }
        QString new_path = m_view.clip_path->text() + extension;
        if (new_path != old_props.value("resource")) {
            m_clipNeedsRefresh = true;
            props["resource"] = new_path;
            kDebug() << "////  SLIDE EDIT, NEW:" << new_path << ", OLD; " << old_props.value("resource");
        }
        int duration;
        if (m_view.slide_duration_format->currentIndex() == 1) {
            // we are in frames mode
            duration = m_view.slide_duration_frames->value();
        } else duration = m_tc.getFrameCount(m_view.slide_duration->text(), m_fps);
        if (duration != old_props.value("ttl").toInt()) {
            m_clipNeedsRefresh = true;
            props["ttl"] = QString::number(duration);
            props["out"] = QString::number(duration * m_count);
        }
        if (duration * m_count != old_props.value("out").toInt()) {
            m_clipNeedsRefresh = true;
            props["out"] = QString::number(duration * m_count);
        }
        if (m_view.slide_fade->isChecked()) {
            int luma_duration;
            if (m_view.slide_duration_format->currentIndex() == 1) {
                // we are in frames mode
                luma_duration = m_view.luma_duration_frames->value();
            } else luma_duration = m_tc.getFrameCount(m_view.luma_duration->text(), m_fps);
            if (luma_duration != old_props.value("luma_duration").toInt()) {
                m_clipNeedsRefresh = true;
                props["luma_duration"] = QString::number(luma_duration);
            }
            QString lumaFile;
            if (m_view.slide_luma->isChecked())
                lumaFile = m_view.luma_file->itemData(m_view.luma_file->currentIndex()).toString();
            if (lumaFile != old_props.value("luma_file")) {
                m_clipNeedsRefresh = true;
                props["luma_file"] = lumaFile;
            }
        } else {
            if (!old_props.value("luma_file").isEmpty()) {
                props["luma_file"].clear();
            }
        }

    }
    return props;
}

bool ClipProperties::needsTimelineRefresh() const
{
    return m_clipNeedsRefresh;
}

void ClipProperties::parseFolder()
{

    QDir dir(m_view.clip_path->text());
    QStringList filters;
    QString extension;
    switch (m_view.image_type->currentIndex()) {
    case TYPE_PNG:
        filters << "*.png";
        extension = "/.all.png";
        break;
    case TYPE_BMP:
        filters << "*.bmp";
        extension = "/.all.bmp";
        break;
    case TYPE_GIF:
        filters << "*.gif";
        extension = "/.all.gif";
        break;
    default:
        filters << "*.jpg";
        // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
        // << "*.jpeg";
        extension = "/.all.jpg";
        break;
    }

    dir.setNameFilters(filters);
    QStringList result = dir.entryList(QDir::Files);
    m_count = result.count();
    m_view.slide_info->setText(i18n("%1 images found", m_count));
    QDomElement xml = m_clip->toXML();
    xml.setAttribute("resource", m_view.clip_path->text() + extension);
    QPixmap pix = m_clip->thumbProducer()->getImage(KUrl(m_view.clip_path->text() + extension), 1, 240, 180);
    QMap <QString, QString> props = m_clip->properties();
    m_view.clip_duration->setText(m_tc.getTimecodeFromFrames(props.value("ttl").toInt() * m_count));
    m_view.clip_thumb->setPixmap(pix);
}

void ClipProperties::slotCheckMaxLength()
{
    int duration = m_tc.getFrameCount(m_view.clip_duration->text(), m_fps);
    if (duration > m_clip->maxDuration().frames(m_fps)) {
        m_view.clip_duration->setText(m_tc.getTimecode(m_clip->maxDuration(), m_fps));
    }
}

void ClipProperties::slotUpdateDurationFormat(int ix)
{
    bool framesFormat = ix == 1;
    if (framesFormat) {
        // switching to frames count, update widget
        m_view.slide_duration_frames->setValue(m_tc.getFrameCount(m_view.slide_duration->text(), m_tc.fps()));
        m_view.luma_duration_frames->setValue(m_tc.getFrameCount(m_view.luma_duration->text(), m_tc.fps()));
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

#include "clipproperties.moc"


