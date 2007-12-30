
#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QToolBar>
#include <QDialog>

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <klistwidgetsearchline.h>

#include "projectlist.h"
#include "projectitem.h"
#include "ui_colorclip_ui.h"

#include <QtGui>

  const int NameRole = Qt::UserRole;
  const int DurationRole = NameRole + 1;
  const int FullPathRole = NameRole + 2;
  const int ClipTypeRole = NameRole + 3;

class ItemDelegate: public QItemDelegate
{
  public:
    ItemDelegate(QObject* parent = 0): QItemDelegate(parent)
    {
    }

void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
  if (index.column() == 1)
  {
    QFont font = painter->font();
    font.setPointSize(font.pointSize() - 2 );
    QRect r1 = option.rect;
    r1.setBottom(r1.y() + (r1.height() *2 / 3 ));
    QRect r2 = option.rect;
    r2.setTop(r2.y() + (r2.height() *2 / 3 ));
    painter->drawText(r1, Qt::AlignLeft | Qt::AlignVCenter , index.data().toString());
    painter->setPen(Qt::green);
    painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop , index.data(DurationRole).toString());
    painter->setPen(Qt::black);
  }
  else
  {
    QItemDelegate::paint(painter, option, index);
  }
}
};


ProjectList::ProjectList(Render *projectRender, QWidget *parent)
    : QWidget(parent), m_render(projectRender)
{

  QWidget *vbox = new QWidget;
  listView = new QTreeWidget(this);;
  QVBoxLayout *layout = new QVBoxLayout;

  // setup toolbar
  searchView = new KTreeWidgetSearchLine (this);
  QToolBar *bar = new QToolBar("projectToolBar", this);
  bar->addWidget (searchView);

  QToolButton *addButton = new QToolButton( bar );
  QMenu *addMenu = new QMenu(this);
  addButton->setMenu( addMenu );
  addButton->setPopupMode(QToolButton::MenuButtonPopup);
  bar->addWidget (addButton);
  
  QAction *addClip = addMenu->addAction (KIcon("document-new"), i18n("Add Clip"));
  connect(addClip, SIGNAL(triggered()), this, SLOT(slotAddClip()));

  QAction *addColorClip = addMenu->addAction (KIcon("document-new"), i18n("Add Color Clip"));
  connect(addColorClip, SIGNAL(triggered()), this, SLOT(slotAddColorClip()));

  QAction *deleteClip = bar->addAction (KIcon("edit-delete"), i18n("Delete Clip"));
  connect(deleteClip, SIGNAL(triggered()), this, SLOT(slotRemoveClip()));

  QAction *editClip = bar->addAction (KIcon("document-properties"), i18n("Edit Clip"));
  connect(editClip, SIGNAL(triggered()), this, SLOT(slotEditClip()));

  addButton->setDefaultAction( addClip );

  layout->addWidget( bar );
  layout->addWidget( listView );
  setLayout( layout );

  searchView->setTreeWidget(listView);
  listView->setColumnCount(3);
  QStringList headers;
  headers<<i18n("Thumbnail")<<i18n("Filename")<<i18n("Description");
  listView->setHeaderLabels(headers);

  QStringList itemEntry;
  itemEntry.append(QString::null);
  itemEntry.append("coucou");
  new ProjectItem(listView, itemEntry);

  connect(listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
  //connect(listView, SIGNAL(itemDoubleClicked ( QTreeWidgetItem *, int )), this, SLOT(slotEditClip(QTreeWidgetItem *, int)));


  listView->setItemDelegate(new ItemDelegate(listView));
  listView->setIconSize(QSize(60, 40));
  listView->setSortingEnabled (true);

}

void ProjectList::setRenderer(Render *projectRender)
{
  m_render = projectRender;
}

void ProjectList::slotDoubleClicked(QListWidgetItem *item, const QPoint &pos)
{
  kDebug()<<" / / / DBL CLICK";
  if (item) {
    KUrl url = KFileDialog::getOpenUrl( KUrl(), "video/mpeg");
  }
}

void ProjectList::slotClipSelected()
{
  ProjectItem *item = (ProjectItem*) listView->currentItem();
  if (item) emit clipSelected(item->toXml());
}

void ProjectList::slotEditClip()
{

}

void ProjectList::slotRemoveClip()
{
  kDebug()<<"//////////  SLOT REMOVE";
  if (!listView->currentItem()) return;
  delete listView->currentItem();
}

void ProjectList::slotAddClip()
{
   
  KUrl::List list = KFileDialog::getOpenUrls( KUrl(), "application/vnd.kde.kdenlive application/vnd.westley.scenelist application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg video/x-ms-wmv audio/x-mp3 audio/x-wav application/ogg *.m2t *.dv video/mp4 video/quicktime image/gif image/jpeg image/png image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr");

  KUrl::List::Iterator it;
  KUrl url;
	
  for (it = list.begin(); it != list.end(); it++) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append((*it).fileName());
      ProjectItem *item = new ProjectItem(listView, itemEntry, QDomElement());
      item->setData(1, FullPathRole, (*it).path());
      emit getFileProperties((*it), 0);
  }
}

void ProjectList::slotAddColorClip()
{
  QDialog *dia = new QDialog;
  Ui::ColorClip_UI *dia_ui = new Ui::ColorClip_UI();
  dia_ui->setupUi(dia);
  dia_ui->clip_name->setText(i18n("Color Clip"));
  if (dia->exec() == QDialog::Accepted)
  {
    QDomDocument doc;
    QDomElement element = doc.createElement("producer");
    element.setAttribute("mlt_service", "colour");
    QString color = dia_ui->clip_color->color().name();
    color = color.replace(0, 1, "0x") + "ff";
    element.setAttribute("colour", color);
    element.setAttribute("type", (int) DocClipBase::COLOR);
    QStringList itemEntry;
    itemEntry.append(QString::null);
    itemEntry.append(dia_ui->clip_name->text());
    ProjectItem *item = new ProjectItem(listView, itemEntry, element);
    QPixmap pix(60, 40);
    pix.fill(dia_ui->clip_color->color());
    item->setIcon(0, QIcon(pix));

    
  }
  delete dia_ui;
  delete dia;
  /*for (it = list.begin(); it != list.end(); it++) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append((*it).fileName());
      ProjectItem *item = new ProjectItem(listView, itemEntry, QDomElement());
      item->setData(1, FullPathRole, (*it).path());
      emit getFileProperties((*it), 0);
  }*/
}

void ProjectList::populate(QDomNodeList prods)
{
  listView->clear();
  for (int i = 0; i <  prods.count () ; i++)
  {
    addProducer(prods.item(i).toElement());
  }
}

void ProjectList::slotReplyGetFileProperties(const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata)
{
  QTreeWidgetItem *parent = 0;
  int count =
    parent ? parent->childCount() : listView->topLevelItemCount();

  for (int i = 0; i < count; i++)
  {
    QTreeWidgetItem *item =
      parent ? parent->child(i) : listView->topLevelItem(i);

    if (item->data(1, FullPathRole).toString() == properties["filename"]) {
      ((ProjectItem *) item)->setProperties(properties, metadata);
      break;
    }
  }
}


void ProjectList::slotReplyGetImage(const KUrl &url, int pos, const QPixmap &pix, int w, int h)
{
   QTreeWidgetItem *parent = 0;
  int count =
    parent ? parent->childCount() : listView->topLevelItemCount();

  for (int i = 0; i < count; i++)
  {
    QTreeWidgetItem *item =
      parent ? parent->child(i) : listView->topLevelItem(i);

    if (item->data(1, FullPathRole).toString() == url.path()) {
      item->setIcon(0,pix);
      break;
    }
  }

}


void ProjectList::addProducer(QDomElement producer)
{
  DocClipBase::CLIPTYPE type = (DocClipBase::CLIPTYPE) producer.attribute("type").toInt();
  
  if (type == DocClipBase::AUDIO || type == DocClipBase::VIDEO || type == DocClipBase::AV)
  {
    KUrl resource = KUrl(producer.attribute("resource"));
    if (!resource.isEmpty()) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append(resource.fileName());
      ProjectItem *item = new ProjectItem(listView, itemEntry, producer);
      //item->setIcon(0, Render::getVideoThumbnail(resource, 0, 60, 40));
      item->setData(1, FullPathRole, resource.path());
      item->setData(1, ClipTypeRole, (int) type);
      emit getFileProperties(resource, producer.attribute("frame_thumbnail", 0).toInt());
    }
  }
  else if (type == DocClipBase::COLOR) {
    QString colour = producer.attribute("colour");
    QPixmap pix(60, 40);
    colour = colour.replace(0, 2, "#");
    pix.fill(QColor(colour.left(7)));
    QStringList itemEntry;
    itemEntry.append(QString::null);
    itemEntry.append(producer.attribute("name"));
    ProjectItem *item = new ProjectItem(listView, itemEntry, producer);
    item->setIcon(0, QIcon(pix));
    item->setData(1, ClipTypeRole, (int) type);
  }
      
}

#include "projectlist.moc"
