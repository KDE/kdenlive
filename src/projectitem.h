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
    ProjectItem(QTreeWidgetItem * parent, const QStringList & strings, QDomElement xml, int clipId);
    ProjectItem(QTreeWidget * parent, const QStringList & strings);
    ~ProjectItem();
    QDomElement toXml();

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);
    int clipId();
    QStringList names();
    bool isGroup();

  private:
    QDomElement m_element;
    GenTime m_duration;
    bool m_durationKnown;
    DocClipBase::CLIPTYPE m_clipType;
    int m_clipId;
    void slotSetToolTip();
    bool m_isGroup;
};

#endif
