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
    ProjectItem(QTreeWidget * parent, const QStringList & strings, QDomElement xml = QDomElement(), int type = QTreeWidgetItem::UserType);
    ~ProjectItem();
    QDomElement toXml();

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);

  private:
    QDomElement m_element;
    GenTime m_duration;
    bool m_durationKnown;
    DocClipBase::CLIPTYPE m_clipType;
    void slotSetToolTip();
};

#endif
