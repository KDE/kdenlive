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
#include <kio/netaccess.h>
#include <KMessageBox>

#include <nepomuk/global.h>
#include <nepomuk/resource.h>
#include <nepomuk/tag.h>

#include "projectlist.h"
#include "projectitem.h"
#include "kdenlivesettings.h"
#include "ui_colorclip_ui.h"

#include "definitions.h"
#include "titlewidget.h"

#include <QtGui>

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
	
	QAction *addTitleClip = addMenu->addAction (KIcon("document-new"), i18n("Add Title Clip"));
	connect(addTitleClip, SIGNAL(triggered()), this, SLOT(slotAddTitleClip()));

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
  //m_toolbar->setEnabled(false);

  searchView->setTreeWidget(listView);
  listView->setColumnCount(3);
  QStringList headers;
  headers<<i18n("Thumbnail")<<i18n("Filename")<<i18n("Description");
  listView->setHeaderLabels(headers);
  listView->sortByColumn(1, Qt::AscendingOrder);

  m_menu = new QMenu();	
  m_menu->addAction(addClipButton);
  m_menu->addAction(addColorClip);
  m_menu->addAction(addTitleClip);
  m_menu->addAction(m_editAction);
  m_menu->addAction(m_deleteAction);
  m_menu->addAction(addFolderButton);
  m_menu->insertSeparator(m_deleteAction);

  connect(listView, SIGNAL(itemSelectionChanged()), this, SLOT(slotClipSelected()));
  connect(listView, SIGNAL(requestMenu ( const QPoint &, QTreeWidgetItem * )), this, SLOT(slotContextMenu(const QPoint &, QTreeWidgetItem *)));
  connect(listView, SIGNAL(addClip ()), this, SLOT(slotAddClip()));
  connect(listView, SIGNAL(addClip (QUrl, const QString &)), this, SLOT(slotAddClip(QUrl, const QString &)));
  connect(listView, SIGNAL (itemChanged ( QTreeWidgetItem *, int )), this, SLOT(slotUpdateItemDescription(QTreeWidgetItem *, int )));

  m_listViewDelegate = new ItemDelegate(listView);
  listView->setItemDelegate(m_listViewDelegate);
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

void ProjectList::slotUpdateItemDescription( QTreeWidgetItem *item, int column)
{
  if (column != 2) return;
  ProjectItem *clip = (ProjectItem*) item;
  CLIPTYPE type = clip->clipType(); 
  if (type == AUDIO || type == VIDEO || type == AV || type == IMAGE || type == PLAYLIST) {
    // Use Nepomuk system to store clip description
    Nepomuk::Resource f( clip->clipUrl().path() );
    f.setDescription(item->text(2));
    kDebug()<<"NEPOMUK, SETTING CLIP: "<<clip->clipUrl().path()<<", TO TEXT: "<<item->text(2);
  }
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
    QFrame *w = new QFrame;
    w->setFrameShape(QFrame::StyledPanel);
    w->setLineWidth(2);
    w->setAutoFillBackground(true);
    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget( new QLabel(i18n("Color:")));
    layout->addWidget( new KColorButton());
    layout->addWidget( new QLabel(i18n("Duration:")));
    layout->addWidget( new KRestrictedLine());
    w->setLayout( layout );
    m_listViewDelegate->extendItem(w, listView->currentIndex());
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
  if (item->numReferences() > 0) {
    if (KMessageBox::questionYesNo(this, i18n("Delete clip <b>%1</b> ?<br>This will also remove its %2 clips in timeline").arg(item->names().at(1)).arg(item->numReferences()), i18n("Delete Clip")) != KMessageBox::Yes) return;
  }
  m_doc->deleteProjectClip(item->clipId());
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
    // if file has Nepomuk comment, use it
    Nepomuk::Resource f( url.path() );
    QString annotation = f.description();
    if (!annotation.isEmpty()) item->setText(2, annotation);
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

void ProjectList::slotDeleteClip( int clipId)
{
  ProjectItem *item = getItemById(clipId);
  if (item) delete item;
}

void ProjectList::slotAddFolder()
{
/*
  QString folderName = KInputDialog::getText(i18n("New Folder"), i18n("Enter new folder name: "));
  if (folderName.isEmpty()) return;
  QStringList itemEntry;
  itemEntry.append(QString::null);
  itemEntry.append(folderName);
  AddClipCommand *command = new AddClipCommand(this, itemEntry, QDomElement(), m_clipIdCounter++, KUrl(), folderName, true);
  m_commandStack->push(command);*/
}

void ProjectList::slotAddClip(DocClipBase *clip)
{
  ProjectItem *item = new ProjectItem(listView, clip);
  listView->setCurrentItem(item);
  emit getFileProperties(clip->toXML(), clip->getId());
}

void ProjectList::slotUpdateClip(int id)
{
  ProjectItem *item = getItemById(id);
  item->setData(1, UsageRole, QString::number(item->numReferences()));
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

  for (it = list.begin(); it != list.end(); it++) {
      m_doc->slotAddClipFile(*it, group);
  }
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
    QString color = dia_ui->clip_color->color().name();
    color = color.replace(0, 1, "0x") + "ff";
    m_doc->slotAddColorClipFile(dia_ui->clip_name->text(), color, dia_ui->clip_duration->text(), QString::null);
  }
  delete dia_ui;
  delete dia;
}

void ProjectList::slotAddTitleClip()
{

	if (!m_commandStack) kDebug()<<"!!!!!!!!!!!!!!!!  NO CMD STK";
	//QDialog *dia = new QDialog;
	
	TitleWidget *dia_ui = new TitleWidget();
	//dia_ui->setupUi(dia);
	//dia_ui->clip_name->setText(i18n("Title Clip"));
	//dia_ui->clip_duration->setText(KdenliveSettings::color_duration());
	if (dia_ui->exec() == QDialog::Accepted)
	{
		//QString color = dia_ui->clip_color->color().name();
		//color = color.replace(0, 1, "0x") + "ff";
		//m_doc->slotAddColorClipFile(dia_ui->clip_name->text(), color, dia_ui->clip_duration->text(), QString::null);
	}
	delete dia_ui;
	//delete dia;
}
void ProjectList::setDocument(KdenliveDoc *doc)
{
  m_fps = doc->fps();
  m_timecode = doc->timecode();
  m_commandStack = doc->commandStack();
  m_doc = doc;
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
  CLIPTYPE type = (CLIPTYPE) producer.attribute("type").toInt();

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
  if (type == AUDIO || type == VIDEO || type == AV || type == IMAGE  || type == PLAYLIST)
  {
    KUrl resource = KUrl(producer.attribute("resource"));
    if (!resource.isEmpty()) {
      QStringList itemEntry;
      itemEntry.append(QString::null);
      itemEntry.append(resource.fileName());
      addClip(itemEntry, producer, id, resource, groupName, parentId);  
    }
  }
  else if (type == COLOR) {
    QString colour = producer.attribute("colour");
    QPixmap pix(60, 40);
    colour = colour.replace(0, 2, "#");
    pix.fill(QColor(colour.left(7)));
    QStringList itemEntry;
    itemEntry.append(QString::null);
    itemEntry.append(producer.attribute("name", i18n("Color clip")));
    addClip(itemEntry, producer, id, KUrl(), groupName, parentId);
  }
      
}

#include "projectlist.moc"
