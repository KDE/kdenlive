/***************************************************************************
                          clippropertiesdialog.cpp  -  description
                             -------------------
    begin                : Mon Mar 17 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
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

#include <klocale.h>

#include <qnamespace.h>
#include <qhgroupbox.h>
#include <qhbox.h>

#include "clippropertiesdialog.h"
#include "docclipavfile.h"
#include "docclipref.h"

namespace Gui {

    ClipPropertiesDialog::ClipPropertiesDialog(QWidget * parent,
	const char *name):QVBox(parent, name), m_clip(0) {
	//video info layout
	QHGroupBox *m_videoGBox =
	    new QHGroupBox(i18n("Video Stream"), this, "videoStream");
	 m_videoGBox->setAlignment(0);
	 m_videoGBox->setOrientation(Horizontal);
	 m_videoGBox->setInsideSpacing(3);
	QHBox *m_videoHBox = new QHBox(m_videoGBox);
	QVBox *m_videoVBoxL = new QVBox(m_videoHBox);
	QVBox *m_videoVBoxR = new QVBox(m_videoHBox);
	//video stream headers
	new QLabel(i18n("Filename:"), m_videoVBoxL);
	new QLabel(i18n("Frame size, (fps):"), m_videoVBoxL);
	new QLabel(i18n("Length (hh:mm:ss):"), m_videoVBoxL);
	new QLabel(i18n("System:"), m_videoVBoxL);
	new QLabel(i18n("Decompressor:"), m_videoVBoxL);
	//video stream info data
	 filenameLabel = new QLabel(m_videoVBoxR);
	 filenameLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 frameSizeLabel = new QLabel(m_videoVBoxR);
	 frameSizeLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 videoLength = new QLabel(m_videoVBoxR);
	 videoLength->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 systemLabel = new QLabel(m_videoVBoxR);
	 systemLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 decompressorLabel = new QLabel(m_videoVBoxR);
	 decompressorLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);

	//audio info layout
	QHGroupBox *m_audioGBox =
	    new QHGroupBox(i18n("Audio Stream"), this, "audioStream");
	 m_audioGBox->setAlignment(0);
	 m_audioGBox->setOrientation(Horizontal);
	 m_audioGBox->setInsideSpacing(3);
	QHBox *m_audioHBox = new QHBox(m_audioGBox);
	QVBox *m_audioVBoxL = new QVBox(m_audioHBox);
	QVBox *m_audioVBoxR = new QVBox(m_audioHBox);
	//audio stream headers
	new QLabel(i18n("Sampling Rate:"), m_audioVBoxL);
	new QLabel(i18n("Channels:"), m_audioVBoxL);
	new QLabel(i18n("Format:"), m_audioVBoxL);
	new QLabel(i18n("Bits per Sample:"), m_audioVBoxL);
	//audio stream info data
	 samplingRateLabel = new QLabel(m_audioVBoxR);
	 samplingRateLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 channelsLabel = new QLabel(m_audioVBoxR);
	 channelsLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 formatLabel = new QLabel(m_audioVBoxR);
	 formatLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	 audiobitLabel = new QLabel(m_audioVBoxR);
	 audiobitLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    } ClipPropertiesDialog::~ClipPropertiesDialog() {
    }

//display properties based on selected clip
    void ClipPropertiesDialog::setClip(DocClipRef * clip) {
	m_clip = clip;
	if (m_clip) {
	    //video properties
	    filenameLabel->setText(m_clip->name());
            if (m_clip->clipWidth()!=0 && m_clip->clipHeight()!=0)
	    frameSizeLabel->setText(i18n("%1x%2, %3 fps").arg(m_clip->
		    clipWidth()).arg(clip->clipHeight()).arg(m_clip->
		    framesPerSecond()));
            else frameSizeLabel->setText(i18n("Unknown size, %1 fps").arg(m_clip->framesPerSecond()));
	    if (m_clip->durationKnown()) {
		QString length = "";
		int seconds = (int) m_clip->duration().seconds() % 60;
		int minutes =
		    (int) floor(m_clip->duration().seconds() / 60);
		int hours = (int) floor(minutes / 60);
		if (hours >= 1) {
		    minutes = (int) hours % 60;
		} else {
		    hours = 0;
		}
		length.append(QString::number(hours).rightJustify(1, '0',
			FALSE));
		length.append(":");
		length.append(QString::number(minutes).rightJustify(2, '0',
			FALSE));
		length.append(":");
		length.append(QString::number(seconds).rightJustify(2, '0',
			FALSE));
		videoLength->setText(length);
	    } else {
		videoLength->setText(i18n("unknown"));
	    }
	    systemLabel->setText(m_clip->avSystem());
	    decompressorLabel->setText(m_clip->avDecompressor());

	    //audio properties
	    samplingRateLabel->setText(i18n("0Hz"));
	    switch (m_clip->audioChannels()) {
	    case 2:
		channelsLabel->setText(i18n("%1 (stereo)").arg(m_clip->
			audioChannels()));
		break;
	    case 1:
		channelsLabel->setText(i18n("%1 (mono)").arg(m_clip->
			audioChannels()));
		break;
	    default:
		channelsLabel->setText(QString::number(m_clip->
			audioChannels()));
		break;
	    }
	    formatLabel->setText(m_clip->audioFormat());
	    audiobitLabel->setText(i18n("%1 bit").arg(m_clip->
		    audioBits()));
	}
    }

}				// namespace Gui
