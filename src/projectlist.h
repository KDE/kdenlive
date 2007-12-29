#ifndef PRJECTLIST_H
#define PRJECTLIST_H

#include <QDomNodeList>
#include <QTreeWidget>
#include <KTreeWidgetSearchLine>

#include "docclipbase.h"
#include "renderer.h"

class ProjectList : public QWidget
{
  Q_OBJECT
  
  public:
    ProjectList(Render *projectRender, QWidget *parent=0);

  public slots:
    void populate(QDomNodeList prods);
    void addProducer(QDomElement producer);
    void setRenderer(Render *projectRender);
    void slotReplyGetImage(const KUrl &url, int pos, const QPixmap &pix, int w, int h);
    void slotReplyGetFileProperties(const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata);

  private:
    QTreeWidget *listView;
    KTreeWidgetSearchLine *searchView;
    Render *m_render;

  private slots:
    void slotDoubleClicked(QListWidgetItem *, const QPoint &);
    void slotAddClip();
    void slotRemoveClip();
    void slotEditClip();
    void slotClipSelected();
    void slotAddColorClip();

  signals:
    void clipSelected(const QDomElement &);
    void getFileProperties(const KUrl &, uint);
};

#endif
