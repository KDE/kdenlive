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

#include <klocale.h>
#include <kdenlivedoc.h>
#include <kdebug.h>

#include <math.h>

#include <documentbasenode.h>
#include <documentclipnode.h>
#include <timecode.h>

#include <iostream>

AVListViewItem::AVListViewItem(KdenliveDoc *doc, QListViewItem *parent, DocumentBaseNode *node) :
					KListViewItem(parent),
					m_listView(parent->listView()),
					m_node(node),
					m_doc(doc)

{
	if(node == NULL) {
		kdError() << "Creating AVListViewItem with no DocumentBaseNode defined!!!" << endl;
	}
	doCommonCtor();
}

AVListViewItem::AVListViewItem(KdenliveDoc *doc, QListView *parent, DocumentBaseNode *node) :
					KListViewItem(parent),
					m_listView(parent),
					m_node(node),
					m_doc(doc)
{
	doCommonCtor();
}

void AVListViewItem::doCommonCtor()
{
	setRenameEnabled(6, true);

	// recursively populate the rest of the node tree.
	QPtrListIterator<DocumentBaseNode> child(m_node->children());
	while(child.current())
	{
		new AVListViewItem(m_doc, this, child.current());
		++child;
	}

}

AVListViewItem::~AVListViewItem()
{
}

void AVListViewItem::setText( int column, const QString &text )
{
	std::cout << "setText ( " << column << ", " << text << " ) " << std::endl;
	if(m_listView->columnText(column) == i18n("Description")) {
		DocumentClipNode *clipNode = m_node->asClipNode();

		if(clipNode) {
			clipNode->clipRef()->referencedClip()->setDescription(text);
		}
	}
}

QString AVListViewItem::text ( int column ) const
{
	QString text = "";
	if(m_listView->columnText(column) == i18n("Filename")) {
 		text = m_node->name();
	} else 	if(m_listView->columnText(column) == i18n("Duration")) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			if(clip->durationKnown()) {
				Timecode timecode;
				timecode.setFormat(Timecode::Frames);

				text = timecode.getTimecode(clip->duration(), 25);
			} else {
				text = i18n("unknown");
			}
		}
	} else if(m_listView->columnText(column) == i18n("Size")) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			long fileSize = clip->fileSize();
			long tenth;
			if(fileSize < 1024) {
				text = QString::number(fileSize) + i18n(" byte(s)");
			} else {
				fileSize = (int)floor((fileSize / 1024.0)+0.5);

				if(fileSize < 1024) {
					text = QString::number(fileSize) + i18n(" Kb");
				} else {
					fileSize = (int)floor((fileSize / 102.4) + 0.5);

					tenth = fileSize % 10;
					fileSize /= 10;

					text = QString::number(fileSize) + "." +
						QString::number(tenth) + i18n(" Mb");
				}
			}
		}
	} else if(m_listView->columnText(column) == i18n("Type")) {
		if(m_node->asClipNode()) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			if (clip->clipType() == DocClipBase::AV) text = i18n("video");
			else if (clip->clipType() == DocClipBase::VIDEO) text = i18n("mute video");
			else if (clip->clipType() == DocClipBase::AUDIO) text = i18n("audio");
			}
		} else if(m_node->asGroupNode()) {
			text = i18n("group");
		} else {
			text = i18n("n/a");
		}
	} else if(m_listView->columnText(column) == i18n("Usage Count")) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			text = QString::number(clip->numReferences());
		}
	} else if(m_listView->columnText(column) == i18n("Description")) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			text = clip->description();
		} 
	}

	return text;
}

const QPixmap *AVListViewItem::pixmap ( int column ) const
{
	const QPixmap *pixmap = 0;

	if(m_listView->columnText(column) == i18n("Thumbnail")) {
		DocumentClipNode *clipNode = m_node->asClipNode();
		if(clipNode) {
			DocClipRef *clip = clipNode->clipRef();
			DocClipBase *baseClip = clip->referencedClip();
			pixmap = &(baseClip->thumbnail());

			if (pixmap->isNull()) pixmap = NULL;
		}
	}

	return pixmap;
}

DocClipRef *AVListViewItem::clip() const
{
	DocumentClipNode *clipNode = m_node->asClipNode();
	if(clipNode) {
		return clipNode->clipRef();
	}
	return 0;
}

