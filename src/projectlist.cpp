/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include <QMouseEvent>
#include <QStylePainter>
#include <QPixmap>
#include <QIcon>
#include <QDialog>

#include <KDebug>
#include <KAction>
#include <KLocale>
#include <KFileDialog>
#include <KInputDialog>
#include <nepomuk/resourcemanager.h>
#include <kio/netaccess.h>
#include <KMessageBox>

#include "projectlist.h"
#include "projectitem.h"
#include "kdenlivesettings.h"
#include "ui_colorclip_ui.h"

#include "addclipcommand.h"

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
    const bool hover = option.state & (QStyle::State_Selected|QStyle::State_MouseOver|QStyle::State_HasFocus);
    QRect r1 = option.rect;
    painter->save();
    if (hover) {
        painter->setPen(option.palette.color(QPalette::HighlightedText));
        QColor backgroundColor = option.palette.color(QPalette::Highlight);
        painter->setBrush(QBrush(backgroundColor));
	painter->fillRect(r1, QBrush(backgroundColor));
    }
    QFont font = painter->font();
    font.setPointSize(font.pointSize() - 1 );
    font.setBold(true);
    painter->setFont(font);
    int mid = (int) ((r1.height() / 2 ));
    r1.setBottom(r1.y() + mid);
    QRect r2 = option.rect;
    r2.setTop(r2.y() + mid);
    painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data().toString());
    //painter->setPen(Qt::green);
    font.setBold(false);
    painter->setFont(font);
    painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , index.data(DurationRole).toString());
    painter->restore();
  }
  else
  {
    QItemDelegate::paint(painter, option, index);
  }
}
};


ProjectList::ProjectList(QWidget *parent)
    : QWidget(parent), m_render(NULL), m_fps(-1), m_commandStack(NULL)
{

  QWidget *vbox = new QWidget;
  listView = new ProjectListView(this);;
  QVBoxLayout *layout = new QVBoxLayout;
  m_clipIdCounter = 0;

  // setup toolbar
  searchView = new KTreeWidgetSearchLine (this);
  m_toolbar = new QToolBar("projectToolBar", this);
  m_toolbar->addWidget (searchView);

  QToolButton *addButton = new QToolButton( m_toolbar );
  QMenu *addMenu = new QMenu(this);
  addButton->setMenu( addMenu );
  addButton->setPopupMode(QToolButton::MenuButtonPopup);
  m_toolbar->addWidget (addButton);

  QAction *addClipButton = addMenu->addAction (KIcon("document-new"), i18n("Add Clip"));
  connect(addClipButton, SIGNAL(triggered()), this, SLOT(slotAddClip()));

  QAction *addColorClip = addMenu->addAction (KIcon("document-new"), i18n("Add Color Clip"));
  connect(addColorClip, SIGNAL(triggered()), this, SLOT(slotAddColorClip()));

  m_deleteAction = m_toolbar->addAction (KIcon("edit-delete"), i18n("Delete Clip"));
  connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(slotRemoveClip()));

  m_editAction = m_toolbar->addAction (KIcon("document-properties"), i18n("Edit Clip"));
  connect(m_editAction, SIGNAL(triggered()), this, SLOT(slotEditClip()));

  QAction *addFolderButton = addMenu->addAction (KIcon("folder-new"), i18n("Create Folder"));
  connect(addFolderButton, SIGNAL(triggered()), this, SLOT(slotAddFolder()));

  addButton->setDefaultAction( addClipButton );

  layout->addWidget( m_toolbar );
  layout->addWidget( listView );
  setLayout( layout );
  m_toolbar->setEnabled(false);

  searchView->setTreeWidget(listView);
  listView->setColumnCount(3);
  QStringList headers;
  headers<<i18n("Thumbnail")<<i18n("Filename")<<i18n("Description");
  listView->setHeaderLabels(headers);
  listView->sortByColumn(1, Qt::AscendingOrder);

  m_menu = new QMenu();	
  m_menu->addAction(addClipButton);
  m_menu->addAction(addColorClip);
  m_menu->addAction(m_editAction);
  m_menu->addAction(m_deleteAction);
  m_menu->addAction(addFolderButton);
  m_menu->insertSeparator(m_deleteAction);

  connect(listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
  connect(listView, SIGNAL(requestMenu ( const QPoint &, QTreeWidgetItem * )), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
  connect(listView, SIGNAL(addClip ()), this, SLOT(slotAddClip()));
  connect(listView, SIGNAL(addClip (QUrl, const QString &)), this, SLOT(slotAddClip(QUrl, const QString &)));


  listView->setItemDelegate(new ItemDelegate(listView));
  listView->setIconSize(QSize(60, 40));
  listView->setSortingEnabled (true);

}

ProjectList::~ProjectList()
{
  delete m_menu;
  delete m_toolbar;
}

void ProjectList::setRenderer(Render *projectRender)
{
  m_render = projectRender;
}

void ProjectList::slotClipSelected()
{
  ProjectItem *item = (ProjectItem*) listView->currentItem();
  if (item && !item->isGroup()) emit clipSelected(item->toXml());
}

void ProjectList::slotEditClip()
{
  kDebug()<<"////////////////////////////////////////   DBL CLK";
}


void ProjectList::slotEditClip(QTreeWidgetItem *item, int column)
{
  kDebug()<<"////////////////////////////////////////   DBL CLK";
}

void ProjectList::slotContextMenu( const QPoint &pos, QTreeWidgetItem *item )
{
  bool enable = false;
  if (item) {
    enable = true;
  }
  m_editAction->setEnabled(enable);
  m_deleteAction->setEnabled(enable);

  m_menu->popup(pos);
}

void ProjectList::slotRemoveClip()
{
  if (!m_commandStack) kDebug()<<"!!!!!!!!!!!!!!!!  NO CMD STK";
  if (!listView->currentItem()) return;
  ProjectItem *item = ((ProjectItem *)listView->currentItem());
  if (!item) kDebug()<<"///////////////  ERROR NOT FOUND";
  if (KMessageBox::questionYesNo(this, i18n("Delete clip <b>%1</b> ?").arg(item->names().at(1)), i18n("Delete Clip")) != KMessageBox::Yes) return;
  AddClipCommand *command = new AddClipCommand(this, item->names(), item->toXml(), item->clipId(), item->clipUrl(), item->groupName(), false);
  m_commandStack->push(command);
}

void ProjectList::selectItemById(const int clipId)
{
  ProjectItem *item = getItemById(clipId);
  if (item) listView->setCurrentItem(item);
}

void ProjectList::addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url, const QString &group, int parentId)
{
  kDebug()<<"/////////  ADDING VCLIP=: "<<name;
  ProjectItem *item;
  ProjectItem *groupItem = NULL;
  QString groupName;
  if (group.isEmpty()) groupName = elem.attribute("group", QString::null);
  else groupName = group;
  if (elem.isNull() && url.isEmpty()) {
    // this is a folder
    groupName = name.at(1);
    QList<QTreeWidgetItem *> groupList = listView->findItems(groupName, Qt::MatchExactly, 1);
    if (groupList.isEmpty())  {
	(void) new ProjectItem(listView, name, clipId);
    }
    return;
  }

  if (parentId != -1) {
    groupItem = getItemById(parentId);
  }
  else if (!groupName.isEmpty()) {
    // Clip is in a group
    QList<QTreeWidgetItem *> groupList = listView->findItems(groupName, Qt::MatchExactly, 1);

    if (groupList.isEmpty())  {
	QStringList itemName;
	itemName<<QString::null<<groupName;
	kDebug()<<"-------  CREATING NEW GRP: "<<itemName;
	groupItem = new ProjectItem(listView, itemName, m_clipIdCounter++);
    }
    else groupItem = (ProjectItem *) groupList.first();
  }
  if (groupItem) item = new ProjectItem(groupItem, name, elem, clipId);
  else item = new ProjectItem(listView, name, elem, clipId);
  if (!url.isEmpty()) {
    item->setData(1, FullPathRole, url.path());
/*    Nepomuk::File f( url.path() );
    QString annotation = f.getAnnotation();
    if (!annotation.isEmpty()) item->setText(2, annotation);*/
    QString resource = url.path();
    if (resource.endsWith("westley") || resource.endsWith("kdenlive")) {
	QString tmpfile;
	QDomDocument doc;
	if (KIO::NetAccess::download(url, tmpfile, 0)) {
	    QFile file(tmpfile);
	    if (file.open(QIODevice::ReadOnly)) {
	      doc.setContent(&file, false);
	      file.close();
	    }
	    KIO::NetAccess::removeTempFile(tmpfile);

	    QDomNodeList subProds = doc.elementsByTagName("producer");
	    int ct = subProds.count();
	    for (int i = 0; i <  ct ; i++)
	    {
	      QDomElement e = subProds.item(i).toElement();
	      if (!e.isNull()) {
		addProducer(e, clipId);
	      }
	    }  
	  }
    }

  }

  if (elem.isNull() ) {
    QDomDocument doc;
    QDomElement element = doc.createElement("producer");
    element.setAttribute("resource", url.path());
    emit getFileProperties(element, clipId);
  }
  else emit getFileProperties(elem, clipId);
  selectItemById(clipId);
}

void ProjectList::deleteClip(const int clipId)
{
  ProjectItem *item = getItemById(clipId);
  if (item) delete item;
}

void ProjectList::slotAddFolder()
{
  QString folderName = KInputDialog::getText(i18n("New Folder"), i18n("Enter new folder name: "));
  if (folderName.isEmpty()) return;
  QStringList itemEntry;
  itemEntry.append(QString::null);
  itemEntry.append(folderName);
  AddClipCommand *command = new AddClipCommand(this, itemEntry, QDomElement(), m_clipIdCounter++, KUrl(), folderName, true);
  m_commandStack->push(command);
}

void ProjectList::slotAddClip(QUrl givenUrl, const QString &group)
{
  if (!m_commandStack) kDebug()<<"!!!!!!!!!!!!!!!!  NO CMD STK";
  KUrl::List list;
  if (givenUrl.isEmpty())
    list = KFileDialog::getOpenUrls( KUrl(), "application/vnd.kde.kdenlive application/vnd.westley.scenelist application/flv application/vnd.rn-realmedia video/x-dv video/x-msvideo video/mpeg video/x-ms-wmv audio/x-mp3 audio/x-wav application/ogg *.m2t *.dv video/mp4 video/quicktime image/gif image/jpeg image/png image/x-bmp image/svg+xml image/tiff image/x-xcf-gimp image/x-vnd.adobe.photoshop image/x-pcx image/x-exr");
  else list.append(givenUrl);
  if (list.isEmpty()) return;
  KUrl::List::Iterator it;
  KUrl url;
//  ProjectItem *item;

  for (it = list.begin(); it != list.end(); it++) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append((*it).fileName());
      AddClipCommand *command = new AddClipCommand(this, itemEntry, QDomElement(), m_clipIdCounter++, *it, group, true);
      m_commandStack->push(command);
      //item = new ProjectItem(listView, itemEntry, QDomElement());
      //item->setData(1, FullPathRole, (*it).path());
  }
  //listView->setCurrentItem(item);
}

void ProjectList::slotAddColorClip()
{
  if (!m_commandStack) kDebug()<<"!!!!!!!!!!!!!!!!  NO CMD STK";
  QDialog *dia = new QDialog;
  Ui::ColorClip_UI *dia_ui = new Ui::ColorClip_UI();
  dia_ui->setupUi(dia);
  dia_ui->clip_name->setText(i18n("Color Clip"));
  dia_ui->clip_duration->setText(KdenliveSettings::color_duration());
  if (dia->exec() == QDialog::Accepted)
  {
    QDomDocument doc;
    QDomElement element = doc.createElement("producer");
    element.setAttribute("mlt_service", "colour");
    QString color = dia_ui->clip_color->color().name();
    color = color.replace(0, 1, "0x") + "ff";
    element.setAttribute("colour", color);
    element.setAttribute("type", (int) DocClipBase::COLOR);
    element.setAttribute("in", "0");
    element.setAttribute("out", m_timecode.getFrameCount(dia_ui->clip_duration->text(), m_fps));
    element.setAttribute("name", dia_ui->clip_name->text());
    QStringList itemEntry;
    itemEntry.append(QString::null);
    itemEntry.append(dia_ui->clip_name->text());
    AddClipCommand *command = new AddClipCommand(this, itemEntry, element, m_clipIdCounter++, KUrl(), QString::null, true);
    m_commandStack->push(command);
    // ProjectItem *item = new ProjectItem(listView, itemEntry, element, m_clipIdCounter++);
    /*QPixmap pix(60, 40);
    pix.fill(dia_ui->clip_color->color());
    item->setIcon(0, QIcon(pix));*/
    //listView->setCurrentItem(item);
    
  }
  delete dia_ui;
  delete dia;
}

void ProjectList::setDocument(KdenliveDoc *doc)
{
  m_fps = doc->fps();
  m_timecode = doc->timecode();
  m_commandStack = doc->commandStack();
  QDomNodeList prods = doc->producersList();
  int ct = prods.count();
  kDebug()<<"////////////  SETTING DOC, FOUND CLIPS: "<<prods.count();
  listView->clear();
  for (int i = 0; i <  ct ; i++)
  {
    QDomElement e = prods.item(i).toElement();
    kDebug()<<"// IMPORT: "<<i<<", :"<<e.attribute("id", "non")<<", NAME: "<<e.attribute("name", "non");
    if (!e.isNull()) addProducer(e);
  }
  QTreeWidgetItem *first = listView->topLevelItem(0);
  if (first) listView->setCurrentItem(first);
  m_toolbar->setEnabled(true);
}

QDomElement ProjectList::producersList()
{
  QDomDocument doc;
  QDomElement prods = doc.createElement("producerlist");
  doc.appendChild(prods);
  kDebug()<<"////////////  PRO LIST BUILD PRDSLIST ";
    QTreeWidgetItemIterator it(listView);
     while (*it) {
         if (!((ProjectItem *)(*it))->isGroup())
	  prods.appendChild(doc.importNode(((ProjectItem *)(*it))->toXml(), true));
         ++it;
     }
  return prods;
}


void ProjectList::slotReplyGetFileProperties(int clipId, const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata)
{
  ProjectItem *item = getItemById(clipId);
  if (item) {
    item->setProperties(properties, metadata);
    emit receivedClipDuration(clipId, item->clipMaxDuration());
  }
}



void ProjectList::slotReplyGetImage(int clipId, int pos, const QPixmap &pix, int w, int h)
{
  ProjectItem *item = getItemById(clipId);
  if (item) item->setIcon(0, pix);
}

ProjectItem *ProjectList::getItemById(int id)
{
    QTreeWidgetItemIterator it(listView);
     while (*it) {
         if (((ProjectItem *)(*it))->clipId() == id)
	  break;
         ++it;
     }
  return ((ProjectItem *)(*it));
}


void ProjectList::addProducer(QDomElement producer, int parentId)
{
  if (!m_commandStack) kDebug()<<"!!!!!!!!!!!!!!!!  NO CMD STK";
  DocClipBase::CLIPTYPE type = (DocClipBase::CLIPTYPE) producer.attribute("type").toInt();

    /*QDomDocument doc;
    QDomElement prods = doc.createElement("list");
    doc.appendChild(prods);
    prods.appendChild(doc.importNode(producer, true));*/
    

  //kDebug()<<"//////  ADDING PRODUCER:\n "<<doc.toString()<<"\n+++++++++++++++++";
  int id = producer.attribute("id").toInt();
  QString groupName = producer.attribute("group");
  if (id >= m_clipIdCounter) m_clipIdCounter = id + 1;
  else if (id == 0) id = m_clipIdCounter++;

  if (parentId != -1) {
    // item is a westley playlist, adjust subproducers ids 
    id = (parentId + 1) * 10000 + id;
  }
  if (type == DocClipBase::AUDIO || type == DocClipBase::VIDEO || type == DocClipBase::AV || type == DocClipBase::IMAGE  || type == DocClipBase::PLAYLIST)
  {
    KUrl resource = KUrl(producer.attribute("resource"));
    if (!resource.isEmpty()) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append(resource.fileName());
      addClip(itemEntry, producer, id, resource, groupName, parentId);
      /*AddClipCommand *command = new AddClipCommand(this, itemEntry, producer, id, resource, true);
      m_commandStack->push(command);*/


      /*ProjectItem *item = new ProjectItem(listView, itemEntry, producer);
      item->setData(1, FullPathRole, resource.path());
      item->setData(1, ClipTypeRole, (int) type);*/
      
    }
  }
  else if (type == DocClipBase::COLOR) {
    QString colour = producer.attribute("colour");
    QPixmap pix(60, 40);
    colour = colour.replace(0, 2, "#");
    pix.fill(QColor(colour.left(7)));
    QStringList itemEntry;
    itemEntry.append(QString::null);
    itemEntry.append(producer.attribute("name", i18n("Color clip")));
    addClip(itemEntry, producer, id, KUrl(), groupName, parentId);
    /*AddClipCommand *command = new AddClipCommand(this, itemEntry, producer, id, KUrl(), true);
    m_commandStack->push(command);*/
    //ProjectItem *item = new ProjectItem(listView, itemEntry, producer);
    /*item->setIcon(0, QIcon(pix));
    item->setData(1, ClipTypeRole, (int) type);*/
  }
      
}

#include "projectlist.moc"
