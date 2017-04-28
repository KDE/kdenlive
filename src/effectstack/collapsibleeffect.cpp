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
#include "effectslist/effectslist.h"
#include "kdenlivesettings.h"
#include "mltcontroller/effectscontroller.h"
#include "utils/KoIconUtils.h"
#include "dialogs/clipcreationdialog.h"

#include <QInputDialog>
#include <QDialog>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QWheelEvent>
#include <QFontDatabase>
#include <QFileDialog>
#include "kdenlive_debug.h"
#include <QStandardPaths>
#include <QPainter>
#include <QTimeLine>
#include <QMimeData>

#include <KRecentDirs>
#include <KComboBox>
#include <klocalizedstring.h>
#include <KMessageBox>
#include <KDualAction>

CollapsibleEffect::CollapsibleEffect(const QDomElement &effect, const QDomElement &original_effect, const ItemInfo &info, EffectMetaInfo *metaInfo, bool canMoveUp, bool lastEffect, QWidget *parent) :
    AbstractCollapsibleWidget(parent),
    m_paramWidget(nullptr),
    m_effect(effect),
    m_itemInfo(info),
    m_original_effect(original_effect),
    m_isMovable(true),
    m_animation(nullptr),
    m_regionEffect(false)
{
    if (m_effect.attribute(QStringLiteral("tag")) == QLatin1String("region")) {
        m_regionEffect = true;
        decoframe->setObjectName(QStringLiteral("decoframegroup"));
    }
    filterWheelEvent = true;
    m_info.fromString(effect.attribute(QStringLiteral("kdenlive_info")));
    //setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    buttonUp->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-up")));
    QSize iconSize = buttonUp->iconSize();
    buttonUp->setMaximumSize(iconSize);
    buttonDown->setMaximumSize(iconSize);
    menuButton->setMaximumSize(iconSize);
    enabledButton->setMaximumSize(iconSize);
    buttonDel->setMaximumSize(iconSize);
    buttonUp->setToolTip(i18n("Move effect up"));
    buttonDown->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-down")));
    buttonDown->setToolTip(i18n("Move effect down"));
    buttonDel->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-deleffect")));
    buttonDel->setToolTip(i18n("Delete effect"));
    buttonUp->setEnabled(canMoveUp);
    buttonDown->setEnabled(!lastEffect);

    if (m_effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
        // Speed effect is a "pseudo" effect, cannot be moved
        buttonUp->setVisible(false);
        buttonDown->setVisible(false);
        m_isMovable = false;
        setAcceptDrops(false);
    } else {
        setAcceptDrops(true);
    }

    /*buttonReset->setIcon(KoIconUtils::themedIcon("view-refresh"));
    buttonReset->setToolTip(i18n("Reset effect"));*/
    //checkAll->setToolTip(i18n("Enable/Disable all effects"));
    //buttonShowComments->setIcon(KoIconUtils::themedIcon("help-about"));
    //buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));
    m_menu = new QMenu(this);
    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("view-refresh")), i18n("Reset Effect"), this, SLOT(slotResetEffect()));
    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("document-save")), i18n("Save Effect"), this, SLOT(slotSaveEffect()));

    QHBoxLayout *l = static_cast <QHBoxLayout *>(frame->layout());
    m_colorIcon = new QLabel(this);
    l->insertWidget(0, m_colorIcon);
    m_colorIcon->setMinimumSize(iconSize);
    title = new QLabel(this);
    l->insertWidget(2, title);

    m_enabledButton = new KDualAction(i18n("Disable Effect"), i18n("Enable Effect"), this);
    m_enabledButton->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("hint")));
    m_enabledButton->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("visibility")));
    enabledButton->setDefaultAction(m_enabledButton);

    m_groupAction = new QAction(KoIconUtils::themedIcon(QStringLiteral("folder-new")), i18n("Create Group"), this);
    connect(m_groupAction, &QAction::triggered, this, &CollapsibleEffect::slotCreateGroup);

    QDomElement namenode = m_effect.firstChildElement(QStringLiteral("name"));
    if (namenode.isNull()) {
        // Warning, broken effect?
        //qCDebug(KDENLIVE_LOG)<<"// Could not create effect";
        return;
    }
    QString effectname = namenode.text().isEmpty() ? QString() : i18n(namenode.text().toUtf8().data());
    if (m_regionEffect) {
        effectname.append(':' + QUrl(EffectsList::parameter(m_effect, QStringLiteral("resource"))).fileName());
    }

    // Create color thumb
    QPixmap pix(iconSize);
    QColor col(m_effect.attribute(QStringLiteral("effectcolor")));
    QFont ft = font();
    ft.setBold(true);
    bool isAudio = m_effect.attribute(QStringLiteral("type")) == QLatin1String("audio");
    if (isAudio) {
        pix.fill(Qt::transparent);
    } else {
        pix.fill(col);
    }
    QPainter p(&pix);
    if (isAudio) {
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(pix.rect());
        p.setPen(QPen());
    }
    p.setFont(ft);
    p.drawText(pix.rect(), Qt::AlignCenter, effectname.at(0));
    p.end();
    m_iconPix = pix;
    m_colorIcon->setPixmap(pix);
    title->setText(effectname);

    if (!m_regionEffect) {
        if (m_info.groupIndex == -1) {
            m_menu->addAction(m_groupAction);
        }
        m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("folder-new")), i18n("Create Region"), this, SLOT(slotCreateRegion()));
    }
    setupWidget(info, metaInfo);
    menuButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-menu")));
    menuButton->setMenu(m_menu);

    if (m_effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
        title->setEnabled(false);
        m_enabledButton->setActive(true);
    } else {
        m_enabledButton->setActive(false);
    }

    connect(collapseButton, &QAbstractButton::clicked, this, &CollapsibleEffect::slotSwitch);
    connect(m_enabledButton, SIGNAL(activeChangedByUser(bool)), this, SLOT(slotDisable(bool)));
    connect(buttonUp, &QAbstractButton::clicked, this, &CollapsibleEffect::slotEffectUp);
    connect(buttonDown, &QAbstractButton::clicked, this, &CollapsibleEffect::slotEffectDown);
    connect(buttonDel, &QAbstractButton::clicked, this, &CollapsibleEffect::slotDeleteEffect);

    Q_FOREACH (QSpinBox *sp, findChildren<QSpinBox *>()) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(Qt::StrongFocus);
    }
    Q_FOREACH (KComboBox *cb, findChildren<KComboBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    Q_FOREACH (QProgressBar *cb, findChildren<QProgressBar *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    m_animation = new QTimeLine(200, this); //duration matches to match kmessagewidget
    connect(m_animation, &QTimeLine::valueChanged, this, &CollapsibleEffect::setWidgetHeight);
    connect(m_animation, &QTimeLine::stateChanged, this, [this](QTimeLine::State state) {
        if (state == QTimeLine::NotRunning) {
            //when collapsed hide contents to save resources and more importantly get it out the focus chain
            //slotShow(widgetFrame->height() > 0);
            widgetFrame->setVisible(widgetFrame->height() > 0);
        }
    });
}

CollapsibleEffect::~CollapsibleEffect()
{
    delete m_animation;
    delete m_paramWidget;
    delete m_menu;
}

void CollapsibleEffect::setWidgetHeight(qreal value)
{
    widgetFrame->setFixedHeight(m_paramWidget->contentHeight() * value);
}

void CollapsibleEffect::slotCreateGroup()
{
    emit createGroup(effectIndex());
}

void CollapsibleEffect::slotCreateRegion()
{
    QString allExtensions = ClipCreationDialog::getExtensions().join(QLatin1Char(' '));
    const QString dialogFilter = allExtensions + QLatin1Char(' ') + QLatin1Char('|') + i18n("All Supported Files") + QStringLiteral("\n* ") + QLatin1Char('|') + i18n("All Files");
    QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QPointer<QFileDialog> d = new QFileDialog(QApplication::activeWindow(), QString(), clipFolder, dialogFilter);
    d->setFileMode(QFileDialog::ExistingFile);
    if (d->exec() == QDialog::Accepted && !d->selectedUrls().isEmpty()) {
        KRecentDirs::add(QStringLiteral(":KdenliveClipFolder"), d->selectedUrls().first().adjusted(QUrl::RemoveFilename).toLocalFile());
        emit createRegion(effectIndex(), d->selectedUrls().first());
    }
    delete d;
}

void CollapsibleEffect::slotUnGroup()
{
    emit unGroup(this);
}

bool CollapsibleEffect::eventFilter(QObject *o, QEvent *e)
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
        if (qobject_cast<QAbstractSpinBox *>(o)) {
            if (qobject_cast<QAbstractSpinBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
                e->ignore();
                return true;
            }
        }
        if (qobject_cast<KComboBox *>(o)) {
            if (qobject_cast<KComboBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
                e->ignore();
                return true;
            }
        }
        if (qobject_cast<QProgressBar *>(o)) {
            if (qobject_cast<QProgressBar *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            } else {
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

QDomElement CollapsibleEffect::effectForSave() const
{
    QDomElement effect = m_effect.cloneNode().toElement();
    effect.removeAttribute(QStringLiteral("kdenlive_ix"));
    if (m_paramWidget) {
        int in = m_paramWidget->range().x();
        EffectsController::offsetKeyframes(in, effect);
    }
    return effect;
}

bool CollapsibleEffect::isActive() const
{
    return decoframe->property("active").toBool();
}

bool CollapsibleEffect::isEnabled() const
{
    return m_enabledButton->isActive();
}

void CollapsibleEffect::setActive(bool activate)
{
    decoframe->setProperty("active", activate);
    decoframe->setStyleSheet(decoframe->styleSheet());
    if (m_paramWidget) {
        m_paramWidget->connectMonitor(activate);
    }
    if (activate) {
        m_colorIcon->setPixmap(m_iconPix);
    } else {
        // desaturate icon
        QPixmap alpha = m_iconPix;
        QPainter p(&alpha);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.fillRect(alpha.rect(), QColor(80, 80, 80, 80));
        p.end();
        m_colorIcon->setPixmap(alpha);
    }
}

void CollapsibleEffect::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (frame->underMouse() && collapseButton->isEnabled()) {
        event->accept();
        slotSwitch();
    } else {
        event->ignore();
    }
}

void CollapsibleEffect::mouseReleaseEvent(QMouseEvent *event)
{
    if (!decoframe->property("active").toBool()) {
        emit activateEffect(effectIndex());
    }
    QWidget::mouseReleaseEvent(event);
}

void CollapsibleEffect::slotDisable(bool disable, bool emitInfo)
{
    title->setEnabled(!disable);
    m_enabledButton->setActive(disable);
    m_effect.setAttribute(QStringLiteral("disable"), disable ? 1 : 0);
    if (!disable || KdenliveSettings::disable_effect_parameters()) {
        widgetFrame->setEnabled(!disable);
    }
    if (emitInfo) {
        emit effectStateChanged(disable, effectIndex(), needsMonitorEffectScene());
    }
}

void CollapsibleEffect::slotDeleteEffect()
{
    emit deleteEffect(m_effect);
}

void CollapsibleEffect::slotEffectUp()
{
    emit changeEffectPosition(QList<int>() << effectIndex(), true);
}

void CollapsibleEffect::slotEffectDown()
{
    emit changeEffectPosition(QList<int>() << effectIndex(), false);
}

void CollapsibleEffect::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.trimmed().isEmpty()) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (dir.exists(name + QStringLiteral(".xml"))) if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", name + QStringLiteral(".xml"))) == KMessageBox::No) {
        return;
    }

    QDomDocument doc;
    QDomElement effect = m_effect.cloneNode().toElement();
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute(QStringLiteral("kdenlive_ix"));
    effect.setAttribute(QStringLiteral("id"), name);
    effect.setAttribute(QStringLiteral("type"), QStringLiteral("custom"));
    if (m_paramWidget) {
        int in = m_paramWidget->range().x();
        EffectsController::offsetKeyframes(in, effect);
    }
    QDomElement effectname = effect.firstChildElement(QStringLiteral("name"));
    effect.removeChild(effectname);
    effectname = doc.createElement(QStringLiteral("name"));
    QDomText nametext = doc.createTextNode(name);
    effectname.appendChild(nametext);
    effect.insertBefore(effectname, QDomNode());
    QDomElement effectprops = effect.firstChildElement(QStringLiteral("properties"));
    effectprops.setAttribute(QStringLiteral("id"), name);
    effectprops.setAttribute(QStringLiteral("type"), QStringLiteral("custom"));

    QFile file(dir.absoluteFilePath(name + QStringLiteral(".xml")));
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
    bool expand = !widgetFrame->isVisible();
    widgetFrame->setVisible(true);
    slotShow(expand);
    m_animation->setDirection(expand ? QTimeLine::Forward : QTimeLine::Backward);
    m_animation->start();
}

void CollapsibleEffect::slotShow(bool show)
{
    if (show) {
        collapseButton->setArrowType(Qt::DownArrow);
        m_info.isCollapsed = false;
    } else {
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
    if (info != m_effect.attribute(QStringLiteral("kdenlive_info"))) {
        m_effect.setAttribute(QStringLiteral("kdenlive_info"), info);
        emit parameterChanged(m_original_effect, m_effect, effectIndex());
    }
}

void CollapsibleEffect::setGroupIndex(int ix)
{
    if (m_info.groupIndex == -1 && ix != -1) {
        m_menu->removeAction(m_groupAction);
    } else if (m_info.groupIndex != -1 && ix == -1) {
        m_menu->addAction(m_groupAction);
    }
    m_info.groupIndex = ix;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
}

void CollapsibleEffect::setGroupName(const QString &groupName)
{
    m_info.groupName = groupName;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
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
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

int CollapsibleEffect::groupIndex() const
{
    return m_info.groupIndex;
}

int CollapsibleEffect::effectIndex() const
{
    if (m_effect.isNull()) {
        return -1;
    }
    return m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
}

void CollapsibleEffect::updateWidget(const ItemInfo &info, const QDomElement &effect, EffectMetaInfo *metaInfo)
{
    // cleanup
    delete m_paramWidget;
    m_paramWidget = nullptr;
    m_effect = effect;
    setupWidget(info, metaInfo);
}

void CollapsibleEffect::updateFrameInfo()
{
    if (m_paramWidget) {
        m_paramWidget->refreshFrameInfo();
    }
}

void CollapsibleEffect::setActiveKeyframe(int kf)
{
    if (m_paramWidget) {
        m_paramWidget->setActiveKeyframe(kf);
    }
}

void CollapsibleEffect::setupWidget(const ItemInfo &info, EffectMetaInfo *metaInfo)
{
    if (m_effect.isNull()) {
        //         //qCDebug(KDENLIVE_LOG) << "// EMPTY EFFECT STACK";
        return;
    }
    delete m_paramWidget;
    m_paramWidget = nullptr;

    if (m_effect.attribute(QStringLiteral("tag")) == QLatin1String("region")) {
        m_regionEffect = true;
        QDomNodeList effects =  m_effect.elementsByTagName(QStringLiteral("effect"));
        QDomNodeList origin_effects =  m_original_effect.elementsByTagName(QStringLiteral("effect"));
        m_paramWidget = new ParameterContainer(m_effect, info, metaInfo, widgetFrame);
        QWidget *container = new QWidget(widgetFrame);
        QVBoxLayout *vbox = static_cast<QVBoxLayout *>(widgetFrame->layout());
        vbox->addWidget(container);
        // m_paramWidget = new ParameterContainer(m_effect.toElement(), info, metaInfo, container);
        for (int i = 0; i < effects.count(); ++i) {
            bool canMoveUp = true;
            if (i == 0 || effects.at(i - 1).toElement().attribute(QStringLiteral("id")) == QLatin1String("speed")) {
                canMoveUp = false;
            }
            CollapsibleEffect *coll = new CollapsibleEffect(effects.at(i).toElement(), origin_effects.at(i).toElement(), info, metaInfo, canMoveUp, i == effects.count() - 1, container);
            m_subParamWidgets.append(coll);
            connect(coll, &CollapsibleEffect::parameterChanged, this, &CollapsibleEffect::slotUpdateRegionEffectParams);
            //container = new QWidget(widgetFrame);
            vbox->addWidget(coll);
            //p = new ParameterContainer(effects.at(i).toElement(), info, isEffect, container);
        }
    } else {
        m_paramWidget = new ParameterContainer(m_effect, info, metaInfo, widgetFrame);
        connect(m_paramWidget, &ParameterContainer::disableCurrentFilter, this, &CollapsibleEffect::slotDisableEffect);
        connect(m_paramWidget, &ParameterContainer::importKeyframes, this, &CollapsibleEffect::importKeyframes);
        if (m_effect.firstChildElement(QStringLiteral("parameter")).isNull()) {
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
    connect(m_paramWidget, &ParameterContainer::parameterChanged, this, &CollapsibleEffect::parameterChanged);

    connect(m_paramWidget, &ParameterContainer::startFilterJob, this, &CollapsibleEffect::startFilterJob);

    connect(this, &CollapsibleEffect::syncEffectsPos, m_paramWidget, &ParameterContainer::syncEffectsPos);
    connect(m_paramWidget, &ParameterContainer::checkMonitorPosition, this, &CollapsibleEffect::checkMonitorPosition);
    connect(m_paramWidget, &ParameterContainer::seekTimeline, this, &CollapsibleEffect::seekTimeline);
    connect(m_paramWidget, &ParameterContainer::importClipKeyframes, this, &CollapsibleEffect::prepareImportClipKeyframes);
}

void CollapsibleEffect::slotDisableEffect(bool disable)
{
    title->setEnabled(!disable);
    m_enabledButton->setActive(disable);
    m_effect.setAttribute(QStringLiteral("disable"), disable ? 1 : 0);
    emit effectStateChanged(disable, effectIndex(), needsMonitorEffectScene());
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
        for (int i = 0; i < m_subParamWidgets.count(); ++i) {
            m_subParamWidgets.at(i)->updateTimecodeFormat();
        }
    }
}

void CollapsibleEffect::slotUpdateRegionEffectParams(const QDomElement &/*old*/, const QDomElement &/*e*/, int /*ix*/)
{
    //qCDebug(KDENLIVE_LOG)<<"// EMIT CHANGE SUBEFFECT.....:";
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

void CollapsibleEffect::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}

void CollapsibleEffect::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        frame->setProperty("target", true);
        frame->setStyleSheet(frame->styleSheet());
        event->acceptProposedAction();
    } else if (m_paramWidget->doesAcceptDrops() && event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry")) && event->source()->objectName() != QStringLiteral("ParameterContainer")) {
        event->setDropAction(Qt::CopyAction);
        event->setAccepted(true);
    } else {
        QWidget::dragEnterEvent(event);
    }
}

void CollapsibleEffect::dragLeaveEvent(QDragLeaveEvent */*event*/)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
}

void CollapsibleEffect::importKeyframes(const QString &kf)
{
    QMap<QString, QString> keyframes;
    if (kf.contains(QLatin1Char('\n'))) {
        const QStringList params = kf.split(QLatin1Char('\n'), QString::SkipEmptyParts);
        for (const QString &param : params) {
            keyframes.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
        }
    } else {
        keyframes.insert(kf.section(QLatin1Char('='), 0, 0), kf.section(QLatin1Char('='), 1));
    }
    emit importClipKeyframes(AVWidget, m_itemInfo, m_effect.cloneNode().toElement(), keyframes);
}

void CollapsibleEffect::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        if (event->source()->objectName() == QStringLiteral("ParameterContainer")) {
            return;
        }
        emit activateEffect(effectIndex());
        QString itemData = event->mimeData()->data(QStringLiteral("kdenlive/geometry"));
        importKeyframes(itemData);
        return;
    }
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
    const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    //event->acceptProposedAction();
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    int ix = e.attribute(QStringLiteral("kdenlive_ix")).toInt();
    int currentEffectIx = effectIndex();
    if (ix == currentEffectIx || e.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
        // effect dropped on itself, or unmovable speed dropped, reject
        event->ignore();
        return;
    }
    if (ix == 0 || e.tagName() == QLatin1String("effectgroup")) {
        if (e.tagName() == QLatin1String("effectgroup")) {
            // moving a group
            QDomNodeList subeffects = e.elementsByTagName(QStringLiteral("effect"));
            if (subeffects.isEmpty()) {
                event->ignore();
                return;
            }
            EffectInfo info;
            info.fromString(subeffects.at(0).toElement().attribute(QStringLiteral("kdenlive_info")));
            event->setDropAction(Qt::MoveAction);
            event->accept();
            if (info.groupIndex >= 0) {
                // Moving group
                QList<int> effectsIds;
                // Collect moved effects ids
                for (int i = 0; i < subeffects.count(); ++i) {
                    QDomElement effect = subeffects.at(i).toElement();
                    effectsIds << effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
                }
                emit moveEffect(effectsIds, currentEffectIx, info.groupIndex, info.groupName);
            } else {
                // group effect dropped from effect list
                if (m_info.groupIndex > -1) {
                    // TODO: Should we merge groups??

                }
                emit addEffect(e);
            }
            return;
        }
        // effect dropped from effects list, add it
        e.setAttribute(QStringLiteral("kdenlive_ix"), ix);
        if (m_info.groupIndex > -1) {
            // Dropped on a group
            e.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
        }
        event->setDropAction(Qt::CopyAction);
        event->accept();
        emit addEffect(e);
        return;
    }
    emit moveEffect(QList<int> () << ix, currentEffectIx, m_info.groupIndex, m_info.groupName);
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void CollapsibleEffect::adjustButtons(int ix, int max)
{
    buttonUp->setEnabled(ix > 0);
    buttonDown->setEnabled(ix < max - 1);
}

MonitorSceneType CollapsibleEffect::needsMonitorEffectScene() const
{
    if (m_paramWidget && !m_enabledButton->isActive()) {
        return m_paramWidget->needsMonitorEffectScene();
    } else {
        return MonitorSceneDefault;
    }
}

void CollapsibleEffect::setRange(int inPoint, int outPoint)
{
    m_paramWidget->setRange(inPoint, outPoint);
}

void CollapsibleEffect::setKeyframes(const QString &tag, const QString &keyframes)
{
    m_paramWidget->setKeyframes(tag, keyframes);
}

bool CollapsibleEffect::isMovable() const
{
    return m_isMovable;
}

void CollapsibleEffect::prepareImportClipKeyframes()
{
    emit importClipKeyframes(AVWidget, m_itemInfo, m_effect.cloneNode().toElement(), QMap<QString, QString>());
}
