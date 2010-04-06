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

#include "slideshowclip.h"
#include "kdenlivesettings.h"

#include <KStandardDirs>
#include <KDebug>
#include <KFileItem>

#include <QDir>
#include <QtConcurrentRun>
#include <QFutureWatcher>


SlideshowClip::SlideshowClip(Timecode tc, QWidget * parent) :
        QDialog(parent),
        m_count(0),
        m_timecode(tc)
{
    setFont(KGlobalSettings::toolBarFont());
    setWindowTitle(i18n("Add Slideshow Clip"));
    m_view.setupUi(this);
    m_view.clip_name->setText(i18n("Slideshow Clip"));
    m_view.folder_url->setMode(KFile::Directory);
    m_view.icon_list->setIconSize(QSize(50, 50));
    m_view.show_thumbs->setChecked(KdenliveSettings::showslideshowthumbs());

    connect(m_view.folder_url, SIGNAL(textChanged(const QString &)), this, SLOT(parseFolder()));
    connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));

    connect(m_view.show_thumbs, SIGNAL(stateChanged(int)), this, SLOT(slotEnableThumbs(int)));
    connect(m_view.slide_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLuma(int)));
    connect(m_view.luma_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLumaFile(int)));

    //WARNING: keep in sync with clipproperties.cpp
    m_view.image_type->addItem("JPG (*.jpg)", "jpg");
    m_view.image_type->addItem("JPEG (*.jpeg)", "jpeg");
    m_view.image_type->addItem("PNG (*.png)", "png");
    m_view.image_type->addItem("BMP (*.bmp)", "bmp");
    m_view.image_type->addItem("GIF (*.gif)", "gif");
    m_view.image_type->addItem("TGA (*.tga)", "tga");
    m_view.image_type->addItem("TIFF (*.tiff)", "tiff");
    m_view.image_type->addItem("Open EXR (*.exr)", "exr");

    m_view.clip_duration->setInputMask(m_timecode.inputMask());
    m_view.clip_duration->setText(m_timecode.reformatSeparators(KdenliveSettings::image_duration()));
    m_view.luma_duration->setInputMask(m_timecode.inputMask());
    m_view.luma_duration->setText(m_timecode.getTimecodeFromFrames(int(ceil(m_timecode.fps()))));
    m_view.folder_url->setUrl(QDir::homePath());

    m_view.clip_duration_format->addItem(i18n("hh:mm:ss::ff"));
    m_view.clip_duration_format->addItem(i18n("Frames"));
    connect(m_view.clip_duration_format, SIGNAL(activated(int)), this, SLOT(slotUpdateDurationFormat(int)));
    m_view.clip_duration_frames->setHidden(true);
    m_view.luma_duration_frames->setHidden(true);

    // Check for Kdenlive installed luma files
    QStringList filters;
    filters << "*.pgm" << "*.png";

    QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
    foreach(const QString &folder, customLumas) {
        QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
        foreach(const QString &fname, filesnames) {
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
    foreach(const QString &fname, filesnames) {
        QString filePath = KUrl(folder).path(KUrl::AddTrailingSlash) + fname;
        m_view.luma_file->addItem(KIcon(filePath), fname, filePath);
    }

    //adjustSize();
}

void SlideshowClip::slotEnableLuma(int state)
{
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_duration->setEnabled(enable);
    m_view.luma_duration_frames->setEnabled(enable);
    m_view.luma_fade->setEnabled(enable);
    if (enable) {
        m_view.luma_file->setEnabled(m_view.luma_fade->isChecked());
    } else m_view.luma_file->setEnabled(false);
    m_view.label_softness->setEnabled(m_view.luma_fade->isChecked() && enable);
    m_view.luma_softness->setEnabled(m_view.label_softness->isEnabled());
}

void SlideshowClip::slotEnableThumbs(int state)
{
    if (state == Qt::Checked) {
        KdenliveSettings::setShowslideshowthumbs(true);
        slotGenerateThumbs();
    } else {
        KdenliveSettings::setShowslideshowthumbs(false);
    }

}

void SlideshowClip::slotEnableLumaFile(int state)
{
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_file->setEnabled(enable);
    m_view.luma_softness->setEnabled(enable);
    m_view.label_softness->setEnabled(enable);
}

void SlideshowClip::parseFolder()
{
    m_view.icon_list->clear();
    QDir dir(m_view.folder_url->url().path());

    QStringList filters;
    QString filter = m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
    filters << "*." + filter;
    // TODO: improve jpeg image detection with extension like jpeg, requires change in MLT image producers
    // << "*.jpeg";

    dir.setNameFilters(filters);
    const QStringList result = dir.entryList(QDir::Files);
    m_count = result.count();
    if (m_count == 0) m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    else m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    m_view.label_info->setText(i18np("1 image found", "%1 images found", m_count));
    QListWidgetItem *item;
    int i = 0;
    KIcon unknownicon("unknown");
    foreach(const QString &path, result) {
        i++;
        item = new QListWidgetItem(unknownicon, KUrl(path).fileName());
        item->setData(Qt::UserRole, dir.filePath(path));
        m_view.icon_list->addItem(item);
    }
    if (m_view.show_thumbs->isChecked()) slotGenerateThumbs();
    m_view.icon_list->setCurrentRow(0);
}

void SlideshowClip::slotGenerateThumbs()
{
    if (!m_future.isRunning()) {
        connect(&m_watcher, SIGNAL(finished()), this, SLOT(slotCheckGenerateThumbs()));
        m_future = QtConcurrent::run(this, &SlideshowClip::doGetThumbs);
        m_watcher.setFuture(m_future);
    }
}

void SlideshowClip::slotCheckGenerateThumbs()
{
    QListWidgetItem* item = m_view.icon_list->item(m_view.icon_list->count() - 1);
    if (!item || item->data(Qt::UserRole).toString().isEmpty() || m_view.show_thumbs->isChecked() == false) return;
    QTimer::singleShot(300, this, SLOT(slotGenerateThumbs()));
}

void SlideshowClip::doGetThumbs()
{
    for (int i = 0; i < m_view.icon_list->count(); i++) {
        QListWidgetItem* item = m_view.icon_list->item(i);
        if (item && m_view.show_thumbs->isChecked()) {
            QString path = item->data(Qt::UserRole).toString();
            if (path.isEmpty()) continue;
            else {
                item->setIcon(KIcon(path));
                item->setData(Qt::UserRole, QString());
                break;
            }
        }
    }
}

QString SlideshowClip::selectedPath() const
{
    QString extension = "/.all." + m_view.image_type->itemData(m_view.image_type->currentIndex()).toString();
    return m_view.folder_url->url().path() + extension;
}


QString SlideshowClip::clipName() const
{
    return m_view.clip_name->text();
}

QString SlideshowClip::clipDuration() const
{
    if (m_view.clip_duration_format->currentIndex() == 1) {
        // we are in frames mode
        return m_timecode.getTimecodeFromFrames(m_view.clip_duration_frames->value());
    }
    return m_view.clip_duration->text();
}

int SlideshowClip::imageCount() const
{
    return m_count;
}

int SlideshowClip::softness() const
{
    return m_view.luma_softness->value();
}

bool SlideshowClip::loop() const
{
    return m_view.slide_loop->isChecked();
}

bool SlideshowClip::fade() const
{
    return m_view.slide_fade->isChecked();
}

QString SlideshowClip::lumaDuration() const
{
    if (m_view.clip_duration_format->currentIndex() == 1) {
        // we are in frames mode
        return m_timecode.getTimecodeFromFrames(m_view.luma_duration_frames->value());
    }
    return m_view.luma_duration->text();
}

QString SlideshowClip::lumaFile() const
{
    if (!m_view.luma_fade->isChecked() || !m_view.luma_file->isEnabled()) return QString();
    return m_view.luma_file->itemData(m_view.luma_file->currentIndex()).toString();
}

void SlideshowClip::slotUpdateDurationFormat(int ix)
{
    bool framesFormat = ix == 1;
    if (framesFormat) {
        // switching to frames count, update widget
        m_view.clip_duration_frames->setValue(m_timecode.getFrameCount(m_view.clip_duration->text()));
        m_view.luma_duration_frames->setValue(m_timecode.getFrameCount(m_view.luma_duration->text()));
    } else {
        // switching to timecode format
        m_view.clip_duration->setInputMask(m_timecode.inputMask());
        m_view.clip_duration->setText(m_timecode.getTimecodeFromFrames(m_view.clip_duration_frames->value()));
        m_view.luma_duration->setInputMask(m_timecode.inputMask());
        m_view.luma_duration->setText(m_timecode.getTimecodeFromFrames(m_view.luma_duration_frames->value()));
    }
    m_view.clip_duration_frames->setHidden(!framesFormat);
    m_view.clip_duration->setHidden(framesFormat);
    m_view.luma_duration_frames->setHidden(!framesFormat);
    m_view.luma_duration->setHidden(framesFormat);
}

#include "slideshowclip.moc"


