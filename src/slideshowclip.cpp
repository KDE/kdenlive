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

#include <QDir>

#include <KStandardDirs>
#include <KDebug>
#include <KFileItem>

#include "kdenlivesettings.h"
#include "slideshowclip.h"

#define TYPE_JPEG 0
#define TYPE_PNG 1
#define TYPE_BMP 2
#define TYPE_GIF 3

SlideshowClip::SlideshowClip(QWidget * parent): QDialog(parent), m_count(0) {
    setFont(KGlobalSettings::toolBarFont());
    setWindowTitle(i18n("Add Slideshow Clip"));
    m_view.setupUi(this);
    m_view.clip_name->setText(i18n("Slideshow Clip"));
    m_view.folder_url->setMode(KFile::Directory);
    m_view.icon_list->setIconSize(QSize(50, 50));
    connect(m_view.folder_url, SIGNAL(textChanged(const QString &)), this, SLOT(parseFolder()));
    connect(m_view.image_type, SIGNAL(currentIndexChanged(int)), this, SLOT(parseFolder()));

    connect(m_view.slide_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLuma(int)));
    connect(m_view.luma_fade, SIGNAL(stateChanged(int)), this, SLOT(slotEnableLumaFile(int)));

    m_view.image_type->addItem("JPG");
    m_view.image_type->addItem("PNG");
    m_view.image_type->addItem("BMP");
    m_view.image_type->addItem("GIF");
    m_view.clip_duration->setText(KdenliveSettings::image_duration());
    m_view.luma_duration->setText("00:00:00:24");
    m_view.folder_url->setUrl(QDir::homePath());


    QString profilePath = KdenliveSettings::mltpath();
    profilePath = profilePath.section('/', 0, -3);
    profilePath += "/lumas/PAL/";

    QDir dir(profilePath);
    QStringList result = dir.entryList(QDir::Files);
    QStringList imagefiles;
    QStringList imagenamelist;
    foreach(QString file, result) {
        if (file.endsWith(".pgm")) {
            m_view.luma_file->addItem(KIcon(profilePath + file), file, profilePath + file);
        }
    }

    adjustSize();
}

void SlideshowClip::slotEnableLuma(int state) {
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_duration->setEnabled(enable);
    m_view.luma_fade->setEnabled(enable);
    if (enable) m_view.luma_file->setEnabled(m_view.luma_fade->isChecked());
    else m_view.luma_file->setEnabled(false);
}

void SlideshowClip::slotEnableLumaFile(int state) {
    bool enable = false;
    if (state == Qt::Checked) enable = true;
    m_view.luma_file->setEnabled(enable);
    m_view.luma_softness->setEnabled(enable);
    m_view.label_softness->setEnabled(enable);
}

void SlideshowClip::parseFolder() {
    m_view.icon_list->clear();
    QDir dir(m_view.folder_url->url().path());

    QStringList filters;
    switch (m_view.image_type->currentIndex()) {
    case TYPE_PNG:
        filters << "*.png";
        break;
    case TYPE_BMP:
        filters << "*.bmp";
        break;
    case TYPE_GIF:
        filters << "*.gif";
        break;
    default:
        filters << "*.jpg" << "*.jpeg";
        break;
    }

    dir.setNameFilters(filters);
    QStringList result = dir.entryList(QDir::Files);
    m_count = result.count();
    if (m_count == 0) m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    else m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    m_view.label_info->setText(i18n("%1 images found", m_count));
    foreach(const QString &path, result) {
        QIcon icon(dir.filePath(path));
        QListWidgetItem *item = new QListWidgetItem(icon, KUrl(path).fileName());
        m_view.icon_list->addItem(item);
    }
}

QString SlideshowClip::selectedPath() const {
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
    return m_view.folder_url->url().path() + extension;
}


QString SlideshowClip::clipName() const {
    return m_view.clip_name->text();
}

QString SlideshowClip::clipDuration() const {
    return m_view.clip_duration->text();
}

int SlideshowClip::imageCount() const {
    return m_count;
}

int SlideshowClip::softness() const {
    return m_view.luma_softness->value();
}

bool SlideshowClip::loop() const {
    return m_view.slide_loop->isChecked();
}

bool SlideshowClip::fade() const {
    return m_view.slide_fade->isChecked();
}

QString SlideshowClip::lumaDuration() const {
    return m_view.luma_duration->text();
}

QString SlideshowClip::lumaFile() const {
    if (!m_view.luma_fade->isChecked() || !m_view.luma_file->isEnabled()) return QString();
    return m_view.luma_file->itemData(m_view.luma_file->currentIndex()).toString();
}

#include "slideshowclip.moc"


