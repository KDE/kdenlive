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
#include "assets/view/assetparameterview.hpp"
#include "assets/view/widgets/colorwheel.h"
#include "core.h"
#include "dialogs/clipcreationdialog.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"

#include "kdenlive_debug.h"
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QProgressBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPointer>

#include <QComboBox>
#include <KDualAction>
#include <KMessageBox>
#include <KRecentDirs>
#include <KSqueezedTextLabel>
#include <klocalizedstring.h>

CollapsibleEffectView::CollapsibleEffectView(const std::shared_ptr<EffectItemModel> &effectModel, QSize frameSize, const QImage &icon, QWidget *parent)
    : AbstractCollapsibleWidget(parent)
    , m_view(nullptr)
    , m_model(effectModel)
    , m_regionEffect(false)
    , m_blockWheel(false)
{
    QString effectId = effectModel->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    if (effectId == QLatin1String("region")) {
        m_regionEffect = true;
        decoframe->setObjectName(QStringLiteral("decoframegroup"));
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    buttonUp->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-up")));
    buttonUp->setToolTip(i18n("Move effect up"));
    buttonDown->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-down")));
    buttonDown->setToolTip(i18n("Move effect down"));
    buttonDel->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-deleffect")));
    buttonDel->setToolTip(i18n("Delete effect"));

    if (effectId == QLatin1String("speed")) {
        // Speed effect is a "pseudo" effect, cannot be moved
        buttonUp->setVisible(false);
        buttonDown->setVisible(false);
        m_isMovable = false;
        setAcceptDrops(false);
    } else {
        setAcceptDrops(true);
    }

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_collapse = new KDualAction(i18n("Collapse Effect"), i18n("Expand Effect"), this);
    m_collapse->setActiveIcon(QIcon::fromTheme(QStringLiteral("arrow-right")));
    collapseButton->setDefaultAction(m_collapse);
    m_collapse->setActive(m_model->isCollapsed());
    connect(m_collapse, &KDualAction::activeChanged, this, &CollapsibleEffectView::slotSwitch);
    if (effectModel->rowCount() == 0) {
        // Effect has no paramerter
        m_collapse->setInactiveIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
        collapseButton->setEnabled(false);
    } else {
        m_collapse->setInactiveIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
    }

    auto *l = static_cast<QHBoxLayout *>(frame->layout());
    m_colorIcon = new QLabel(this);
    l->insertWidget(0, m_colorIcon);
    m_colorIcon->setFixedSize(collapseButton->sizeHint());
    m_colorIcon->setToolTip(effectName);
    title = new KSqueezedTextLabel(this);
    l->insertWidget(2, title);

    m_keyframesButton = new QToolButton(this);
    m_keyframesButton->setIcon(QIcon::fromTheme(QStringLiteral("adjustcurves")));
    m_keyframesButton->setAutoRaise(true);
    m_keyframesButton->setCheckable(true);
    m_keyframesButton->setToolTip(i18n("Enable Keyframes"));
    l->insertWidget(3, m_keyframesButton);

    // Enable button
    m_enabledButton = new KDualAction(i18n("Disable Effect"), i18n("Enable Effect"), this);
    m_enabledButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("hint")));
    m_enabledButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("visibility")));
    enabledButton->setDefaultAction(m_enabledButton);
    connect(m_model.get(), &AssetParameterModel::enabledChange, this, &CollapsibleEffectView::enableView);
    m_groupAction = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), i18n("Create Group"), this);
    connect(m_groupAction, &QAction::triggered, this, &CollapsibleEffectView::slotCreateGroup);

    if (m_regionEffect) {
        effectName.append(':' + QUrl(Xml::getXmlParameter(m_effect, QStringLiteral("resource"))).fileName());
    }

    // Color thumb
    m_colorIcon->setScaledContents(true);
    m_colorIcon->setPixmap(QPixmap::fromImage(icon));
    title->setText(effectName);
    frame->setMinimumHeight(collapseButton->sizeHint().height());

    m_view = new AssetParameterView(this);
    const std::shared_ptr<AssetParameterModel> effectParamModel = std::static_pointer_cast<AssetParameterModel>(effectModel);
    m_view->setModel(effectParamModel, frameSize);
    connect(m_view, &AssetParameterView::seekToPos, this, &AbstractCollapsibleWidget::seekToPos);
    connect(m_view, &AssetParameterView::activateEffect, this, [this]() {
        if (!decoframe->property("active").toBool()) {
            // Activate effect if not already active
            emit activateEffect(m_model);
        }
    });
    connect(m_view, &AssetParameterView::updateHeight, this, &CollapsibleEffectView::updateHeight);
    connect(this, &CollapsibleEffectView::refresh, m_view, &AssetParameterView::slotRefresh);
    m_keyframesButton->setVisible(m_view->keyframesAllowed());
    auto *lay = new QVBoxLayout(widgetFrame);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_view);
    connect(m_keyframesButton, &QToolButton::toggled, this, [this](bool toggle) {
        m_view->toggleKeyframes(toggle);
    });
    if (!effectParamModel->hasMoreThanOneKeyframe()) {
        // No keyframe or only one, allow hiding
        bool hideByDefault = effectParamModel->data(effectParamModel->index(0, 0), AssetParameterModel::HideKeyframesFirstRole).toBool();
        if (hideByDefault) {
            m_view->toggleKeyframes(false);
        } else {
            m_keyframesButton->setChecked(true);
        }
    } else {
        m_keyframesButton->setChecked(true);
    }
    // Presets
    presetButton->setIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
    presetButton->setMenu(m_view->presetMenu());
    presetButton->setToolTip(i18n("Presets"));

    // Main menu
    m_menu = new QMenu(this);
    if (effectModel->rowCount() == 0) {
        collapseButton->setEnabled(false);
        m_view->setVisible(false);
    }
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("document-save")), i18n("Save Effect"), this, SLOT(slotSaveEffect()));
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("document-save-all")), i18n("Save Effect Stack"), this, SIGNAL(saveStack()));
    if (!m_regionEffect) {
        /*if (m_info.groupIndex == -1) {
            m_menu->addAction(m_groupAction);
        }*/
        m_menu->addAction(QIcon::fromTheme(QStringLiteral("folder-new")), i18n("Create Region"), this, SLOT(slotCreateRegion()));
    }

    // setupWidget(info, metaInfo);
    menuButton->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
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

    connect(m_enabledButton, &KDualAction::activeChangedByUser, this, &CollapsibleEffectView::slotDisable);
    connect(buttonUp, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotEffectUp);
    connect(buttonDown, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotEffectDown);
    connect(buttonDel, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotDeleteEffect);

    for (QSpinBox *sp : findChildren<QSpinBox *>()) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(Qt::StrongFocus);
    }
    for (QComboBox *cb : findChildren<QComboBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    for (QProgressBar *cb : findChildren<QProgressBar *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    for (WheelContainer *cb : findChildren<WheelContainer *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    for (QDoubleSpinBox *cb : findChildren<QDoubleSpinBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(Qt::StrongFocus);
    }
    QMetaObject::invokeMethod(this, "slotSwitch", Qt::QueuedConnection, Q_ARG(bool, m_model->isCollapsed()));
}

CollapsibleEffectView::~CollapsibleEffectView()
{
    qDebug() << "deleting collapsibleeffectview";
}

void CollapsibleEffectView::setWidgetHeight(qreal value)
{
    widgetFrame->setFixedHeight(m_view->contentHeight() * value);
}

void CollapsibleEffectView::slotCreateGroup()
{
    emit createGroup(m_model);
}

void CollapsibleEffectView::slotCreateRegion()
{
    const QString dialogFilter = ClipCreationDialog::getExtensionsFilter(QStringList() << i18n("All Files") + QStringLiteral(" (*)"));
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
        auto *we = static_cast<QWheelEvent *>(e);
        if (!m_blockWheel || we->modifiers() != Qt::NoModifier) {
            e->accept();
            return false;
        }
        if (qobject_cast<QAbstractSpinBox *>(o)) {
            if (!qobject_cast<QAbstractSpinBox *>(o)->hasFocus()) {
                e->ignore();
                return true;
            }
            e->accept();
            return false;
        }
        if (qobject_cast<QComboBox *>(o)) {
            if (qobject_cast<QComboBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                e->accept();
                return false;
            }
            e->ignore();
            return true;
        }
        if (qobject_cast<QProgressBar *>(o)) {
            if (!qobject_cast<QProgressBar *>(o)->hasFocus()) {
                e->ignore();
                return true;
            }
            e->accept();
            return false;
        }
        if (qobject_cast<WheelContainer *>(o)) {
            if (!qobject_cast<WheelContainer *>(o)->hasFocus()) {
                e->ignore();
                return true;
            }
            e->accept();
            return false;
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

void CollapsibleEffectView::slotActivateEffect(bool active)
{
    // m_colorIcon->setEnabled(active);
    // bool active = ix.row() == m_model->row(); 
    decoframe->setProperty("active", active);
    decoframe->setStyleSheet(decoframe->styleSheet());
    if (active) {
        pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    }
    emit m_view->initKeyframeView(active);
}

void CollapsibleEffectView::mousePressEvent(QMouseEvent *e)
{
    m_dragStart = e->globalPos();
    if (!decoframe->property("active").toBool()) {
        // Activate effect if not already active
        emit activateEffect(m_model);
    }
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
        // emit activateEffect(effectIndex());
    }
    QWidget::mouseReleaseEvent(event);
}

void CollapsibleEffectView::slotDisable(bool disable)
{
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    std::static_pointer_cast<AbstractEffectItem>(m_model)->markEnabled(effectName, !disable);
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    emit m_view->initKeyframeView(!disable);
    emit activateEffect(m_model);
}

void CollapsibleEffectView::updateScene()
{
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    emit m_view->initKeyframeView(m_model->isEnabled());
}

void CollapsibleEffectView::slotDeleteEffect()
{
    emit deleteEffect(m_model);
}

void CollapsibleEffectView::slotEffectUp()
{
    emit moveEffect(qMax(0, m_model->row() - 1), m_model);
}

void CollapsibleEffectView::slotEffectDown()
{
    emit moveEffect(m_model->row() + 2, m_model);
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
    // Get base effect xml
    QString effectId = m_model->getAssetId();
    QDomElement effect = EffectsRepository::get()->getXml(effectId);
    // Adjust param values
    QVector<QPair<QString, QVariant>> currentValues = m_model->getAllParameters();
    QMap<QString, QString> values;
    for (const auto &param : qAsConst(currentValues)) {
        values.insert(param.first, param.second.toString());
    }
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        const QString paramName = params.item(i).toElement().attribute("name");
        const QString paramType = params.item(i).toElement().attribute("type");
        if (paramType == QLatin1String("fixed") || !values.contains(paramName)) {
            continue;
        }
        params.item(i).toElement().setAttribute(QStringLiteral("value"), values.value(paramName));
    }
    doc.appendChild(doc.importNode(effect, true));
    effect = doc.firstChild().toElement();
    effect.removeAttribute(QStringLiteral("kdenlive_ix"));
    effect.setAttribute(QStringLiteral("id"), name);
    QString masterType = effect.attribute(QLatin1String("type"));
    effect.setAttribute(QStringLiteral("type"), (masterType == QLatin1String("audio") || masterType == QLatin1String("customAudio")) ? QStringLiteral("customAudio") : QStringLiteral("customVideo"));

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
    emit reloadEffect(dir.absoluteFilePath(name + QStringLiteral(".xml")));
}

QDomDocument CollapsibleEffectView::toXml() const
{
    QDomDocument doc;
    // Get base effect xml
    QString effectId = m_model->getAssetId();
    // Adjust param values
    QVector<QPair<QString, QVariant>> currentValues = m_model->getAllParameters();

    QDomElement effect = doc.createElement(QStringLiteral("effect"));
    doc.appendChild(effect);
    effect.setAttribute(QStringLiteral("id"), effectId);
    for (const auto &param : qAsConst(currentValues)) {
        QDomElement xmlParam = doc.createElement(QStringLiteral("property"));
        effect.appendChild(xmlParam);
        xmlParam.setAttribute(QStringLiteral("name"), param.first);
        QString value;
        value = param.second.toString();
        QDomText val = doc.createTextNode(value);
        xmlParam.appendChild(val);
    }
    return doc;
}

void CollapsibleEffectView::slotResetEffect()
{
    m_view->resetValues();
}


void CollapsibleEffectView::updateHeight()
{
    if (m_view->height() == widgetFrame->height()) {
        return;
    }
    widgetFrame->setFixedHeight(m_collapse->isActive() ? 0 : m_view->height());
    setFixedHeight(widgetFrame->height() + frame->minimumHeight() + 2 * (contentsMargins().top() + decoframe->lineWidth()));
    emit switchHeight(m_model, height());
}

void CollapsibleEffectView::switchCollapsed(int row)
{
    if (row == m_model->row()) {
        slotSwitch(!m_model->isCollapsed());
    }
}

void CollapsibleEffectView::slotSwitch(bool collapse)
{
    widgetFrame->setFixedHeight(collapse ? 0 : m_view->height());
    setFixedHeight(widgetFrame->height() + frame->minimumHeight() + 2 * (contentsMargins().top() + decoframe->lineWidth()));
    m_model->setCollapsed(collapse);
    emit switchHeight(m_model, height());
}

void CollapsibleEffectView::setGroupIndex(int ix)
{
    Q_UNUSED(ix)
    /*if (m_info.groupIndex == -1 && ix != -1) {
        m_menu->removeAction(m_groupAction);
    } else if (m_info.groupIndex != -1 && ix == -1) {
        m_menu->addAction(m_groupAction);
    }
    m_info.groupIndex = ix;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());*/
}

void CollapsibleEffectView::setGroupName(const QString &groupName){
    Q_UNUSED(groupName)
    /*m_info.groupName = groupName;
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());*/
}

QString CollapsibleEffectView::infoString() const
{
    return QString(); // m_info.toString();
}

void CollapsibleEffectView::removeFromGroup()
{
    /*if (m_info.groupIndex != -1) {
        m_menu->addAction(m_groupAction);
    }
    m_info.groupIndex = -1;
    m_info.groupName.clear();
    m_effect.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
    emit parameterChanged(m_original_effect, m_effect, effectIndex());*/
}

int CollapsibleEffectView::groupIndex() const
{
    return -1; // m_info.groupIndex;
}

int CollapsibleEffectView::effectIndex() const
{
    if (m_effect.isNull()) {
        return -1;
    }
    return m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
}

void CollapsibleEffectView::updateWidget(const ItemInfo &info, const QDomElement &effect)
{
    // cleanup
    /*
    delete m_paramWidget;
    m_paramWidget = nullptr;
    */
    m_effect = effect;
    setupWidget(info);
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
    Q_UNUSED(kf)
    /*
    if (m_paramWidget) {
        m_paramWidget->setActiveKeyframe(kf);
    }
    */
}

void CollapsibleEffectView::setupWidget(const ItemInfo &info)
{
    Q_UNUSED(info)
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
    Q_UNUSED(event)
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList params = kf.split(QLatin1Char('\n'), QString::SkipEmptyParts);
#else
        const QStringList params = kf.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
#endif
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
            event->setDropAction(Qt::MoveAction);
            event->accept();
            emit addEffect(e);
            return;
        }
        // effect dropped from effects list, add it
        e.setAttribute(QStringLiteral("kdenlive_ix"), ix);
        /*if (m_info.groupIndex > -1) {
            // Dropped on a group
            e.setAttribute(QStringLiteral("kdenlive_info"), m_info.toString());
        }*/
        event->setDropAction(Qt::CopyAction);
        event->accept();
        emit addEffect(e);
        return;
    }
    // emit moveEffect(QList<int>() << ix, currentEffectIx, m_info.groupIndex, m_info.groupName);
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
    if (!m_model->isEnabled() || !m_view) {
        return MonitorSceneDefault;
    }
    return m_view->needsMonitorEffectScene();
}

void CollapsibleEffectView::setKeyframes(const QString &tag, const QString &keyframes)
{
    Q_UNUSED(tag)
    Q_UNUSED(keyframes)
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

void CollapsibleEffectView::enableView(bool enabled)
{
    m_enabledButton->setActive(enabled);
    title->setEnabled(!enabled);
    m_colorIcon->setEnabled(!enabled);
    if (enabled) {
        if (KdenliveSettings::disable_effect_parameters()) {
            widgetFrame->setEnabled(false);
        }
    } else {
        widgetFrame->setEnabled(true);
    }
}

void CollapsibleEffectView::blockWheenEvent(bool block)
{
    m_blockWheel = block;
}

