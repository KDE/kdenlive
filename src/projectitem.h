#ifndef PROJECTITEM_H
#define PROJECTITEM_H

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QDomElement>

#include "gentime.h"
#include "docclipbase.h"


class ProjectItem : public QTreeWidgetItem
{
  public:
    ProjectItem(QTreeWidget * parent, const QStringList & strings, QDomElement xml, int clipId);
    ~ProjectItem();
    QDomElement toXml();

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);
    int clipId();
    QStringList names();

  private:
    QDomElement m_element;
    GenTime m_duration;
    bool m_durationKnown;
    DocClipBase::CLIPTYPE m_clipType;
    int m_clipId;
    void slotSetToolTip();

};

#endif
