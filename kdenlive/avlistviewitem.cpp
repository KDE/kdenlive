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

#include "avlistviewitem.h"
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

AVListViewItem::AVListViewItem(KdenliveDoc * doc, QListViewItem * parent, DocumentBaseNode * node):
KListViewItem(parent),
m_listView(parent->listView()), m_node(node), m_doc(doc)
{
    if (node == NULL) {
	kdError() <<
	    "Creating AVListViewItem with no DocumentBaseNode defined!!!"
	    << endl;
    }
    doCommonCtor();
}

AVListViewItem::AVListViewItem(KdenliveDoc * doc, QListView * parent, DocumentBaseNode * node):
KListViewItem(parent), m_listView(parent), m_node(node), m_doc(doc)
{
    doCommonCtor();
}

void AVListViewItem::doCommonCtor()
{
    if (m_node->asClipNode()) setRenameEnabled(2, true);
    // recursively populate the rest of the node tree.
    QPtrListIterator < DocumentBaseNode > child(m_node->children());
    while (child.current()) {
	if (child.current()) {
		new AVListViewItem(m_doc, this, child.current());
	}
	++child;
    }

}

QString AVListViewItem::key ( int column, bool ascending ) const
{
  if (column == 0) column = 1;
  QString key = QListViewItem::key(column, ascending);
  // Hack to make folders appear first in the list
  if (!m_node->asClipNode()) key = "000000" + key;
  return key; 
} 

void AVListViewItem::paintCell(QPainter *p, const QColorGroup &cg, int column, int width, int align)
{
    if (column == 1 && m_node->asClipNode()) {
        // Draw the clip name with duration underneath
        QFont font = p->font();
        font.setPointSize(font.pointSize() - 2 );
        int fontHeight = QFontMetrics(p->font()).height() + 1;
        int smallFontHeight = QFontMetrics(font).height();
        p->setPen(QPen(NoPen));
        if (isSelected()) p->setBrush(cg.highlight());
	else p->setBrush(QColor(backgroundColor(column)));
        p->drawRect(0,0,m_listView->header()->sectionSize(1),height());
        QRect r1(0, height()/2 - (fontHeight + smallFontHeight)/2, m_listView->header()->sectionSize(1), fontHeight);
        
        if (isSelected()) p->setPen(cg.highlightedText());
        p->drawText(r1, Qt::AlignLeft | Qt::AlignBottom | Qt::SingleLine, text(1));
        QRect r2(0, height()/2 - (fontHeight + smallFontHeight)/2 + fontHeight, m_listView->header()->sectionSize(1), smallFontHeight);
	p->setFont(font);
        p->setPen(cg.mid());
	p->drawText(r2, Qt::AlignLeft | Qt::AlignTop | Qt::SingleLine, clipDuration());
	return;
    }
    KListViewItem::paintCell(p, cg, column, width, align);
}

AVListViewItem::~AVListViewItem()
{
}

void AVListViewItem::setText(int column, const QString & text)
{
    //kdDebug() << "setText ( " << column << ", " << text << " ) " << endl;
    if (m_listView->columnText(column) == i18n("Description")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    clipNode->clipRef()->referencedClip()->setDescription(text);
	}
    }
}

QString AVListViewItem::clipDuration() const {
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

QString AVListViewItem::getInfo() const
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
		text.append(i18n("%1 clips").arg(childCount()));

	}
	return text;
}

QString AVListViewItem::text(int column) const
{
    QString text = "";
    if (m_listView->columnText(column) == i18n("Filename")) {
	text = m_node->name();
    } else if (m_listView->columnText(column) == i18n("Duration")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    if (clip->durationKnown()) {
		/*Timecode timecode;
		timecode.setFormat(Timecode::Frames);

                text = timecode.getTimecode(clip->duration(), KdenliveSettings::defaultfps());*/
	    } else {
		text = i18n("unknown");
	    }
	}
    } else if (m_listView->columnText(column) == i18n("Size")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    text = clip->formattedFileSize();
	}
    } else if (m_listView->columnText(column) == i18n("Type")) {
	if (m_node->asClipNode()) {
	    DocumentClipNode *clipNode = m_node->asClipNode();
	    if (clipNode) {
		DocClipRef *clip = clipNode->clipRef();
		if (clip->clipType() == DocClipBase::AV)
		    text = i18n("video");
		else if (clip->clipType() == DocClipBase::VIDEO)
		    text = i18n("mute video");
		else if (clip->clipType() == DocClipBase::AUDIO)
		    text = i18n("audio");
	    }
	} else if (m_node->asGroupNode()) {
	    text = i18n("group");
	} else {
	    text = i18n("n/a");
	}
    } else if (m_listView->columnText(column) == i18n("Usage Count")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    text = QString::number(clip->numReferences());
	}
    } else if (m_listView->columnText(column) == i18n("Description")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    text = clip->description();
	}
    }

    return text;
}

const QPixmap *AVListViewItem::pixmap(int column) const
{
    const QPixmap *pixmap = 0;

    if (m_listView->columnText(column) == i18n("Thumbnail")) {
	DocumentClipNode *clipNode = m_node->asClipNode();
	if (clipNode) {
	    DocClipRef *clip = clipNode->clipRef();
	    DocClipBase *baseClip = clip->referencedClip();
	    pixmap = &(baseClip->thumbnail());

	    if (pixmap->isNull())
		pixmap = NULL;
	}
	else {
	    pixmap = new QPixmap(KGlobal::iconLoader()->loadIcon("folder", KIcon::Toolbar));
	}
    }

    return pixmap;
}

DocClipRef *AVListViewItem::clip() const
{
    DocumentClipNode *clipNode = m_node->asClipNode();
    if (clipNode) {
	return clipNode->clipRef();
    }
    return 0;
}
