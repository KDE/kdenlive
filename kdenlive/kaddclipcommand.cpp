/***************************************************************************
                          kaddclipcommand.cpp  -  description
                             -------------------
    begin                : Fri Dec 13 2002
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

#include <kdebug.h>

#include "kaddclipcommand.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "clipmanager.h"
#include "docclipproject.h"
#include "docclipavfile.h"
#include "documentbasenode.h"
#include "documentclipnode.h"
#include "documentgroupnode.h"

namespace Command {


KCommand *KAddClipCommand::clearProject(KdenliveDoc &document)
{
	KMacroCommand *macroCommand = new KMacroCommand( i18n("Clean Project") );

	KCommand *command = clearChildren(*document.clipHierarch(), document);
	macroCommand->addCommand(command);

	return macroCommand;
}

KCommand *KAddClipCommand::clearChildren(DocumentBaseNode &node, KdenliveDoc &document)
{
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Clean project") );

	QPtrListIterator<DocumentBaseNode> itt(node.children());

	while(itt.current()) {
		DocumentBaseNode *node = itt.current();

		if(node->hasChildren()) {
			macroCommand->addCommand(clearChildren(*node, document));
		} else {
			DocumentClipNode *clipNode = node->asClipNode();

			if(clipNode) {
				DocClipBase *file = clipNode->clipRef()->referencedClip();
				if(file->numReferences() == 0) {
					macroCommand->addCommand(new KAddClipCommand(document, clipNode->name(), clipNode->clipRef()->referencedClip(), clipNode->parent(), false));
				}
			} else {
				macroCommand->addCommand(new KAddClipCommand(document, node->name(), NULL, node->parent(), false));
			}
		}

		++itt;
	}

	return macroCommand;
}

/** Construct an AddClipCommand that will add or delete a clip */
KAddClipCommand::KAddClipCommand(KdenliveDoc &document,
	       				const QString &name,
					DocClipBase *clip,
					DocumentBaseNode *parent,
					bool create) :
			m_document(document),
			m_name(name),
			m_parent(parent->name()),
			m_create(create),
			m_xmlClip(clip->toXML())
{
	if(!m_parent) {
		kdWarning() << "Error - all clips create with kaddclipcommand should have a parent!" << endl;
	}
}

KAddClipCommand::KAddClipCommand(KdenliveDoc &document, const KURL &url, bool create) :
		m_document(document),
		m_name(url.filename()),
		m_parent(document.clipHierarch()->name()),
		m_create(create)
{
	if(!m_parent) {
		kdWarning() << "Error - all clips create with kaddclipcommand should have a parent!" << endl;
	}

	DocClipBase *clip = document.clipManager().insertClip(url);

	DocumentClipNode *clipNode = new DocumentClipNode(0, clip);
	m_xmlClip = clipNode->clipRef()->toXML();
	delete clipNode;
}

KAddClipCommand::~KAddClipCommand()
{
}

/** Returns the name of this command */
QString KAddClipCommand::name() const
{
	if(m_create) {
		return i18n("Add Clip");
	} else {
		return i18n("Delete Clip");
	}
}

/** Execute the command */
void KAddClipCommand::execute()
{
	if(m_create) {
		addClip();
	} else {
		deleteClip();
	}
}

/** Unexecute the command */
void KAddClipCommand::unexecute()
{
	if(m_create) {
		deleteClip();
	} else {
		addClip();
	}
}

/** Adds the clip */
void KAddClipCommand::addClip()
{
	DocumentBaseNode *node = m_document.findClipNode(m_parent);

	if(!node) {
		kdWarning() << "Could not find parent in document, cannot add document base node" << endl;
	} else {
		DocClipBase *clip = m_document.clipManager().insertClip(m_xmlClip.documentElement());
		if(!clip) {
			m_document.addClipNode(m_parent, new DocumentGroupNode(node, m_name ));
		} else {
			m_document.addClipNode(m_parent, new DocumentClipNode(node, clip));
		}
	}
}

/** Deletes the clip */
void KAddClipCommand::deleteClip()
{
	DocumentBaseNode *node = m_document.findClipNode(m_parent);

	if(!node) {
		kdWarning() << "Could not find parent in document, cannot delete document base node" << endl;
	} else {
		m_document.deleteClipNode(m_name);
	}
}

} // namespace command

