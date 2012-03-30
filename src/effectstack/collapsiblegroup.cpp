/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "collapsiblegroup.h"


#include <QMenu>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMutexLocker>

#include <KDebug>
#include <KGlobalSettings>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include <KFileDialog>
#include <KUrlRequester>
#include <KColorScheme>

MyEditableLabel::MyEditableLabel(QWidget * parent):
    QLineEdit(parent)
{
    setFrame(false);
    setReadOnly(true);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
}

void MyEditableLabel::mouseDoubleClickEvent ( QMouseEvent * e )
{
    setReadOnly(false);
    selectAll();
    e->accept();
}


CollapsibleGroup::CollapsibleGroup(int ix, bool firstGroup, bool lastGroup, QString groupName, QWidget * parent) :
        AbstractCollapsibleWidget(parent),
        m_index(ix)
{
    setupUi(this);
    m_subWidgets = QList <CollapsibleEffect *> ();
    setFont(KGlobalSettings::smallestReadableFont());
    QHBoxLayout *l = static_cast <QHBoxLayout *>(framegroup->layout());
    m_title = new MyEditableLabel(this);
    l->insertWidget(4, m_title);
    m_title->setText(groupName.isEmpty() ? i18n("Effect Group") : groupName);
    connect(m_title, SIGNAL(editingFinished()), this, SLOT(slotRenameGroup()));
    buttonUp->setIcon(KIcon("kdenlive-up"));
    buttonUp->setToolTip(i18n("Move effect up"));
    buttonDown->setIcon(KIcon("kdenlive-down"));
    buttonDown->setToolTip(i18n("Move effect down"));

    buttonDel->setIcon(KIcon("kdenlive-deleffect"));
    buttonDel->setToolTip(i18n("Delete effect"));
    if (firstGroup) buttonUp->setVisible(false);
    if (lastGroup) buttonDown->setVisible(false);
    m_menu = new QMenu;
    m_menu->addAction(KIcon("view-refresh"), i18n("Reset effect"), this, SLOT(slotResetEffect()));
    m_menu->addAction(KIcon("document-save"), i18n("Save effect"), this, SLOT(slotSaveEffect()));
    
    effecticon->setPixmap(KIcon("folder").pixmap(16,16));
    m_menu->addAction(KIcon("list-remove"), i18n("Ungroup"), this, SLOT(slotUnGroup()));
    setAcceptDrops(true);
    menuButton->setIcon(KIcon("kdenlive-menu"));
    menuButton->setMenu(m_menu);
    
    enabledBox->setChecked(true);

    connect(collapseButton, SIGNAL(clicked()), this, SLOT(slotSwitch()));
    connect(enabledBox, SIGNAL(toggled(bool)), this, SLOT(slotEnable(bool)));
    connect(buttonUp, SIGNAL(clicked()), this, SLOT(slotEffectUp()));
    connect(buttonDown, SIGNAL(clicked()), this, SLOT(slotEffectDown()));
    connect(buttonDel, SIGNAL(clicked()), this, SLOT(slotDeleteEffect()));

}

CollapsibleGroup::~CollapsibleGroup()
{
    delete m_menu;
}

void CollapsibleGroup::slotUnGroup()
{
    emit unGroup(this);
}

bool CollapsibleGroup::isActive() const
{
    return decoframegroup->property("active").toBool();
}

void CollapsibleGroup::setActive(bool activate)
{
    decoframegroup->setProperty("active", activate);
    decoframegroup->setStyleSheet(decoframegroup->styleSheet());
}

void CollapsibleGroup::mouseDoubleClickEvent ( QMouseEvent * event )
{
    if (framegroup->underMouse() && collapseButton->isEnabled()) slotSwitch();
    QWidget::mouseDoubleClickEvent(event);
}


void CollapsibleGroup::slotEnable(bool enable)
{
    m_title->setEnabled(enable);
    enabledBox->blockSignals(true);
    enabledBox->setChecked(enable);
    enabledBox->blockSignals(false);
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == NULL) return;
    for (int i = 0; i < vbox->count(); i++) {
	CollapsibleGroup *e = static_cast<CollapsibleGroup *>(vbox->itemAt(i)->widget());
	if (e) e->enabledBox->setChecked(enable);// slotEnable(enable);
    }
}

void CollapsibleGroup::slotDeleteEffect()
{
    emit deleteGroup(groupIndex());
}

void CollapsibleGroup::slotEffectUp()
{
    emit changeGroupPosition(groupIndex(), true);
}

void CollapsibleGroup::slotEffectDown()
{
    emit changeGroupPosition(groupIndex(), false);
}

void CollapsibleGroup::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.isEmpty()) return;
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    path = path + name + ".xml";
    if (QFile::exists(path)) if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", path)) == KMessageBox::No) return;

    /*TODO
    QDomDocument doc;
    QDomElement effect = m_effect.cloneNode().toElement();
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute("kdenlive_ix");
    effect.setAttribute("id", name);
    effect.setAttribute("type", "custom");
    QDomElement effectname = effect.firstChildElement("name");
    effect.removeChild(effectname);
    effectname = doc.createElement("name");
    QDomText nametext = doc.createTextNode(name);
    effectname.appendChild(nametext);
    effect.insertBefore(effectname, QDomNode());
    QDomElement effectprops = effect.firstChildElement("properties");
    effectprops.setAttribute("id", name);
    effectprops.setAttribute("type", "custom");

    QFile file(path);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << doc.toString();
    }
    file.close();
    emit reloadEffects();*/
}

void CollapsibleGroup::slotResetEffect()
{
    //TODO: emit resetEffect(effectIndex());
}

void CollapsibleGroup::slotSwitch()
{
    bool enable = !widgetFrame->isVisible();
    slotShow(enable);
}

void CollapsibleGroup::slotShow(bool show)
{
    widgetFrame->setVisible(show);
    if (show) {
        collapseButton->setArrowType(Qt::DownArrow);
	m_info.isCollapsed = false;
    }
    else {
        collapseButton->setArrowType(Qt::RightArrow);
	m_info.isCollapsed = true;
    }
    //emit parameterChanged(m_original_effect, m_effect, effectIndex());   
}

QWidget *CollapsibleGroup::title() const
{
    return m_title;
}

void CollapsibleGroup::addGroupEffect(CollapsibleEffect *effect)
{
    QMutexLocker lock(&m_mutex);
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == NULL) {
	vbox = new QVBoxLayout();
	vbox->setContentsMargins(0, 0, 0, 0);
	vbox->setSpacing(2);
	widgetFrame->setLayout(vbox);
    }
    effect->setGroupIndex(groupIndex());
    effect->setGroupName(m_title->text());
    m_subWidgets.append(effect);
    vbox->addWidget(effect);
}

QString CollapsibleGroup::infoString() const
{
    return m_info.toString();
}

void CollapsibleGroup::removeGroup(int ix, QVBoxLayout *layout)
{
    QMutexLocker lock(&m_mutex);
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == NULL) return;
    for (int i = m_subWidgets.count() - 1; i >= 0 ; i--) {
	vbox->removeWidget(m_subWidgets.at(i));
	layout->insertWidget(ix, m_subWidgets.at(i));
        m_subWidgets.at(i)->removeFromGroup();
	kDebug()<<"// Removing effect at: "<<i;
    }
    m_subWidgets.clear();
}

int CollapsibleGroup::groupIndex() const
{
    return m_index;
}

bool CollapsibleGroup::isGroup() const
{
    return true;
}

void CollapsibleGroup::updateTimecodeFormat()
{
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == NULL) return;
    for (int j = vbox->count() - 1; j >= 0; j--) {
	CollapsibleEffect *e = static_cast<CollapsibleEffect *>(vbox->itemAt(j)->widget());
	if (e) e->updateTimecodeFormat();
    }
}

void CollapsibleGroup::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
	framegroup->setProperty("active", true);
	framegroup->setStyleSheet(framegroup->styleSheet());
	event->acceptProposedAction();
    }
}

void CollapsibleGroup::dragLeaveEvent(QDragLeaveEvent */*event*/)
{
    framegroup->setProperty("active", false);
    framegroup->setStyleSheet(framegroup->styleSheet());
}

void CollapsibleGroup::dropEvent(QDropEvent *event)
{
    QMutexLocker lock(&m_mutex);
    framegroup->setProperty("active", false);
    framegroup->setStyleSheet(framegroup->styleSheet());
    const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    int ix = e.attribute("kdenlive_ix").toInt();
    if (ix == 0) {
	// effect dropped from effects list, add it
	e.setAttribute("kdenlive_ix", ix);
	event->setDropAction(Qt::CopyAction);
	event->accept();
	emit addEffect(e);
	return;
    }
    if (m_subWidgets.isEmpty()) return;
    int new_index = m_subWidgets.at(m_subWidgets.count() - 1)->effectIndex();
    emit moveEffect(ix, new_index, m_index, m_title->text());
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void CollapsibleGroup::slotRenameGroup()
{
    QMutexLocker lock(&m_mutex);
    m_title->setReadOnly(true);
    if (m_title->text().isEmpty()) m_title->setText(i18n("Effect Group"));
    for (int j = 0; j < m_subWidgets.count(); j++) {
	m_subWidgets.at(j)->setGroupName(m_title->text());
    }
    emit groupRenamed(this);
}

QList <CollapsibleEffect*> CollapsibleGroup::effects()
{
    QMutexLocker lock(&m_mutex);
    return m_subWidgets;
}

QDomDocument CollapsibleGroup::effectsData()
{
    QMutexLocker lock(&m_mutex);
    QDomDocument doc;
    QDomElement list = doc.createElement("list");
    list.setAttribute("name", m_title->text());
    doc.appendChild(list);
    for (int j = 0; j < m_subWidgets.count(); j++) {
	list.appendChild(doc.importNode(m_subWidgets.at(j)->effect(), true));
    }
    return doc;
}

