/***************************************************************************
                          ClipProperties.cpp  -  description
                             -------------------
    begin                :  Mar 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <cmath>

#include <qnamespace.h>
#include <qhgroupbox.h>
#include <qhbox.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qslider.h>
#include <qcheckbox.h>
#include <qtextedit.h>
#include <qcolor.h>
#include <qtoolbutton.h>
#include <qspinbox.h>

#include <kpushbutton.h>
#include <kurlrequester.h>
#include <klocale.h>
#include <kcolorbutton.h>
#include <kfiledialog.h>
#include <kcombobox.h>
#include <kdebug.h>

#include "transition.h"
#include "docclipref.h"
#include "docclipavfile.h"
#include "clipproperties.h"


namespace Gui {

    ClipProperties::ClipProperties(DocClipRef *refClip, KdenliveDoc * document, QWidget * parent, const char *name):  KDialogBase (KDialogBase::Swallow, 0, parent,name, true, i18n("Clip Properties"), KDialogBase::Ok | KDialogBase::Cancel), m_imageCount(0) {

        clipChoice = new clipProperties_UI(this);
        clipChoice->edit_name->setText(refClip->name());
        clipChoice->edit_description->setText(refClip->description());
        Timecode tcode;
        clipChoice->edit_duration->setText(tcode.getTimecode(refClip->duration(), KdenliveSettings::defaultfps()));
        m_pix = new QPixmap(120,96);
        m_document = document;

        clipChoice->preview_pixmap->pixmap()->resize(120, 96);
        connect(clipChoice->button_color, SIGNAL(changed(const QColor &)), this, SLOT(updateColor(const QColor &)));

	// slideshow stuff
	clipChoice->ttl_label->hide();
        clipChoice->image_ttl->hide();
	clipChoice->image_type_label->hide();
        clipChoice->imageType->hide();

        clipChoice->edit_url->setURL(refClip->fileURL().path());
        DocClipBase *clip = refClip->referencedClip();

        if (refClip->clipType() == DocClipBase::COLOR) {
            QString color = dynamic_cast < DocClipAVFile * >(clip)->color();
            color = color.replace(0, 2, "#");
            color = color.left(7);
            clipChoice->preview_pixmap->pixmap()->fill(color);
            clipChoice->button_color->setColor(color);
            clipChoice->transparent_bg->hide();
            clipChoice->label_file->hide();
            clipChoice->edit_url->hide();
            clipChoice->clipType->setText(i18n("Color Clip"));
            clipChoice->clipSize->setText("-");
            clipChoice->clipFps->setText("-");
	    clipChoice->clipAudio->setText("-");
            clipChoice->clipFilesize->setText("-");
        }
        else if (refClip->clipType() == DocClipBase::IMAGE) {
            document->renderer()->getImage(refClip->fileURL().path(), 0, m_pix);
            clipChoice->preview_pixmap->setPixmap(*m_pix);
            clipChoice->transparent_bg->setChecked(clip->toDocClipAVFile()->isTransparent());
            clipChoice->label_color->hide();
            clipChoice->button_color->hide();
            clipChoice->label_name->hide();
            clipChoice->edit_name->hide();
	    if ((refClip->referencedClip()->toDocClipAVFile()->clipTtl()==0)) {
            	clipChoice->clipType->setText(i18n("Image Clip"));
            	clipChoice->clipSize->setText(QString::number(refClip->clipWidth())+"x"+QString::number(refClip->clipHeight()));
	    }
	    else { // this is a slideshow clip
		clipChoice->clipType->setText(i18n("Slideshow Clip"));
	    	clipChoice->ttl_label->show();
	    	clipChoice->image_ttl->show();
		clipChoice->image_ttl->setValue(refClip->referencedClip()->toDocClipAVFile()->clipTtl());
	    	clipChoice->image_type_label->show();
	    	clipChoice->imageType->show();
		clipChoice->label_file->setText(i18n("Folder:"));
		clipChoice->imageType->setCurrentItem(refClip->fileURL().filename().left(3));
		clipChoice->edit_url->fileDialog()->setMode(KFile::Directory);
		clipChoice->edit_url->setURL(refClip->fileURL().directory());
		connect(clipChoice->imageType, SIGNAL(activated (int)), this, SLOT(updateList()));
		connect(clipChoice->image_ttl, SIGNAL(valueChanged (int)), this, SLOT(updateDuration()));

		clipChoice->edit_duration->setReadOnly(true);
		updateList();
		clipChoice->edit_url->setEnabled(false); // disable folder change for the moment
	    }
            clipChoice->clipFps->setText("-");
	    clipChoice->clipAudio->setText("-");
            clipChoice->clipFilesize->setText(formattedSize(refClip->fileSize()));
        }
        else {
            	clipChoice->transparent_bg->hide();
            	clipChoice->label_color->hide();
            	clipChoice->button_color->hide();
            	clipChoice->label_name->hide();
            	clipChoice->edit_name->hide();
		clipChoice->clipFps->setText(QString::number(refClip->framesPerSecond()));
            	clipChoice->edit_duration->setReadOnly(true);
            	clipChoice->clipFilesize->setText(formattedSize(refClip->fileSize()));
	    if (refClip->clipType() != DocClipBase::VIDEO) { // Clip is not a mute video
		QString soundChannels;
		switch (clip->toDocClipAVFile()->audioChannels()) {
	    	    case 1:
			soundChannels = i18n("Mono");
			break;
	    	    case 2:
			soundChannels = i18n("Stereo");
			break;
	    	    default:
			soundChannels = i18n("%1 Channels").arg(clip->toDocClipAVFile()->audioChannels());
			break;
		}
		clipChoice->clipAudio->setText(i18n("%1Hz %2").arg(clip->toDocClipAVFile()->audioFrequency()).arg(soundChannels));
	    }
	    else clipChoice->clipAudio->setText(i18n("None"));
	    if (refClip->clipType() == DocClipBase::AUDIO) {
            	clipChoice->clipType->setText(i18n("Audio Clip"));
            }
            else { // Video clip
            	document->renderer()->getImage(refClip->fileURL().path(), 0, m_pix);
            	clipChoice->preview_pixmap->setPixmap(*m_pix);
            	clipChoice->clipType->setText(i18n("Video Clip"));
            	clipChoice->clipSize->setText(QString::number(refClip->clipWidth())+"x"+QString::number(refClip->clipHeight()));
            }
        }

	connect(clipChoice->edit_url, SIGNAL(textChanged(const QString &)), this, SLOT(updateThumb(const QString &)));
        setMainWidget(clipChoice);
        clipChoice->show();
    }

    ClipProperties::~ClipProperties() 
    {
        delete m_pix;
    }

    void ClipProperties::updateDuration()
    {
	Timecode tcode;
        clipChoice->edit_duration->setText(tcode.getTimecode(GenTime(m_imageCount * ttl(), KdenliveSettings::defaultfps()), KdenliveSettings::defaultfps()));
    }

    void ClipProperties::updateList()
    {
	QStringList more;
    	QStringList::Iterator it;

        QDir dir( url() );
        more = dir.entryList( QDir::Files );
	m_imageCount = 0;

        for ( it = more.begin() ; it != more.end() ; ++it )
            if ((*it).endsWith("." + clipChoice->imageType->currentText(), FALSE)) {
		m_imageCount++;
	    }

	updateDuration();

	clipChoice->label_duration->setText(i18n("Duration (%1 images)").arg(QString::number(m_imageCount)));

	if (m_imageCount == 0) enableButtonOK(false);
	else enableButtonOK(true);
    }

    QString ClipProperties::formattedSize(uint fileSize)
    {
        long tenth;
        QString text;
        if (fileSize < 1024) {
            text = QString::number(fileSize) + i18n(" byte(s)");
        } else {
            fileSize = (int) floor((fileSize / 1024.0) + 0.5);

            if (fileSize < 1024) {
                text = QString::number(fileSize) + i18n(" Kb");
            } else {
                fileSize = (int) floor((fileSize / 102.4) + 0.5);

                tenth = fileSize % 10;
                fileSize /= 10;

                text = QString::number(fileSize) + "." +
                        QString::number(tenth) + i18n(" Mb");
            }
        }
        return text;
    }
    
    void ClipProperties::updateThumb(const QString &path)
    {
        m_document->renderer()->getImage(path, 0, m_pix);
        clipChoice->preview_pixmap->setPixmap(*m_pix);
    }

    QString ClipProperties::color()
    {
        QString color = clipChoice->button_color->color().name();
        color = color.replace(0, 1, "0x") + "ff";
        return color;
    }
    
    QString ClipProperties::name()
    {
        return clipChoice->edit_name->text();
    }

    int ClipProperties::ttl()
    {
        return clipChoice->image_ttl->value();
    }

    QString ClipProperties::extension()
    {
        return clipChoice->imageType->currentText();
    }
    
    QString ClipProperties::description()
    {
        return clipChoice->edit_description->text();
    }
    
    GenTime ClipProperties::duration()
    {
        QString d = clipChoice->edit_duration->text();
        int frames = (d.section(":",0,0).toInt()*3600 + d.section(":",1,1).toInt()*60 + d.section(":",2,2).toInt()) * KdenliveSettings::defaultfps() + d.section(":",3,3).toInt();
        return GenTime(frames , KdenliveSettings::defaultfps());
    }
    
    void ClipProperties::updateColor(const QColor &c)
    {
        clipChoice->preview_pixmap->pixmap()->fill(c);
        clipChoice->preview_pixmap->repaint();
    }
    
    QString ClipProperties::url()
    {
        return clipChoice->edit_url->url(); 
    }
    
    bool ClipProperties::transparency()
    {
        return clipChoice->transparent_bg->isChecked(); 
    }
    


} // namespace Gui


