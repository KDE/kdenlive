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
#include <qradiobutton.h>
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
        clipChoice->edit_duration->setText(document->timeCode().getTimecode(refClip->duration(), KdenliveSettings::defaultfps()));
        m_document = document;

        //clipChoice->preview_pixmap->pixmap()->resize(120, 96);
        connect(clipChoice->button_color, SIGNAL(changed(const QColor &)), this, SLOT(updateColor(const QColor &)));

        clipChoice->edit_url->setURL(refClip->fileURL().path());
        DocClipBase *clip = refClip->referencedClip();
	m_clipType = refClip->clipType();

	DocClipAVFile * avclip = refClip->referencedClip()->toDocClipAVFile();

	// slideshow stuff
	if (m_clipType != DocClipBase::SLIDESHOW) {
            clipChoice->ttl_label->hide();
            clipChoice->crossfade->hide();
            clipChoice->image_ttl->hide();
            clipChoice->image_type_label->hide();
            clipChoice->imageType->hide();
	}
        if (m_clipType == DocClipBase::COLOR) {
            QString color = avclip->color();
            color = color.replace(0, 2, "#");
            color = color.left(7);
	    QPixmap pix(clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
	    pix.fill(color);
            clipChoice->preview_pixmap->setPixmap(pix);
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
        else if (m_clipType == DocClipBase::IMAGE) {
            clipChoice->transparent_bg->setChecked(avclip->isTransparent());
	    QPixmap pix = document->renderer()->getImageThumbnail(refClip->fileURL().path(), clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
	    clipChoice->preview_pixmap->setPixmap(pix);
            clipChoice->label_color->hide();
            clipChoice->button_color->hide();
            clipChoice->label_name->hide();
            clipChoice->edit_name->hide();
	    clipChoice->clipType->setText(i18n("Image Clip"));
            clipChoice->clipSize->setText(QString::number(refClip->clipWidth())+"x"+QString::number(refClip->clipHeight()));
	    clipChoice->clipFps->setText("-");
	    clipChoice->clipAudio->setText("-");
            clipChoice->clipFilesize->setText(refClip->formattedFileSize());
	}
        else if (m_clipType == DocClipBase::VIRTUAL) {
            clipChoice->label_color->hide();
            clipChoice->button_color->hide();
            clipChoice->label_name->hide();
            clipChoice->edit_name->hide();
	    clipChoice->clipType->setText(i18n("Virtual Clip"));
            clipChoice->clipSize->setText(QString::number(refClip->clipWidth())+"x"+QString::number(refClip->clipHeight()));
	    clipChoice->clipFps->setText("-");
	    clipChoice->clipAudio->setText("-");
            clipChoice->clipFilesize->setText("-");
	}
	else if (m_clipType == DocClipBase::SLIDESHOW) {
	    clipChoice->transparent_bg->setChecked(avclip->isTransparent());
	    clipChoice->crossfade->setChecked(avclip->hasCrossfade());
	    m_luma = avclip->lumaFile();
	    if (m_luma != QString::null) {
		clipChoice->luma_selected->setChecked(true);
		m_luma = KURL(m_luma).filename();
		m_luma.truncate(m_luma.length() - 4);
		clipChoice->luma_softness->setValue(avclip->lumaSoftness() * 100.0);
	    }
	    QPixmap pix = document->renderer()->getImageThumbnail(refClip->fileURL().path(), clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
	    clipChoice->preview_pixmap->setPixmap(pix);
            clipChoice->label_color->hide();
            clipChoice->button_color->hide();
            clipChoice->label_name->hide();
            clipChoice->edit_name->hide();
	    clipChoice->clipType->setText(i18n("Slideshow Clip"));
	    clipChoice->image_ttl->setText(document->timeCode().getTimecode(GenTime(avclip->clipTtl(), KdenliveSettings::defaultfps()), KdenliveSettings::defaultfps()));
	    clipChoice->transition_ttl->setText(document->timeCode().getTimecode(GenTime(avclip->lumaDuration(), KdenliveSettings::defaultfps()), KdenliveSettings::defaultfps()));

	    clipChoice->label_file->setText(i18n("Folder:"));
	    clipChoice->imageType->setCurrentItem(refClip->fileURL().filename().left(3));
	    clipChoice->edit_url->fileDialog()->setMode(KFile::Directory);
	    clipChoice->edit_url->setURL(refClip->fileURL().directory());
	    clipChoice->imageType->setCurrentText(refClip->fileURL().path().section(".", -1));
	    connect(clipChoice->imageType, SIGNAL(activated (int)), this, SLOT(updateList()));
	    connect(clipChoice->image_ttl, SIGNAL(textChanged (const QString &)), this, SLOT(updateDuration()));

	    clipChoice->edit_duration->setReadOnly(true);
	    updateList();
	    clipChoice->edit_url->setEnabled(false); // disable folder change for the moment
	
            clipChoice->clipFps->setText("-");
	    clipChoice->clipAudio->setText("-");
            clipChoice->clipFilesize->setText(refClip->formattedFileSize());
        }
        else {
		clipChoice->meta_text->setText(avclip->formattedMetaData());
            	clipChoice->transparent_bg->hide();
            	clipChoice->label_color->hide();
            	clipChoice->button_color->hide();
            	clipChoice->label_name->hide();
            	clipChoice->edit_name->hide();
		clipChoice->clipFps->setText(QString::number(refClip->framesPerSecond()));
		clipChoice->clipVCodec->setText(avclip->videoCodec());
		clipChoice->clipACodec->setText(avclip->audioCodec());
            	clipChoice->edit_duration->setReadOnly(true);
            	clipChoice->clipFilesize->setText(refClip->formattedFileSize());
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
			soundChannels = i18n("%1 Channels").arg(avclip->audioChannels());
			break;
		}
		clipChoice->clipAudio->setText(i18n("%1Hz %2").arg(avclip->audioFrequency()).arg(soundChannels));
	    }
	    else clipChoice->clipAudio->setText(i18n("None"));
	    if (m_clipType == DocClipBase::AUDIO) {
            	clipChoice->clipType->setText(i18n("Audio Clip"));
            }
            else { // Video clip
            	QPixmap pix = document->renderer()->getVideoThumbnail(refClip->fileURL().path(), clip->getProjectThumbFrame(), clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
            	if (!pix.isNull()) clipChoice->preview_pixmap->setPixmap(pix);
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
    }

    void ClipProperties::insertLuma(const QPixmap &pix, const QString &txt)
    {
	clipChoice->luma_list->insertItem(pix, txt);
    }

    void ClipProperties::preselectLuma()
    {
	clipChoice->luma_list->setCurrentText(m_luma);
    }

    void ClipProperties::updateDuration()
    {
        clipChoice->edit_duration->setText(m_document->timeCode().getTimecode(GenTime(m_imageCount * ttl(), KdenliveSettings::defaultfps()), KdenliveSettings::defaultfps()));
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

    void ClipProperties::updateThumb(const QString &path)
    {
	QPixmap pix;
	if (m_clipType == DocClipBase::VIDEO) 
	    pix = m_document->renderer()->getVideoThumbnail(path, 0, clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
	else if (m_clipType == DocClipBase::IMAGE || m_clipType == DocClipBase::SLIDESHOW)
	    pix = m_document->renderer()->getImageThumbnail(path, clipChoice->preview_pixmap->width(), clipChoice->preview_pixmap->height());
	else return;
        if (!pix.isNull()) clipChoice->preview_pixmap->setPixmap(pix);
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
        return  m_document->getTimecodePosition(clipChoice->image_ttl->text()).frames(KdenliveSettings::defaultfps());
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
        return m_document->getTimecodePosition(d);
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

    bool ClipProperties::crossfading()
    {
        return clipChoice->crossfade->isChecked(); 
    }

    QString ClipProperties::lumaFile() const
    {
        return clipChoice->luma_list->currentText();
    }

    double ClipProperties::lumaSoftness() const
    {
        return clipChoice->luma_softness->value() / 100.0;
    }

    int ClipProperties::lumaDuration() const
    {
	return  m_document->getTimecodePosition(clipChoice->transition_ttl->text()).frames(KdenliveSettings::defaultfps());
    }
    


} // namespace Gui


