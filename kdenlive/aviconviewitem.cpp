/***************************************************************************
                          avlistviewitem.cpp  -  description
                             -------------------
    begin                : Wed Mar 20 2002
    copyright            : (C) 2002 by Jason Wood
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

#include "aviconviewitem.h"
#include "docclipavfile.h"

#include <qptrlist.h>
#include <qpainter.h>
#include <qheader.h>

#include <klocale.h>
#include <kdebug.h>
#include <kiconloader.h>

#include "kdenlivedoc.h"
#include "documentbasenode.h"
#include "documentclipnode.h"
#include "timecode.h"
#include "kdenlivesettings.h"

AVIconViewItem::AVIconViewItem(KdenliveDoc * doc, QIconViewItem * parent, DocumentBaseNode * node):
KIconViewItem(parent->iconView(), parent),
m_iconView(parent->iconView()), m_node(node), m_doc(doc)
{
    if (node == NULL) {
	kdError() <<
	    "Creating AVIconViewItem with no DocumentBaseNode defined!!!"
	    << endl;
    }
    doCommonCtor();
}

AVIconViewItem::AVIconViewItem(KdenliveDoc * doc, QIconView * parent, DocumentBaseNode * node):
KIconViewItem(parent), m_iconView(parent), m_node(node), m_doc(doc)
{
    DocumentClipNode *clipNode = m_node->asClipNode();
    setText(m_node->name());
    if (clipNode) {
	DocClipRef *clip = clipNode->clipRef();
	setPixmap(clip->referencedClip()->thumbnail());
    }
    else {
	setPixmap(QPixmap(KGlobal::iconLoader()->loadIcon("folder", KIcon::Toolbar)));
    }
    doCommonCtor();
}

void AVIconViewItem::doCommonCtor()
{
    // recursively populate the rest of the node tree.
    QPtrListIterator < DocumentBaseNode > child(m_node->children());
    while (child.current()) {
	if (child.current()) {
		new AVIconViewItem(m_doc, this, child.current());
	}
	++child;
    }

}

AVIconViewItem::~AVIconViewItem()
{
}

QString AVIconViewItem::text() const
{
    return m_node->name();
}


QPixmap *AVIconViewItem::pixmap() const
{
    DocumentClipNode *clipNode = m_node->asClipNode();
    if (clipNode) {
	DocClipRef *clip = clipNode->clipRef();
	DocClipBase *baseClip = clip->referencedClip();
	return new QPixmap(baseClip->thumbnail());
    }
    else {
	return new QPixmap(KGlobal::iconLoader()->loadIcon("folder", KIcon::Toolbar));
    }
}

QString AVIconViewItem::clipDuration() const {
	QString text;
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
            text = Timecode::getEasyTimecode(clip->duration(), KdenliveSettings::defaultfps());
	    int usage = clip->referencedClip()->numReferences();
	    if (usage > 0) {
	    	text.append(", [" + QString::number(usage) + "]");
	    }
	}
	return text;
}

QString AVIconViewItem::getInfo() const
{
	QString text;
	DocumentClipNode *clipNode = NULL;
	if (m_node) clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    DocClipBase::CLIPTYPE fileType = clip->clipType();
		if (fileType == DocClipBase::AV)
		    text = "<b>"+i18n("Video Clip")+"</b><br>";
		else if (fileType == DocClipBase::VIDEO)
		    text = "<b>"+i18n("Mute Video Clip")+"</b><br>";
		else if (fileType == DocClipBase::AUDIO)
		    text = "<b>"+i18n("Audio Clip")+"</b><br>";
	    	else if (fileType == DocClipBase::COLOR)
		    text = "<b>"+i18n("Color Clip")+"</b><br>";
	    	else if (fileType == DocClipBase::VIRTUAL)
		    text = "<b>"+i18n("Virtual Clip")+"</b><br>";
	    	else if (fileType == DocClipBase::IMAGE)
		    text = "<b>"+i18n("Image Clip")+"</b><br>";
		else if (fileType == DocClipBase::SLIDESHOW)
		    text = "<b>"+i18n("Slideshow Clip")+"</b><br>";
            	else if (fileType == DocClipBase::TEXT)
                    text = "<b>"+i18n("Text Clip")+"</b><br>";

	    if (fileType != DocClipBase::TEXT && fileType != DocClipBase::COLOR && fileType != DocClipBase::VIRTUAL) {
		text.append(i18n("Path: %1").arg(clip->fileURL().directory()) + "<br>" );
	    	text.append(i18n("File Size: ") + clip->formattedFileSize() + "<br>" );
	    }
	if (clip->audioChannels() + clip->audioFrequency() != 0) {
	    QString soundChannels;
	    switch (clip->audioChannels()) {
	        case 1:
		    soundChannels = i18n("Mono");
		    break;
	        case 2:
		    soundChannels = i18n("Stereo");
		    break;
	        default:
		    soundChannels = i18n("%1 Channels").arg(clip->audioChannels());
		    break;
	    }
	    text.append(i18n("Audio: %1Hz %2").arg(QString::number(clip->audioFrequency())).arg(soundChannels) + "<br>");
	}
	text.append(i18n("Usage: %1").arg(QString::number(clip->numReferences())));
	}
	else {
		text = "<b>"+i18n("Folder")+"</b><br>";
		//text.append(i18n("%1 clips").arg(count()));

	}
	return text;
}

DocClipRef *AVIconViewItem::clip() const
{
    DocumentClipNode *clipNode = m_node->asClipNode();
    if (clipNode) {
	return clipNode->clipRef();
    }
    return 0;
}
