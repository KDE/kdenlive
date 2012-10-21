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


#include "collapsibleeffect.h"
#include "effectslist.h"
#include "kdenlivesettings.h"
#include "projectlist.h"

#include <QInputDialog>
#include <QDialog>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QWheelEvent>

#include <KDebug>
#include <KComboBox>
#include <KGlobalSettings>
#include <KLocale>
#include <KMessageBox>
#include <KStandardDirs>
#include <KFileDialog>
#include <KApplication>


CollapsibleEffect::CollapsibleEffect(QDomElement effect, QDomElement original_effect, ItemInfo info, EffectMetaInfo *metaInfo, bool lastEffect, QWidget * parent) :
        AbstractCollapsibleWidget(parent),
        m_paramWidget(NULL),
        m_effect(effect),
        m_original_effect(original_effect),
        m_lastEffect(lastEffect),
        m_regionEffect(false)
{
    if (m_effect.attribute("tag") == "region") {
	m_regionEffect = true;
	decoframe->setObjectName("decoframegroup");
    }
    filterWheelEvent = true;
    m_info.fromString(effect.attribute("kdenlive_info"));
    setFont(KGlobalSettings::smallestReadableFont());
    buttonUp->setIcon(KIcon("kdenlive-up"));
    buttonUp->setToolTip(i18n("Move effect up"));
    if (!lastEffect) {
        buttonDown->setIcon(KIcon("kdenlive-down"));
        buttonDown->setToolTip(i18n("Move effect down"));
    }
    buttonDel->setIcon(KIcon("kdenlive-deleffect"));
    buttonDel->setToolTip(i18n("Delete effect"));
    if (effectIndex() == 1) buttonUp->setVisible(false);
    if (m_lastEffect) buttonDown->setVisible(false);
    //buttonUp->setVisible(false);
    //buttonDown->setVisible(false);
    
    /*buttonReset->setIcon(KIcon("view-refresh"));
    buttonReset->setToolTip(i18n("Reset effect"));*/
    //checkAll->setToolTip(i18n("Enable/Disable all effects"));
    //buttonShowComments->setIcon(KIcon("help-about"));
    //buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));
    m_menu = new QMenu;
    m_menu->addAction(KIcon("view-refresh"), i18n("Reset Effect"), this, SLOT(slotResetEffect()));
    m_menu->addAction(KIcon("document-save"), i18n("Save Effect"), this, SLOT(slotSaveEffect()));
    
    QDomElement namenode = m_effect.firstChildElement("name");
    if (namenode.isNull()) return;
    QString effectname = i18n(namenode.text().toUtf8().data());
    if (m_regionEffect) effectname.append(':' + KUrl(EffectsList::parameter(m_effect, "resource")).fileName());
    
    QHBoxLayout *l = static_cast <QHBoxLayout *>(frame->layout());
    title = new QLabel(this);
    l->insertWidget(2, title);
    
    title->setText(effectname);
    /*
     * Do not show icon, makes too much visual noise
    QString type = m_effect.attribute("type", QString());
    KIcon icon;
    if (type == "audio") icon = KIcon("kdenlive-show-audio");
    else if (m_effect.attribute("tag") == "region") icon = KIcon("kdenlive-mask-effect");
    else if (type == "custom") icon = KIcon("kdenlive-custom-effect");
    else icon = KIcon("kdenlive-show-video");
    effecticon->setPixmap(icon.pixmap(16,16));*/
    m_groupAction = new QAction(KIcon("folder-new"), i18n("Create Group"), this);
    connect(m_groupAction, SIGNAL(triggered(bool)), this, SLOT(slotCreateGroup()));

    if (!m_regionEffect) {
	if (m_info.groupIndex == -1) m_menu->addAction(m_groupAction);
	m_menu->addAction(KIcon("folder-new"), i18n("Create Region"), this, SLOT(slotCreateRegion()));
    }
    setupWidget(info, metaInfo);
    setAcceptDrops(true);
    menuButton->setIcon(KIcon("kdenlive-menu"));
    menuButton->setMenu(m_menu);
    
    if (m_effect.attribute("disable") == "1") {
        title->setEnabled(false);
	enabledButton->setChecked(true);
	enabledButton->setIcon(KIcon("novisible"));
    }
    else {
        enabledButton->setChecked(false);
	enabledButton->setIcon(KIcon("visible"));
    }

    connect(collapseButton, SIGNAL(clicked()), this, SLOT(slotSwitch()));
    connect(enabledButton, SIGNAL(toggled(bool)), this, SLOT(slotDisable(bool)));
    connect(buttonUp, SIGNAL(clicked()), this, SLOT(slotEffectUp()));
    connect(buttonDown, SIGNAL(clicked()), this, SLOT(slotEffectDown()));
    connect(buttonDel, SIGNAL(clicked()), this, SLOT(slotDeleteEffect()));

    Q_FOREACH( QSpinBox * sp, findChildren<QSpinBox*>() ) {
        sp->installEventFilter( this );
        sp->setFocusPolicy( Qt::StrongFocus );
    }
    Q_FOREACH( KComboBox * cb, findChildren<KComboBox*>() ) {
	cb->installEventFilter( this );
        cb->setFocusPolicy( Qt::StrongFocus );
    }
    Q_FOREACH( QProgressBar * cb, findChildren<QProgressBar*>() ) {
	cb->installEventFilter( this );
        cb->setFocusPolicy( Qt::StrongFocus );
    }
}

CollapsibleEffect::~CollapsibleEffect()
{
    if (m_paramWidget) delete m_paramWidget;
    delete m_menu;
}

void CollapsibleEffect::slotCreateGroup()
{
    emit createGroup(effectIndex());
}

void CollapsibleEffect::slotCreateRegion()
{
    QString allExtensions = ProjectList::getExtensions();
    const QString dialogFilter = allExtensions + ' ' + QLatin1Char('|') + i18n("All Supported Files") + "\n* " + QLatin1Char('|') + i18n("All Files");
    QPointer<KFileDialog> d = new KFileDialog(KUrl("kfiledialog:///clipfolder"), dialogFilter, kapp->activeWindow());
    d->setOperationMode(KFileDialog::Opening);
    d->setMode(KFile::File);
    if (d->exec() == QDialog::Accepted) {
	KUrl url = d->selectedUrl();
	if (!url.isEmpty()) emit createRegion(effectIndex(), url);
    }
    delete d;
}

void CollapsibleEffect::slotUnGroup()
{
    emit unGroup(this);
}

bool CollapsibleEffect::eventFilter( QObject * o, QEvent * e ) 
{
    if (e->type() == QEvent::Enter) {
	frame->setProperty("mouseover", true);
	frame->setStyleSheet(frame->styleSheet());
	return QWidget::eventFilter(o, e);
    }
    if (e->type() == QEvent::Wheel) {
	QWheelEvent *we = static_cast<QWheelEvent *>(e);
	if (!filterWheelEvent || we->modifiers() != Qt::NoModifier) {
	    e->accept();
	    return false;
	}
	if (qobject_cast<QAbstractSpinBox*>(o)) {
	    if(qobject_cast<QAbstractSpinBox*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
	if (qobject_cast<KComboBox*>(o)) {
	    if(qobject_cast<KComboBox*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
	if (qobject_cast<QProgressBar*>(o)) {
	    if(qobject_cast<QProgressBar*>(o)->focusPolicy() == Qt::WheelFocus)
	    {
		e->accept();
		return false;
	    }
	    else
	    {
		e->ignore();
		return true;
	    }
	}
    }
    return QWidget::eventFilter(o, e);
}

QDomElement CollapsibleEffect::effect() const
{
    return m_effect;
}

bool CollapsibleEffect::isActive() const
{
    return decoframe->property("active").toBool();
}

void CollapsibleEffect::setActive(bool activate)
{
    decoframe->setProperty("active", activate);
    decoframe->setStyleSheet(decoframe->styleSheet());
}

void CollapsibleEffect::mouseDoubleClickEvent ( QMouseEvent * event )
{
    if (frame->underMouse() && collapseButton->isEnabled()) {
        event->accept();
        slotSwitch();
    }
    else event->ignore();
}

void CollapsibleEffect::mouseReleaseEvent( QMouseEvent *event )
{
  if (!decoframe->property("active").toBool()) emit activateEffect(effectIndex());
  QWidget::mouseReleaseEvent(event);
}

void CollapsibleEffect::slotDisable(bool disable, bool emitInfo)
{
    title->setEnabled(!disable);
    enabledButton->blockSignals(true);
    enabledButton->setChecked(disable);
    enabledButton->blockSignals(false);
    enabledButton->setIcon(disable ? KIcon("novisible") : KIcon("visible"));
    m_effect.setAttribute("disable", disable ? 1 : 0);
    if (!disable || KdenliveSettings::disable_effect_parameters()) {
        widgetFrame->setEnabled(!disable);
    }
    if (emitInfo) emit effectStateChanged(disable, effectIndex(), isActive() && needsMonitorEffectScene());
}

void CollapsibleEffect::slotDeleteEffect()
{
    emit deleteEffect(m_effect);
}

void CollapsibleEffect::slotEffectUp()
{
    emit changeEffectPosition(QList <int>() <<effectIndex(), true);
}

void CollapsibleEffect::slotEffectDown()
{
    emit changeEffectPosition(QList <int>() <<effectIndex(), false);
}

void CollapsibleEffect::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.isEmpty()) return;
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    path = path + name + ".xml";
    if (QFile::exists(path)) if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", path)) == KMessageBox::No) return;

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
    emit reloadEffects();
}

void CollapsibleEffect::slotResetEffect()
{
    emit resetEffect(effectIndex());
}

void CollapsibleEffect::slotSwitch()
{
    bool enable = !widgetFrame->isVisible();
    slotShow(enable);
}

void CollapsibleEffect::slotShow(bool show)
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
    updateCollapsedState();
}

void CollapsibleEffect::groupStateChanged(bool collapsed)
{
    m_info.groupIsCollapsed = collapsed;
    updateCollapsedState();
}

void CollapsibleEffect::updateCollapsedState()
{
    QString info = m_info.toString();
    if (info != m_effect.attribute("kdenlive_info")) {
	m_effect.setAttribute("kdenlive_info", info);
	emit parameterChanged(m_original_effect, m_effect, effectIndex());   
    }
}

void CollapsibleEffect::setGroupIndex(int ix)
{
    if (m_info.groupIndex == -1 && ix != -1) {
	m_menu->removeAction(m_groupAction); 
    }
    else if (m_info.groupIndex != -1 && ix == -1) {
	m_menu->addAction(m_groupAction); 
    }
    m_info.groupIndex = ix;
    m_effect.setAttribute("kdenlive_info", m_info.toString());
}

void CollapsibleEffect::setGroupName(const QString &groupName)
{
    m_info.groupName = groupName;
    m_effect.setAttribute("kdenlive_info", m_info.toString());
}

QString CollapsibleEffect::infoString() const
{
    return m_info.toString();
}

void CollapsibleEffect::removeFromGroup()
{
    if (m_info.groupIndex != -1) {
	m_menu->addAction(m_groupAction); 
    }
    m_info.groupIndex = -1;
    m_info.groupName.clear();
    m_effect.setAttribute("kdenlive_info", m_info.toString());
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

int CollapsibleEffect::groupIndex() const
{
    return m_info.groupIndex;
}

int CollapsibleEffect::effectIndex() const
{
    if (m_effect.isNull()) return -1;
    return m_effect.attribute("kdenlive_ix").toInt();
}

void CollapsibleEffect::updateWidget(ItemInfo info, QDomElement effect, EffectMetaInfo *metaInfo)
{
    if (m_paramWidget) {
        // cleanup
        delete m_paramWidget;
        m_paramWidget = NULL;
    }
    m_effect = effect;
    setupWidget(info, metaInfo);
}

void CollapsibleEffect::setupWidget(ItemInfo info, EffectMetaInfo *metaInfo)
{
    if (m_effect.isNull()) {
//         kDebug() << "// EMPTY EFFECT STACK";
        return;
    }

    if (m_effect.attribute("tag") == "region") {
	m_regionEffect = true;
        QDomNodeList effects =  m_effect.elementsByTagName("effect");
        QDomNodeList origin_effects =  m_original_effect.elementsByTagName("effect");
	m_paramWidget = new ParameterContainer(m_effect, info, metaInfo, widgetFrame);
        QWidget *container = new QWidget(widgetFrame);
	QVBoxLayout *vbox = static_cast<QVBoxLayout *> (widgetFrame->layout());
        vbox->addWidget(container);
       // m_paramWidget = new ParameterContainer(m_effect.toElement(), info, metaInfo, container);
        for (int i = 0; i < effects.count(); i++) {
            CollapsibleEffect *coll = new CollapsibleEffect(effects.at(i).toElement(), origin_effects.at(i).toElement(), info, metaInfo, container);
            m_subParamWidgets.append(coll);
	    connect(coll, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this , SLOT(slotUpdateRegionEffectParams(const QDomElement, const QDomElement, int)));
            //container = new QWidget(widgetFrame);
            vbox->addWidget(coll);
            //p = new ParameterContainer(effects.at(i).toElement(), info, isEffect, container);
        }
        
    }
    else {
        m_paramWidget = new ParameterContainer(m_effect, info, metaInfo, widgetFrame);
	connect(m_paramWidget, SIGNAL(disableCurrentFilter(bool)), this, SLOT(slotDisableEffect(bool)));
        if (m_effect.firstChildElement("parameter").isNull()) {
            // Effect has no parameter, don't allow expand
            collapseButton->setEnabled(false);
	    collapseButton->setVisible(false);
            widgetFrame->setVisible(false);            
        }
    }
    if (collapseButton->isEnabled() && m_info.isCollapsed) {
 	widgetFrame->setVisible(false);
	collapseButton->setArrowType(Qt::RightArrow);
	
    }
    connect (m_paramWidget, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)), this, SIGNAL(parameterChanged(const QDomElement, const QDomElement, int)));
    
    connect(m_paramWidget, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QStringList)), this, SIGNAL(startFilterJob(QString,QString,QString,QString,QString,QStringList)));
    
    connect (this, SIGNAL(syncEffectsPos(int)), m_paramWidget, SIGNAL(syncEffectsPos(int)));
    connect (m_paramWidget, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
    connect (m_paramWidget, SIGNAL(seekTimeline(int)), this, SIGNAL(seekTimeline(int)));
    connect(m_paramWidget, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
    
    
}

void CollapsibleEffect::slotDisableEffect(bool disable)
{
    title->setEnabled(!disable);
    enabledButton->blockSignals(true);
    enabledButton->setChecked(disable);
    enabledButton->blockSignals(false);
    enabledButton->setIcon(disable ? KIcon("novisible") : KIcon("visible"));
    m_effect.setAttribute("disable", disable ? 1 : 0);
    emit effectStateChanged(disable, effectIndex(), isActive() && needsMonitorEffectScene());
}

bool CollapsibleEffect::isGroup() const
{
    return false;
}

void CollapsibleEffect::updateTimecodeFormat()
{
    m_paramWidget->updateTimecodeFormat();
    if (!m_subParamWidgets.isEmpty()) {
        // we have a group
        for (int i = 0; i < m_subParamWidgets.count(); i++)
            m_subParamWidgets.at(i)->updateTimecodeFormat();
    }
}

void CollapsibleEffect::slotUpdateRegionEffectParams(const QDomElement /*old*/, const QDomElement /*e*/, int /*ix*/)
{
    kDebug()<<"// EMIT CHANGE SUBEFFECT.....:";
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

void CollapsibleEffect::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}

void CollapsibleEffect::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
	frame->setProperty("target", true);
	frame->setStyleSheet(frame->styleSheet());
	event->acceptProposedAction();
    }
}

void CollapsibleEffect::dragLeaveEvent(QDragLeaveEvent */*event*/)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
}

void CollapsibleEffect::dropEvent(QDropEvent *event)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
    const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    int ix = e.attribute("kdenlive_ix").toInt();
    int currentEffectIx = effectIndex();
    if (ix == currentEffectIx) {
	// effect dropped on itself, reject
	event->ignore();
	return;
    }
    if (ix == 0 || e.tagName() == "effectgroup") {
	if (e.tagName() == "effectgroup") {
	    // moving a group
	    QDomNodeList subeffects = e.elementsByTagName("effect");
	    if (subeffects.isEmpty()) {
		event->ignore();
		return;
	    }
	    EffectInfo info;
	    info.fromString(subeffects.at(0).toElement().attribute("kdenlive_info"));
	    event->setDropAction(Qt::MoveAction);
	    event->accept();
	    if (info.groupIndex >= 0) {
		// Moving group
		QList <int> effectsIds;
		// Collect moved effects ids
		for (int i = 0; i < subeffects.count(); i++) {
		    QDomElement effect = subeffects.at(i).toElement();
		    effectsIds << effect.attribute("kdenlive_ix").toInt();
		}
		emit moveEffect(effectsIds, currentEffectIx, info.groupIndex, info.groupName);
	    }
	    else {
		// group effect dropped from effect list
		if (m_info.groupIndex > -1) {
		    // TODO: Should we merge groups??
		    
		}
		emit addEffect(e);
	    }
	    return;
	}
	// effect dropped from effects list, add it
	e.setAttribute("kdenlive_ix", ix);
	if (m_info.groupIndex > -1) {
	    // Dropped on a group
	    e.setAttribute("kdenlive_info", m_info.toString());
	}
	event->setDropAction(Qt::CopyAction);
	event->accept();
	emit addEffect(e);
	return;
    }
    emit moveEffect(QList <int> () <<ix, currentEffectIx, m_info.groupIndex, m_info.groupName);
    event->setDropAction(Qt::MoveAction);
    event->accept();
}


void CollapsibleEffect::adjustButtons(int ix, int max)
{
    buttonUp->setVisible(ix > 0);
    buttonDown->setVisible(ix < max - 1);
}

bool CollapsibleEffect::needsMonitorEffectScene() const
{
    return m_paramWidget->needsMonitorEffectScene();
}


