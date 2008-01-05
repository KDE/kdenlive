#ifndef ADDCLIPCOMMAND_H
#define ADDCLIPCOMMAND_H

#include <QUndoCommand>
#include <KDebug>

#include "projectlist.h"

class AddClipCommand : public QUndoCommand
 {
 public:
     AddClipCommand(ProjectList *list, const QStringList &names, const QDomElement &xml, const int id, const KUrl &url, bool doIt)
         : m_list(list), m_names(names), m_xml(xml), m_id(id), m_url(url), m_doIt(doIt) {
	    if (doIt) setText(i18n("Add clip"));
	    else setText(i18n("Delete clip"));
	  }
     virtual void undo()
         {
	    kDebug()<<"----  undoing action";
	    if (m_doIt) m_list->deleteClip(m_id);
	    else m_list->addClip(m_names, m_xml, m_id, m_url);
	 }
     virtual void redo()
         {
	    kDebug()<<"----  redoing action";
	    if (m_doIt) m_list->addClip(m_names, m_xml, m_id, m_url);
	    else m_list->deleteClip(m_id);
	 }
 private:
     ProjectList *m_list;
     QStringList m_names;
     QDomElement m_xml;
     int m_id;
     KUrl m_url;
     bool m_doIt;
 };

#endif

