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
    //wsetCaption(i18n("Add Slideshow Clip"));
    m_view.setupUi(this);
    m_view.clip_name->setText(i18n("Slideshow Clip"));
    m_view.folder_url->setMode(KFile::Directory);
    m_view.icon_list->setIconSize(QSize(50, 50));
    connect(m_view.folder_url, SIGNAL(textChanged(const QString &)), this, SLOT(parseFolder()));
    connect(m_view.image_type, SIGNAL(currentIndexChanged ( int )), this, SLOT(parseFolder()));
    m_view.image_type->addItem("JPG");
    m_view.image_type->addItem("PNG");
    m_view.image_type->addItem("BMP");
    m_view.image_type->addItem("GIF");
    m_view.clip_duration->setText("00:00:03:00");
    adjustSize();
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
    m_view.label_info->setText(i18n("%1 images found", m_count));
    foreach (QString path, result) {
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

bool SlideshowClip::loop() const {
    return m_view.slide_loop->isChecked();
}
#include "slideshowclip.moc"


