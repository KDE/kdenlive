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
#include "utils/KoIconUtils.h"

#include <QMenu>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMutexLocker>
#include <QDir>
#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QStandardPaths>
#include <QMimeData>

#include <klocalizedstring.h>
#include <KMessageBox>
#include <KColorScheme>
#include <KDualAction>

MyEditableLabel::MyEditableLabel(QWidget *parent):
    QLineEdit(parent)
{
    setFrame(false);
    setReadOnly(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void MyEditableLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    setReadOnly(false);
    selectAll();
    e->accept();
}

CollapsibleGroup::CollapsibleGroup(int ix, bool firstGroup, bool lastGroup, const EffectInfo &info, QWidget *parent) :
    AbstractCollapsibleWidget(parent)
{
    m_info.groupIndex = ix;
    m_subWidgets = QList<CollapsibleEffect *> ();
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    frame->setObjectName(QStringLiteral("framegroup"));
    decoframe->setObjectName(QStringLiteral("decoframegroup"));
    QHBoxLayout *l = static_cast <QHBoxLayout *>(frame->layout());
    m_title = new MyEditableLabel(this);
    l->insertWidget(2, m_title);
    m_title->setText(info.groupName.isEmpty() ? i18n("Effect Group") : info.groupName);
    m_info.groupName = m_title->text();
    connect(m_title, &QLineEdit::editingFinished, this, &CollapsibleGroup::slotRenameGroup);
    buttonUp->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-up")));
    buttonUp->setToolTip(i18n("Move effect up"));
    buttonDown->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-down")));
    buttonDown->setToolTip(i18n("Move effect down"));

    buttonDel->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-deleffect")));
    buttonDel->setToolTip(i18n("Delete effect"));
    if (firstGroup) {
        buttonUp->setVisible(false);
    }
    if (lastGroup) {
        buttonDown->setVisible(false);
    }
    m_menu = new QMenu;
    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("view-refresh")), i18n("Reset Group"), this, SLOT(slotResetGroup()));
    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("document-save")), i18n("Save Group"), this, SLOT(slotSaveGroup()));

    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("list-remove")), i18n("Ungroup"), this, SLOT(slotUnGroup()));
    setAcceptDrops(true);
    menuButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-menu")));
    menuButton->setMenu(m_menu);

    m_enabledButton = new KDualAction(i18n("Disable Effect"), i18n("Enable Effect"), this);
    m_enabledButton->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("hint")));
    m_enabledButton->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("visibility")));
    enabledButton->setDefaultAction(m_enabledButton);

    if (info.groupIsCollapsed) {
        slotShow(false);
    }

    connect(collapseButton, &QAbstractButton::clicked, this, &CollapsibleGroup::slotSwitch);
    connect(m_enabledButton, SIGNAL(activeChangedByUser(bool)), this, SLOT(slotEnable(bool)));
    connect(buttonUp, &QAbstractButton::clicked, this, &CollapsibleGroup::slotEffectUp);
    connect(buttonDown, &QAbstractButton::clicked, this, &CollapsibleGroup::slotEffectDown);
    connect(buttonDel, &QAbstractButton::clicked, this, &CollapsibleGroup::slotDeleteGroup);

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
    return decoframe->property("active").toBool();
}

void CollapsibleGroup::setActive(bool activate)
{
    decoframe->setProperty("active", activate);
    decoframe->setStyleSheet(decoframe->styleSheet());
}

void CollapsibleGroup::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (frame->underMouse() && collapseButton->isEnabled()) {
        slotSwitch();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CollapsibleGroup::slotEnable(bool disable, bool emitInfo)
{
    m_title->setEnabled(!disable);
    for (int i = 0; i < m_subWidgets.count(); ++i) {
        m_subWidgets.at(i)->slotDisable(disable, emitInfo);
    }
}

void CollapsibleGroup::slotDeleteGroup()
{
    QDomDocument doc;
    // delete effects from the last one to the first, otherwise each deletion would trigger an update
    // in other effects's kdenlive_ix index.
    for (int i = m_subWidgets.count() - 1; i >= 0; --i) {
        doc.appendChild(doc.importNode(m_subWidgets.at(i)->effect(), true));
    }
    doc.documentElement().setAttribute(QStringLiteral("name"), m_info.groupName);
    emit deleteGroup(doc);
}

void CollapsibleGroup::slotEffectUp()
{
    QList<int> indexes;
    indexes.reserve(m_subWidgets.count());
    for (int i = 0; i < m_subWidgets.count(); ++i) {
        indexes << m_subWidgets.at(i)->effectIndex();
    }
    emit changeEffectPosition(indexes, true);
}

void CollapsibleGroup::slotEffectDown()
{
    QList<int> indexes;
    indexes.reserve(m_subWidgets.count());
    for (int i = 0; i < m_subWidgets.count(); ++i) {
        indexes << m_subWidgets.at(i)->effectIndex();
    }
    emit changeEffectPosition(indexes, false);
}

void CollapsibleGroup::slotSaveGroup()
{
    QString name = QInputDialog::getText(this, i18n("Save Group"), i18n("Name for saved group: "), QLineEdit::Normal, m_title->text());
    if (name.isEmpty()) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (dir.exists(name + QStringLiteral(".xml"))) {
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", name + QStringLiteral(".xml"))) == KMessageBox::No) {
            return;
        }
    }

    QDomDocument doc = effectsData();
    QDomElement base = doc.documentElement();
    QDomNodeList effects = base.elementsByTagName(QStringLiteral("effect"));
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement eff = effects.at(i).toElement();
        eff.removeAttribute(QStringLiteral("kdenlive_ix"));
        EffectInfo info;
        info.fromString(eff.attribute(QStringLiteral("kdenlive_info")));
        // Make sure all effects have the correct new group name
        info.groupName = name;
        // Saved effect group should have a group index of -1
        info.groupIndex = -1;
        eff.setAttribute(QStringLiteral("kdenlive_info"), info.toString());

    }

    base.setAttribute(QStringLiteral("name"), name);
    base.setAttribute(QStringLiteral("id"), name);
    base.setAttribute(QStringLiteral("type"), QStringLiteral("custom"));

    QFile file(dir.absoluteFilePath(name + QStringLiteral(".xml")));
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&file);
        out << doc.toString();
    }
    file.close();
    emit reloadEffects();
}

void CollapsibleGroup::slotResetGroup()
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_subWidgets.count(); ++i) {
        m_subWidgets.at(i)->slotResetEffect();
    }
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
        m_info.groupIsCollapsed = false;
    } else {
        collapseButton->setArrowType(Qt::RightArrow);
        m_info.groupIsCollapsed = true;
    }
    if (!m_subWidgets.isEmpty()) {
        m_subWidgets.at(0)->groupStateChanged(m_info.groupIsCollapsed);
    }
}

QWidget *CollapsibleGroup::title() const
{
    return m_title;
}

void CollapsibleGroup::addGroupEffect(CollapsibleEffect *effect)
{
    QMutexLocker lock(&m_mutex);
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == nullptr) {
        vbox = new QVBoxLayout();
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(2);
        widgetFrame->setLayout(vbox);
    }
    effect->setGroupIndex(groupIndex());
    effect->setGroupName(m_title->text());
    effect->decoframe->setObjectName(QStringLiteral("decoframesub"));
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
    if (vbox == nullptr) {
        return;
    }
    for (int i = m_subWidgets.count() - 1; i >= 0; --i) {
        vbox->removeWidget(m_subWidgets.at(i));
        layout->insertWidget(ix, m_subWidgets.at(i));
        m_subWidgets.at(i)->decoframe->setObjectName(QStringLiteral("decoframe"));
        m_subWidgets.at(i)->removeFromGroup();
    }
    m_subWidgets.clear();
}

int CollapsibleGroup::groupIndex() const
{
    return m_info.groupIndex;
}

bool CollapsibleGroup::isGroup() const
{
    return true;
}

void CollapsibleGroup::updateTimecodeFormat()
{
    QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
    if (vbox == nullptr) {
        return;
    }
    for (int j = vbox->count() - 1; j >= 0; --j) {
        CollapsibleEffect *e = static_cast<CollapsibleEffect *>(vbox->itemAt(j)->widget());
        if (e) {
            e->updateTimecodeFormat();
        }
    }
}

void CollapsibleGroup::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        frame->setProperty("target", true);
        frame->setStyleSheet(frame->styleSheet());
        event->acceptProposedAction();
    }
}

void CollapsibleGroup::dragLeaveEvent(QDragLeaveEvent */*event*/)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
}

void CollapsibleGroup::dropEvent(QDropEvent *event)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
    const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    int ix = e.attribute(QStringLiteral("kdenlive_ix")).toInt();
    if (ix == 0 || e.tagName() == QLatin1String("effectgroup")) {
        if (e.tagName() == QLatin1String("effectgroup")) {
            // dropped a group on another group
            QDomNodeList pastedEffects = e.elementsByTagName(QStringLiteral("effect"));
            if (pastedEffects.isEmpty() || m_subWidgets.isEmpty()) {
                // Buggy groups, should not happen
                event->ignore();
                return;
            }
            QList<int> pastedEffectIndexes;
            QList<int> currentEffectIndexes;
            EffectInfo pasteInfo;
            pasteInfo.fromString(pastedEffects.at(0).toElement().attribute(QStringLiteral("kdenlive_info")));
            if (pasteInfo.groupIndex == -1) {
                // Group dropped from effects list, add effect
                e.setAttribute(QStringLiteral("kdenlive_ix"), m_subWidgets.last()->effectIndex());
                emit addEffect(e);
                event->setDropAction(Qt::CopyAction);
                event->accept();
                return;
            }
            // Moving group
            for (int i = 0; i < pastedEffects.count(); ++i) {
                pastedEffectIndexes << pastedEffects.at(i).toElement().attribute(QStringLiteral("kdenlive_ix")).toInt();
            }
            for (int i = 0; i < m_subWidgets.count(); ++i) {
                currentEffectIndexes << m_subWidgets.at(i)->effectIndex();
            }
            //qCDebug(KDENLIVE_LOG)<<"PASTING: "<<pastedEffectIndexes<<" TO "<<currentEffectIndexes;
            if (pastedEffectIndexes.at(0) < currentEffectIndexes.at(0)) {
                // Pasting group after current one:
                emit moveEffect(pastedEffectIndexes, currentEffectIndexes.last(), pasteInfo.groupIndex, pasteInfo.groupName);
            } else {
                // Group moved before current one
                emit moveEffect(pastedEffectIndexes, currentEffectIndexes.first(), pasteInfo.groupIndex, pasteInfo.groupName);
            }
            event->setDropAction(Qt::MoveAction);
            event->accept();
            return;
        }
        // effect dropped from effects list, add it
        e.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
        if (!m_subWidgets.isEmpty()) {
            e.setAttribute(QStringLiteral("kdenlive_ix"), m_subWidgets.at(0)->effectIndex());
        }
        emit addEffect(e);
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }
    if (m_subWidgets.isEmpty()) {
        return;
    }
    int new_index = m_subWidgets.last()->effectIndex();
    emit moveEffect(QList<int> () << ix, new_index, m_info.groupIndex, m_title->text());
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void CollapsibleGroup::slotRenameGroup()
{
    m_title->setReadOnly(true);
    if (m_title->text().isEmpty()) {
        m_title->setText(i18n("Effect Group"));
    }
    for (int j = 0; j < m_subWidgets.count(); ++j) {
        m_subWidgets.at(j)->setGroupName(m_title->text());
    }
    m_info.groupName = m_title->text();
    emit groupRenamed(this);
}

QList<CollapsibleEffect *> CollapsibleGroup::effects()
{
    QMutexLocker lock(&m_mutex);
    return m_subWidgets;
}

QDomDocument CollapsibleGroup::effectsData()
{
    QMutexLocker lock(&m_mutex);
    QDomDocument doc;
    QDomElement list = doc.createElement(QStringLiteral("effectgroup"));
    list.setAttribute(QStringLiteral("name"), m_title->text());
    doc.appendChild(list);
    for (int j = 0; j < m_subWidgets.count(); ++j) {
        list.appendChild(doc.importNode(m_subWidgets.at(j)->effectForSave(), true));
    }
    return doc;
}

void CollapsibleGroup::adjustEffects()
{
    for (int i = 0; i < m_subWidgets.count(); ++i) {
        if (i > 0 && !m_subWidgets.at(i - 1)->isMovable()) {
            m_subWidgets.at(i)->adjustButtons(0, 0);
        } else {
            m_subWidgets.at(i)->adjustButtons(i, m_subWidgets.count());
        }
    }
}

