/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "collapsibleeffectview.hpp"
#include "dialogs/clipcreationdialog.h"
#include "effectslist/effectslist.h"
#include "kdenlivesettings.h"
#include "mltcontroller/effectscontroller.h"
#include "utils/KoIconUtils.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectsrepository.hpp"
#include "assets/view/assetparameterview.hpp"
#include "core.h"

#include "kdenlive_debug.h"
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPropertyAnimation>

#include <KComboBox>
#include <KDualAction>
#include <KMessageBox>
#include <KRecentDirs>
#include <klocalizedstring.h>

CollapsibleEffectView::CollapsibleEffectView(std::shared_ptr<EffectItemModel> effectModel, QImage icon, QWidget *parent)
    : AbstractCollapsibleWidget(parent)
/*    , m_effect(effect)
    , m_itemInfo(info)
    , m_original_effect(original_effect)
    , m_isMovable(true)*/
    , m_model(effectModel)
    , m_regionEffect(false)
{
    QString effectId = effectModel->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    qDebug()<<" * * *DISPLAYING EFFECT; "<<effectName<<" = "<<effectId;
    if (effectId == QLatin1String("region")) {
        m_regionEffect = true;
        decoframe->setObjectName(QStringLiteral("decoframegroup"));
    }
    filterWheelEvent = true;
    //decoframe->setProperty("active", true);
    //m_info.fromString(effect.attribute(QStringLiteral("kdenlive_info")));
    // setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
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
    //buttonUp->setEnabled(canMoveUp);
    //buttonDown->setEnabled(!lastEffect);

    if (effectId == QLatin1String("speed")) {
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
    // checkAll->setToolTip(i18n("Enable/Disable all effects"));
    // buttonShowComments->setIcon(KoIconUtils::themedIcon("help-about"));
    // buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));
    m_menu = new QMenu(this);
    if (effectModel->rowCount() > 0) {
        m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("view-refresh")), i18n("Reset Effect"), this, SLOT(slotResetEffect()));
    }
    m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("document-save")), i18n("Save Effect"), this, SLOT(slotSaveEffect()));

    QHBoxLayout *l = static_cast<QHBoxLayout *>(frame->layout());
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
    connect(m_groupAction, &QAction::triggered, this, &CollapsibleEffectView::slotCreateGroup);

    if (m_regionEffect) {
        effectName.append(':' + QUrl(EffectsList::parameter(m_effect, QStringLiteral("resource"))).fileName());
    }

    // Color thumb
    m_colorIcon->setPixmap(QPixmap::fromImage(icon));
    title->setText(effectName);

    if (!m_regionEffect) {
        if (m_info.groupIndex == -1) {
            m_menu->addAction(m_groupAction);
        }
        m_menu->addAction(KoIconUtils::themedIcon(QStringLiteral("folder-new")), i18n("Create Region"), this, SLOT(slotCreateRegion()));
    }
    m_view = new AssetParameterView(this);
    m_view->setModel(std::static_pointer_cast<AssetParameterModel>(effectModel));
    QVBoxLayout *lay = new QVBoxLayout(widgetFrame);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_view);
    //setupWidget(info, metaInfo);
    menuButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-menu")));
    menuButton->setMenu(m_menu);

    if (!effectModel->isEnabled()) {
        title->setEnabled(false);
        m_colorIcon->setEnabled(false);
        if (KdenliveSettings::disable_effect_parameters()) {
            widgetFrame->setEnabled(false);
        }
        m_enabledButton->setActive(true);
    } else {
        m_enabledButton->setActive(false);
    }

    m_collapse = new KDualAction(i18n("Collapse Effect"), i18n("Expand Effect"), this);
    m_collapse->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("arrow-right")));
    m_collapse->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("arrow-down")));
    collapseButton->setDefaultAction(m_collapse);
    connect(m_collapse, &KDualAction::activeChanged, this, &CollapsibleEffectView::slotSwitch);

    connect(m_enabledButton, SIGNAL(activeChangedByUser(bool)), this, SLOT(slotDisable(bool)));
    connect(buttonUp, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotEffectUp);
    connect(buttonDown, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotEffectDown);
    connect(buttonDel, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotDeleteEffect);

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
}

CollapsibleEffectView::~CollapsibleEffectView()
{
    // delete m_paramWidget;
    delete m_menu;
}

void CollapsibleEffectView::setWidgetHeight(qreal value)
{
    widgetFrame->setFixedHeight(m_view->contentHeight() * value);
}

void CollapsibleEffectView::slotCreateGroup()
{
    emit createGroup(effectIndex());
}

void CollapsibleEffectView::slotCreateRegion()
{
    QString allExtensions = ClipCreationDialog::getExtensions().join(QLatin1Char(' '));
    const QString dialogFilter =
        allExtensions + QLatin1Char(' ') + QLatin1Char('|') + i18n("All Supported Files") + QStringLiteral("\n* ") + QLatin1Char('|') + i18n("All Files");
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

void CollapsibleEffectView::slotUnGroup()
{
    emit unGroup(this);
}

bool CollapsibleEffectView::eventFilter(QObject *o, QEvent *e)
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
            }
            e->ignore();
            return true;
        }
        if (qobject_cast<KComboBox *>(o)) {
            if (qobject_cast<KComboBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            }
            e->ignore();
            return true;
        }
        if (qobject_cast<QProgressBar *>(o)) {
            if (qobject_cast<QProgressBar *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            }
            e->ignore();
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}

QDomElement CollapsibleEffectView::effect() const
{
    return m_effect;
}

QDomElement CollapsibleEffectView::effectForSave() const
{
    QDomElement effect = m_effect.cloneNode().toElement();
    effect.removeAttribute(QStringLiteral("kdenlive_ix"));
    /*
    if (m_paramWidget) {
        int in = m_paramWidget->range().x();
        EffectsController::offsetKeyframes(in, effect);
    }
    */
    return effect;
}

bool CollapsibleEffectView::isActive() const
{
    return decoframe->property("active").toBool();
}

bool CollapsibleEffectView::isEnabled() const
{
    return m_enabledButton->isActive();
}

void CollapsibleEffectView::slotActivateEffect(QModelIndex ix)
{
    decoframe->setProperty("active", ix.row() == m_model->row());
    decoframe->setStyleSheet(decoframe->styleSheet());
}

void CollapsibleEffectView::setActive(bool activate)
{
    /*
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
    */
}

void CollapsibleEffectView::mousePressEvent(QMouseEvent *e)
{
    m_dragStart = e->globalPos();
    emit activateEffect(m_model);
    QWidget::mousePressEvent(e);
}

void CollapsibleEffectView::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->globalPos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
        QPixmap pix = frame->grab();
        emit startDrag(pix, m_model);
    }
    QWidget::mouseMoveEvent(e);
}

void CollapsibleEffectView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (frame->underMouse() && collapseButton->isEnabled()) {
        event->accept();
        m_collapse->setActive(!m_collapse->isActive());
    } else {
        event->ignore();
    }
}

void CollapsibleEffectView::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragStart = QPoint();
    if (!decoframe->property("active").toBool()) {
        //emit activateEffect(effectIndex());
    }
    QWidget::mouseReleaseEvent(event);
}

void CollapsibleEffectView::slotDisable(bool disable)
{
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    std::static_pointer_cast<AbstractEffectItem>(m_model)->markEnabled(effectName, !disable);
}

void CollapsibleEffectView::slotDeleteEffect()
{
    emit deleteEffect(m_model);
}

void CollapsibleEffectView::slotEffectUp()
{
    emit moveEffect(qMax(0, m_model->row() -1), m_model);
}

void CollapsibleEffectView::slotEffectDown()
{
    emit moveEffect(m_model->row() + 1, m_model);
}

void CollapsibleEffectView::slotSaveEffect()
{
    QString name = QInputDialog::getText(this, i18n("Save Effect"), i18n("Name for saved effect: "));
    if (name.trimmed().isEmpty()) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    if (dir.exists(name + QStringLiteral(".xml")))
        if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", name + QStringLiteral(".xml"))) == KMessageBox::No) {
            return;
        }

    QDomDocument doc;
    QDomElement effect = m_effect.cloneNode().toElement();
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute(QStringLiteral("kdenlive_ix"));
    effect.setAttribute(QStringLiteral("id"), name);
    effect.setAttribute(QStringLiteral("type"), QStringLiteral("custom"));
    /*
    if (m_paramWidget) {
        int in = m_paramWidget->range().x();
        EffectsController::offsetKeyframes(in, effect);
    }
    */
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

void CollapsibleEffectView::slotResetEffect()
{
    m_view->resetValues();
}

void CollapsibleEffectView::slotSwitch(bool expand)
{
    slotShow(expand);
    emit switchHeight(m_model, expand ? frame->height() : frame->height() + m_view->contentHeight());
    setFixedHeight(expand ? frame->height() : frame->height() + m_view->contentHeight());
    widgetFrame->setVisible(!expand);
    /*if (!expand) {
        widgetFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        widgetFrame->setFixedHeight(m_view->contentHeight());
    } else {
        widgetFrame->setFixedHeight(QWIDGETSIZE_MAX);
    }*/
    /*const QRect final_geometry = expand ? QRect(0, 0, width(), title->height()) : QRect(rect().topLeft(), size());
    QPropertyAnimation *anim = new QPropertyAnimation(this, "geometry", this);
    anim->setDuration(200);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setEndValue(final_geometry);
    //connect(anim, SIGNAL(valueChanged(const QVariant &)), SLOT(animationChanged(const QVariant &)));
    connect(anim, SIGNAL(finished()), SLOT(animationFinished()));
    anim->start(QPropertyAnimation::DeleteWhenStopped);*/
}

void CollapsibleEffectView::animationChanged(const QVariant &geom)
{
    parentWidget()->setFixedHeight(geom.toRect().height());
}

void CollapsibleEffectView::animationFinished()
{
    if (m_collapse->isActive()) {
        widgetFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    } else {
        widgetFrame->setFixedHeight(m_view->contentHeight());
    }
}

void CollapsibleEffectView::slotShow(bool show)
{
    if (show) {
        //collapseButton->setArrowType(Qt::DownArrow);
        m_info.isCollapsed = false;
    } else {
        //collapseButton->setArrowType(Qt::RightArrow);
        m_info.isCollapsed = true;
    }
    updateCollapsedState();
}

void CollapsibleEffectView::groupStateChanged(bool collapsed)
{
    m_info.groupIsCollapsed = collapsed;
    updateCollapsedState();
}

void CollapsibleEffectView::updateCollapsedState()
{
    QString info = m_info.toString();
    if (info != m_effect.attribute(QStringLiteral("kdenlive_info"))) {
        m_effect.setAttribute(QStringLiteral("kdenlive_info"), info);
        emit parameterChanged(m_original_effect, m_effect, effectIndex());
    }
}

void CollapsibleEffectView::setGroupIndex(int ix)
{
    if (m_info.groupIndex == -1 && ix != -1) {
        m_menu->removeAction(m_groupAction);
    } else if (m_info.groupIndex != -1 && ix == -1) {
        m_menu->addAction(m_groupAction);
    }
    m_info.groupIndex = ix;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
}

void CollapsibleEffectView::setGroupName(const QString &groupName)
{
    m_info.groupName = groupName;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
}

QString CollapsibleEffectView::infoString() const
{
    return m_info.toString();
}

void CollapsibleEffectView::removeFromGroup()
{
    if (m_info.groupIndex != -1) {
        m_menu->addAction(m_groupAction);
    }
    m_info.groupIndex = -1;
    m_info.groupName.clear();
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

int CollapsibleEffectView::groupIndex() const
{
    return m_info.groupIndex;
}

int CollapsibleEffectView::effectIndex() const
{
    if (m_effect.isNull()) {
        return -1;
    }
    return m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
}

void CollapsibleEffectView::updateWidget(const ItemInfo &info, const QDomElement &effect, EffectMetaInfo *metaInfo)
{
    // cleanup
    /*
    delete m_paramWidget;
    m_paramWidget = nullptr;
    */
    m_effect = effect;
    setupWidget(info, metaInfo);
}

void CollapsibleEffectView::updateFrameInfo()
{
    /*
    if (m_paramWidget) {
        m_paramWidget->refreshFrameInfo();
    }
    */
}

void CollapsibleEffectView::setActiveKeyframe(int kf)
{
    /*
    if (m_paramWidget) {
        m_paramWidget->setActiveKeyframe(kf);
    }
    */
}

void CollapsibleEffectView::setupWidget(const ItemInfo &info, EffectMetaInfo *metaInfo)
{
    /*
    if (m_effect.isNull()) {
        //         //qCDebug(KDENLIVE_LOG) << "// EMPTY EFFECT STACK";
        return;
    }
    delete m_paramWidget;
    m_paramWidget = nullptr;

    if (m_effect.attribute(QStringLiteral("tag")) == QLatin1String("region")) {
        m_regionEffect = true;
        QDomNodeList effects = m_effect.elementsByTagName(QStringLiteral("effect"));
        QDomNodeList origin_effects = m_original_effect.elementsByTagName(QStringLiteral("effect"));
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
            CollapsibleEffectView *coll = new CollapsibleEffectView(effects.at(i).toElement(), origin_effects.at(i).toElement(), info, metaInfo, canMoveUp,
                                                            i == effects.count() - 1, container);
            m_subParamWidgets.append(coll);
            connect(coll, &CollapsibleEffectView::parameterChanged, this, &CollapsibleEffectView::slotUpdateRegionEffectParams);
            // container = new QWidget(widgetFrame);
            vbox->addWidget(coll);
            // p = new ParameterContainer(effects.at(i).toElement(), info, isEffect, container);
        }
    } else {
        m_paramWidget = new ParameterContainer(m_effect, info, metaInfo, widgetFrame);
        connect(m_paramWidget, &ParameterContainer::disableCurrentFilter, this, &CollapsibleEffectView::slotDisable);
        connect(m_paramWidget, &ParameterContainer::importKeyframes, this, &CollapsibleEffectView::importKeyframes);
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
    connect(m_paramWidget, &ParameterContainer::parameterChanged, this, &CollapsibleEffectView::parameterChanged);

    connect(m_paramWidget, &ParameterContainer::startFilterJob, this, &CollapsibleEffectView::startFilterJob);

    connect(this, &CollapsibleEffectView::syncEffectsPos, m_paramWidget, &ParameterContainer::syncEffectsPos);
    connect(m_paramWidget, &ParameterContainer::checkMonitorPosition, this, &CollapsibleEffectView::checkMonitorPosition);
    connect(m_paramWidget, &ParameterContainer::seekTimeline, this, &CollapsibleEffectView::seekTimeline);
    connect(m_paramWidget, &ParameterContainer::importClipKeyframes, this, &CollapsibleEffectView::prepareImportClipKeyframes);
    */
}

bool CollapsibleEffectView::isGroup() const
{
    return false;
}

void CollapsibleEffectView::updateTimecodeFormat()
{
    /*
    m_paramWidget->updateTimecodeFormat();
    if (!m_subParamWidgets.isEmpty()) {
        // we have a group
        for (int i = 0; i < m_subParamWidgets.count(); ++i) {
            m_subParamWidgets.at(i)->updateTimecodeFormat();
        }
    }
    */
}

void CollapsibleEffectView::slotUpdateRegionEffectParams(const QDomElement & /*old*/, const QDomElement & /*e*/, int /*ix*/)
{
    // qCDebug(KDENLIVE_LOG)<<"// EMIT CHANGE SUBEFFECT.....:";
    emit parameterChanged(m_original_effect, m_effect, effectIndex());
}

void CollapsibleEffectView::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}

void CollapsibleEffectView::dragEnterEvent(QDragEnterEvent *event)
{
    /*
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        frame->setProperty("target", true);
        frame->setStyleSheet(frame->styleSheet());
        event->acceptProposedAction();
    } else if (m_paramWidget->doesAcceptDrops() && event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry")) &&
               event->source()->objectName() != QStringLiteral("ParameterContainer")) {
        event->setDropAction(Qt::CopyAction);
        event->setAccepted(true);
    } else {
        QWidget::dragEnterEvent(event);
    }
    */
}

void CollapsibleEffectView::dragLeaveEvent(QDragLeaveEvent * /*event*/)
{
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
}

void CollapsibleEffectView::importKeyframes(const QString &kf)
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

void CollapsibleEffectView::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(QStringLiteral("kdenlive/geometry"))) {
        if (event->source()->objectName() == QStringLiteral("ParameterContainer")) {
            return;
        }
        //emit activateEffect(effectIndex());
        QString itemData = event->mimeData()->data(QStringLiteral("kdenlive/geometry"));
        importKeyframes(itemData);
        return;
    }
    frame->setProperty("target", false);
    frame->setStyleSheet(frame->styleSheet());
    const QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    // event->acceptProposedAction();
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
                //emit moveEffect(effectsIds, currentEffectIx, info.groupIndex, info.groupName);
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
    //emit moveEffect(QList<int>() << ix, currentEffectIx, m_info.groupIndex, m_info.groupName);
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void CollapsibleEffectView::adjustButtons(int ix, int max)
{
    buttonUp->setEnabled(ix > 0);
    buttonDown->setEnabled(ix < max - 1);
}

MonitorSceneType CollapsibleEffectView::needsMonitorEffectScene() const
{
    /*
    if ((m_paramWidget != nullptr) && !m_enabledButton->isActive()) {
        return m_paramWidget->needsMonitorEffectScene();
    }
    */
    return MonitorSceneDefault;
}

void CollapsibleEffectView::setRange(int inPoint, int outPoint)
{
    /*
    m_paramWidget->setRange(inPoint, outPoint);
    */
}

void CollapsibleEffectView::setKeyframes(const QString &tag, const QString &keyframes)
{
    /*
    m_paramWidget->setKeyframes(tag, keyframes);
    */
}

bool CollapsibleEffectView::isMovable() const
{
    return m_isMovable;
}

void CollapsibleEffectView::prepareImportClipKeyframes()
{
    emit importClipKeyframes(AVWidget, m_itemInfo, m_effect.cloneNode().toElement(), QMap<QString, QString>());
}
