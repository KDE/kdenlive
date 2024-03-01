/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "collapsibleeffectview.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/view/assetparameterview.hpp"
#include "assets/view/widgets/colorwheel.h"
#include "assets/view/widgets/keyframewidget.hpp"
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
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QProgressBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KDualAction>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentDirs>
#include <KSqueezedTextLabel>
#include <QComboBox>

CollapsibleEffectView::CollapsibleEffectView(const QString &effectName, const std::shared_ptr<EffectItemModel> &effectModel, QSize frameSize, QWidget *parent)
    : AbstractCollapsibleWidget(parent)
    , m_view(nullptr)
    , m_model(effectModel)
    , m_blockWheel(false)
    , m_dragging(false)
{
    QString effectId = effectModel->getAssetId();
    buttonUp->setIcon(QIcon::fromTheme(QStringLiteral("selection-raise")));
    buttonUp->setToolTip(i18n("Move effect up"));
    buttonUp->setWhatsThis(xi18nc(
        "@info:whatsthis", "Moves the effect above the one right above it. Effects are handled sequentially from top to bottom so sequence is important."));
    buttonDown->setIcon(QIcon::fromTheme(QStringLiteral("selection-lower")));
    buttonDown->setToolTip(i18n("Move effect down"));
    buttonDown->setWhatsThis(xi18nc(
        "@info:whatsthis", "Moves the effect below the one right below it. Effects are handled sequentially from top to bottom so sequence is important."));
    buttonDel->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    buttonDel->setToolTip(i18n("Delete effect"));
    buttonDel->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the effect from the effect stack."));

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
        // Effect has no parameter
        m_collapse->setInactiveIcon(QIcon::fromTheme(QStringLiteral("tools-wizard")));
        collapseButton->setEnabled(false);
    } else {
        m_collapse->setInactiveIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
    }

    auto *l = static_cast<QHBoxLayout *>(frame->layout());
    title = new KSqueezedTextLabel(this);
    title->setToolTip(effectName);
    title->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    l->insertWidget(1, title);

    keyframesButton->setIcon(QIcon::fromTheme(QStringLiteral("keyframe")));
    keyframesButton->setCheckable(true);
    keyframesButton->setToolTip(i18n("Enable Keyframes"));

    m_keyframesButton = new KDualAction(i18n("Hide Keyframes"), i18n("Show Keyframes"), this);
    m_keyframesButton->setWhatsThis(xi18nc("@info:whatsthis", "Turns the display of the keyframe ruler on."));
    m_keyframesButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-disable")));
    m_keyframesButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("keyframe")));
    keyframesButton->setDefaultAction(m_keyframesButton);
    connect(m_keyframesButton, &KDualAction::activeChangedByUser, this, &CollapsibleEffectView::slotHideKeyframes);
    connect(m_model.get(), &AssetParameterModel::hideKeyframesChange, this, &CollapsibleEffectView::enableHideKeyframes);

    // Enable button
    m_enabledButton = new KDualAction(i18n("Disable Effect"), i18n("Enable Effect"), this);
    m_enabledButton->setWhatsThis(xi18nc("@info:whatsthis", "Disables the effect. Useful to compare before and after settings."));
    m_enabledButton->setWhatsThis(xi18nc("@info:whatsthis", "Enables the effect. Useful to compare before and after settings."));
    m_enabledButton->setActiveIcon(QIcon::fromTheme(QStringLiteral("hint")));
    m_enabledButton->setInactiveIcon(QIcon::fromTheme(QStringLiteral("visibility")));
    enabledButton->setDefaultAction(m_enabledButton);
    connect(m_model.get(), &AssetParameterModel::enabledChange, this, &CollapsibleEffectView::enableView);
    connect(m_model.get(), &AssetParameterModel::showEffectZone, this, [=](ObjectId id, QPair<int, int> inOut, bool checked) {
        m_inOutButton->setChecked(checked);
        zoneFrame->setFixedHeight(checked ? frame->height() : 0);
        slotSwitch(m_collapse->isActive());
        if (checked) {
            QSignalBlocker bk(m_inPos);
            QSignalBlocker bk2(m_outPos);
            m_inPos->setValue(inOut.first);
            m_outPos->setValue(inOut.second);
        }
        Q_EMIT showEffectZone(id, inOut, checked);
    });
    m_groupAction = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), i18n("Create Group"), this);
    connect(m_groupAction, &QAction::triggered, this, &CollapsibleEffectView::slotCreateGroup);

    // In /out effect button
    auto *layZone = new QHBoxLayout(zoneFrame);
    layZone->setContentsMargins(0, 0, 0, 0);
    layZone->setSpacing(0);
    QLabel *in = new QLabel(i18n("In:"), this);
    in->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    layZone->addWidget(in);
    auto *setIn = new QToolButton(this);
    setIn->setIcon(QIcon::fromTheme(QStringLiteral("zone-in")));
    setIn->setAutoRaise(true);
    setIn->setToolTip(i18n("Set zone in"));
    setIn->setWhatsThis(xi18nc("@info:whatsthis", "Sets the current frame/playhead position as start of the zone."));
    layZone->addWidget(setIn);
    m_inPos = new TimecodeDisplay(this);
    layZone->addWidget(m_inPos);
    layZone->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum));
    QLabel *out = new QLabel(i18n("Out:"), this);
    out->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    layZone->addWidget(out);
    auto *setOut = new QToolButton(this);
    setOut->setIcon(QIcon::fromTheme(QStringLiteral("zone-out")));
    setOut->setAutoRaise(true);
    setOut->setToolTip(i18n("Set zone out"));
    setOut->setWhatsThis(xi18nc("@info:whatsthis", "Sets the current frame/playhead position as end of the zone."));
    layZone->addWidget(setOut);
    m_outPos = new TimecodeDisplay(this);
    layZone->addWidget(m_outPos);

    connect(setIn, &QToolButton::clicked, this, [=]() {
        if (m_model->getOwnerId().type == KdenliveObjectType::BinClip) {
            m_outPos->setValue(pCore->getMonitor(Kdenlive::ClipMonitor)->position());
        } else {
            m_inPos->setValue(pCore->getMonitorPosition());
        }
        updateEffectZone();
    });
    connect(setOut, &QToolButton::clicked, this, [=]() {
        if (m_model->getOwnerId().type == KdenliveObjectType::BinClip) {
            m_outPos->setValue(pCore->getMonitor(Kdenlive::ClipMonitor)->position());
        } else {
            m_outPos->setValue(pCore->getMonitorPosition());
        }
        updateEffectZone();
    });

    m_inOutButton = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-width")), i18n("Use effect zone"), this);
    m_inOutButton->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the display of the effect zone."));
    m_inOutButton->setCheckable(true);
    inOutButton->setDefaultAction(m_inOutButton);
    m_inOutButton->setChecked(m_model->hasForcedInOut());
    if (m_inOutButton->isChecked()) {
        QPair<int, int> inOut = m_model->getInOut();
        m_inPos->setValue(inOut.first);
        m_outPos->setValue(inOut.second);
    } else {
        zoneFrame->setFixedHeight(0);
    }
    inOutButton->setVisible(m_model->getOwnerId().type != KdenliveObjectType::TimelineClip);
    connect(m_inPos, &TimecodeDisplay::timeCodeEditingFinished, this, &CollapsibleEffectView::updateEffectZone);
    connect(m_outPos, &TimecodeDisplay::timeCodeEditingFinished, this, &CollapsibleEffectView::updateEffectZone);
    connect(m_inOutButton, &QAction::triggered, this, &CollapsibleEffectView::switchInOut);

    title->setText(effectName);
    frame->setMinimumHeight(collapseButton->sizeHint().height());

    m_view = new AssetParameterView(this);
    const std::shared_ptr<AssetParameterModel> effectParamModel = std::static_pointer_cast<AssetParameterModel>(effectModel);
    m_view->setModel(effectParamModel, frameSize);
    connect(m_view, &AssetParameterView::seekToPos, this, &AbstractCollapsibleWidget::seekToPos);
    connect(m_view, &AssetParameterView::activateEffect, this, [this]() {
        if (!decoframe->property("active").toBool()) {
            // Activate effect if not already active
            Q_EMIT activateEffect(m_model->row());
        }
    });

    if (effectModel->rowCount() == 0) {
        // Effect has no parameter
        m_view->setVisible(false);
    }

    connect(m_view, &AssetParameterView::updateHeight, this, &CollapsibleEffectView::updateHeight);
    connect(this, &CollapsibleEffectView::refresh, m_view, &AssetParameterView::slotRefresh);
    keyframesButton->setVisible(m_view->keyframesAllowed());
    auto *lay = new QVBoxLayout(widgetFrame);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_view);

    if (!effectParamModel->hasMoreThanOneKeyframe()) {
        // No keyframe or only one, allow hiding
        bool hideByDefault = effectParamModel->data(effectParamModel->index(0, 0), AssetParameterModel::HideKeyframesFirstRole).toBool();
        if (hideByDefault && m_model->keyframesHiddenUnset()) {
            m_model->setKeyframesHidden(true);
        }
    }

    if (m_model->isKeyframesHidden()) {
        m_view->toggleKeyframes(false);
        m_keyframesButton->setActive(true);
    }
    // Presets
    presetButton->setIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
    presetButton->setMenu(m_view->presetMenu());
    presetButton->setToolTip(i18n("Presets"));
    presetButton->setWhatsThis(xi18nc("@info:whatsthis", "Opens a list of advanced options to manage presets for the effect."));

    connect(saveEffectButton, &QAbstractButton::clicked, this, &CollapsibleEffectView::slotSaveEffect);
    saveEffectButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    saveEffectButton->setToolTip(i18n("Save effect"));

    if (!effectModel->isEnabled()) {
        title->setEnabled(false);
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
    widgetFrame->setFixedHeight(int(m_view->contentHeight() * value));
}

void CollapsibleEffectView::slotCreateGroup()
{
    Q_EMIT createGroup(m_model);
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
        Q_EMIT createRegion(effectIndex(), d->selectedUrls().first());
    }
    delete d;
}

void CollapsibleEffectView::slotUnGroup()
{
    Q_EMIT unGroup(this);
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
            return false;
        }
        if (qobject_cast<QAbstractSpinBox *>(o)) {
            if (m_blockWheel && !qobject_cast<QAbstractSpinBox *>(o)->hasFocus()) {
                return true;
            }
            return false;
        }
        if (qobject_cast<QComboBox *>(o)) {
            if (qobject_cast<QComboBox *>(o)->focusPolicy() == Qt::WheelFocus) {
                return false;
            }
            return true;
        }
        if (qobject_cast<QProgressBar *>(o)) {
            if (!qobject_cast<QProgressBar *>(o)->hasFocus()) {
                return true;
            }
            return false;
        }
        if (qobject_cast<WheelContainer *>(o)) {
            if (!qobject_cast<WheelContainer *>(o)->hasFocus()) {
                return true;
            }
            return false;
        }
        if (qobject_cast<KeyframeView *>(o)) {
            if (!qobject_cast<KeyframeView *>(o)->hasFocus()) {
                return true;
            }
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
    decoframe->setProperty("active", active);
    decoframe->setStyleSheet(decoframe->styleSheet());
    if (active) {
        pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    }
    Q_EMIT m_view->initKeyframeView(active);
    if (m_inOutButton->isChecked()) {
        Q_EMIT showEffectZone(m_model->getOwnerId(), m_model->getInOut(), true);
    } else {
        Q_EMIT showEffectZone(m_model->getOwnerId(), {0, 0}, false);
    }
}

void CollapsibleEffectView::mousePressEvent(QMouseEvent *e)
{
    qDebug() << "XXXX COLLAPSIBLE PRESS EVENT....";
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_dragStart = e->globalPos();
#else
    m_dragStart = e->globalPosition().toPoint();
#endif
    m_dragging = false;
    if (!decoframe->property("active").toBool()) {
        // Activate effect if not already active
        Q_EMIT activateEffect(m_model->row());
    }
    QWidget::mousePressEvent(e);
}

void CollapsibleEffectView::wheelEvent(QWheelEvent *e)
{
    if (m_blockWheel) {
        // initiating a wheel event in an empty space will clear focus
        setFocus();
    }
    QWidget::wheelEvent(e);
}

void CollapsibleEffectView::mouseMoveEvent(QMouseEvent *e)
{
    qDebug() << "XXXX COLLAPSIBLE MOVE EVENT....";
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!m_dragging && (e->globalPos() - m_dragStart).manhattanLength() > QApplication::startDragDistance()) {
#else
    if (!m_dragging && (e->globalPosition().toPoint() - m_dragStart).manhattanLength() > QApplication::startDragDistance()) {
#endif
        m_dragging = true;
        QPixmap pix = frame->grab();
        Q_EMIT startDrag(pix, m_model->getAssetId(), m_model->getOwnerId(), m_model->row());
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
    qDebug() << "XXXX COLLAPSIBLE RELASE EVENT....";
    m_dragStart = QPoint();
    m_dragging = false;
    if (!decoframe->property("active").toBool()) {
        // Q_EMIT activateEffect(effectIndex());
    }
    QWidget::mouseReleaseEvent(event);
}

void CollapsibleEffectView::slotDisable(bool disable)
{
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    std::static_pointer_cast<AbstractEffectItem>(m_model)->markEnabled(effectName, !disable);
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    Q_EMIT m_view->initKeyframeView(!disable);
    Q_EMIT activateEffect(m_model->row());
}

void CollapsibleEffectView::updateScene()
{
    pCore->getMonitor(m_model->monitorId)->slotShowEffectScene(needsMonitorEffectScene());
    Q_EMIT m_view->initKeyframeView(m_model->isEnabled());
}

void CollapsibleEffectView::slotDeleteEffect()
{
    Q_EMIT deleteEffect(m_model);
}

void CollapsibleEffectView::slotEffectUp()
{
    Q_EMIT moveEffect(qMax(0, m_model->row() - 1), m_model);
}

void CollapsibleEffectView::slotEffectDown()
{
    Q_EMIT moveEffect(m_model->row() + 2, m_model);
}

void CollapsibleEffectView::slotSaveEffect()
{
    QDialog dialog(this);
    QFormLayout form(&dialog);

    dialog.setWindowTitle(i18nc("@title:window", "Save Effect"));

    auto *effectName = new QLineEdit(&dialog);
    auto *descriptionBox = new QTextEdit(&dialog);
    form.addRow(i18n("Name:"), effectName);
    form.addRow(i18n("Comments:"), descriptionBox);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);

    QObject::connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = effectName->text();
        QString enteredDescription = descriptionBox->toPlainText();
        if (name.trimmed().isEmpty()) {
            KMessageBox::error(this, i18n("No name provided, effect not saved."));
            return;
        }
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }

        if (dir.exists(name + QStringLiteral(".xml")))
            if (KMessageBox::questionTwoActions(this, i18n("File %1 already exists.\nDo you want to overwrite it?", name + QStringLiteral(".xml")), {},
                                                KStandardGuiItem::overwrite(), KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
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
            if (paramType == QLatin1String("multiswitch")) {
                // Multiswitch param value is not updated on change, fo fetch real value now
                QString val = m_model->getParamFromName(paramName).toString();
                params.item(i).toElement().setAttribute(QStringLiteral("value"), val);
                continue;
            }
            params.item(i).toElement().setAttribute(QStringLiteral("value"), values.value(paramName));
        }
        doc.appendChild(doc.importNode(effect, true));
        effect = doc.firstChild().toElement();
        effect.removeAttribute(QStringLiteral("kdenlive_ix"));
        QString namedId = name;
        QString sourceId = effect.attribute("id");
        // When saving an effect as custom, it might be necessary to keep track of the original
        // effect id as it is sometimes used in Kdenlive to trigger special behaviors
        if (sourceId.startsWith(QStringLiteral("fade_to_"))) {
            namedId.prepend(QStringLiteral("fade_to_"));
        } else if (sourceId.startsWith(QStringLiteral("fade_from_"))) {
            namedId.prepend(QStringLiteral("fade_from_"));
        }
        if (sourceId.startsWith(QStringLiteral("fadein"))) {
            namedId.prepend(QStringLiteral("fadein_"));
        }
        if (sourceId.startsWith(QStringLiteral("fadeout"))) {
            namedId.prepend(QStringLiteral("fadeout_"));
        }
        effect.setAttribute(QStringLiteral("id"), namedId);
        effect.setAttribute(QStringLiteral("type"), m_model->isAudio() ? QStringLiteral("customAudio") : QStringLiteral("customVideo"));

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

        if (!enteredDescription.trimmed().isEmpty()) {
            QDomElement root = doc.documentElement();
            QDomElement nodelist = root.firstChildElement("description");
            QDomElement newNodeTag = doc.createElement(QString("description"));
            QDomText text = doc.createTextNode(enteredDescription);
            newNodeTag.appendChild(text);
            root.replaceChild(newNodeTag, nodelist);
        }

        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
#endif
            out << doc.toString();
        } else {
            KMessageBox::error(this, i18n("Cannot write to file %1", file.fileName()));
        }
        file.close();
        Q_EMIT reloadEffect(dir.absoluteFilePath(name + QStringLiteral(".xml")));
    }
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
    setFixedHeight(widgetFrame->height() + frame->minimumHeight() + zoneFrame->minimumHeight() + 2 * (contentsMargins().top() + decoframe->lineWidth()));
    Q_EMIT switchHeight(m_model, height());
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
    zoneFrame->setFixedHeight(collapse || !m_inOutButton->isChecked() ? 0 : frame->height());
    setFixedHeight(widgetFrame->height() + frame->minimumHeight() + zoneFrame->height() + 2 * (contentsMargins().top() + decoframe->lineWidth()));
    m_model->setCollapsed(collapse);
    Q_EMIT switchHeight(m_model, height());
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

void CollapsibleEffectView::setGroupName(const QString &groupName)
{
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
    Q_EMIT parameterChanged(m_original_effect, m_effect, effectIndex());*/
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
    Q_UNUSED(info)
    // cleanup
    /*
    delete m_paramWidget;
    m_paramWidget = nullptr;
    */
    m_effect = effect;
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
    Q_EMIT parameterChanged(m_original_effect, m_effect, effectIndex());
}

void CollapsibleEffectView::slotSyncEffectsPos(int pos)
{
    Q_EMIT syncEffectsPos(pos);
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
        const QStringList params = kf.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &param : params) {
            keyframes.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
        }
    } else {
        keyframes.insert(kf.section(QLatin1Char('='), 0, 0), kf.section(QLatin1Char('='), 1));
    }
    Q_EMIT importClipKeyframes(AVWidget, m_itemInfo, m_effect.cloneNode().toElement(), keyframes);
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
            Q_EMIT addEffect(e);
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
        Q_EMIT addEffect(e);
        return;
    }
    // Q_EMIT moveEffect(QList<int>() << ix, currentEffectIx, m_info.groupIndex, m_info.groupName);
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
    Q_EMIT importClipKeyframes(AVWidget, m_itemInfo, m_effect.cloneNode().toElement(), QMap<QString, QString>());
}

void CollapsibleEffectView::enableView(bool enabled)
{
    m_enabledButton->setActive(enabled);
    title->setEnabled(!enabled);
    if (enabled) {
        if (KdenliveSettings::disable_effect_parameters()) {
            widgetFrame->setEnabled(false);
        }
    } else {
        widgetFrame->setEnabled(true);
    }
}

void CollapsibleEffectView::enableHideKeyframes(bool enabled)
{
    m_keyframesButton->setActive(enabled);
    m_view->toggleKeyframes(!enabled);
}

void CollapsibleEffectView::blockWheelEvent(bool block)
{
    m_blockWheel = block;
    Qt::FocusPolicy policy = block ? Qt::StrongFocus : Qt::WheelFocus;
    for (QSpinBox *sp : findChildren<QSpinBox *>()) {
        sp->installEventFilter(this);
        sp->setFocusPolicy(policy);
    }
    for (QComboBox *cb : findChildren<QComboBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(policy);
    }
    for (QProgressBar *cb : findChildren<QProgressBar *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(policy);
    }
    for (WheelContainer *cb : findChildren<WheelContainer *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(policy);
    }
    for (QDoubleSpinBox *cb : findChildren<QDoubleSpinBox *>()) {
        cb->installEventFilter(this);
        cb->setFocusPolicy(policy);
    }
    for (KeyframeWidget *cb : findChildren<KeyframeWidget *>()) {
        for (KeyframeView *cb2 : cb->findChildren<KeyframeView *>()) {
            cb2->installEventFilter(this);
            cb2->setFocusPolicy(policy);
        }
    }
}

void CollapsibleEffectView::switchInOut(bool checked)
{
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    QPair<int, int> inOut = m_model->getInOut();
    zoneFrame->setFixedHeight(checked ? frame->height() : 0);
    slotSwitch(m_collapse->isActive());
    if (inOut.first == inOut.second || !checked) {
        ObjectId owner = m_model->getOwnerId();
        switch (owner.type) {
        case KdenliveObjectType::TimelineClip: {
            int in = pCore->getItemIn(owner);
            inOut = {in, in + pCore->getItemDuration(owner)};
            break;
        }
        case KdenliveObjectType::TimelineTrack:
        case KdenliveObjectType::Master: {
            if (!checked) {
                inOut = {0, 0};
            } else {
                int in = pCore->getMonitorPosition();
                inOut = {in, in + pCore->getDurationFromString(KdenliveSettings::transition_duration())};
            }
            break;
        }
        default:
            qDebug() << "== UNSUPPORTED ITEM TYPE FOR EFFECT RANGE: " << int(owner.type);
            break;
        }
    }
    qDebug() << "==== SWITCHING IN / OUT: " << inOut.first << "-" << inOut.second;
    if (inOut.first > -1) {
        m_model->setInOut(effectName, inOut, checked, true);
        m_inPos->setValue(inOut.first);
        m_outPos->setValue(inOut.second);
    }
}

void CollapsibleEffectView::updateInOut(QPair<int, int> inOut, bool withUndo)
{
    if (!m_inOutButton->isChecked()) {
        qDebug() << "=== CANNOT UPDATE ZONE ON EFFECT!!!";
        return;
    }
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    if (inOut.first > -1) {
        m_model->setInOut(effectName, inOut, true, withUndo);
        m_inPos->setValue(inOut.first);
        m_outPos->setValue(inOut.second);
    }
}

void CollapsibleEffectView::updateEffectZone()
{
    QString effectId = m_model->getAssetId();
    QString effectName = EffectsRepository::get()->getName(effectId);
    QPair<int, int> inOut = {m_inPos->getValue(), m_outPos->getValue()};
    m_model->setInOut(effectName, inOut, true, true);
}

void CollapsibleEffectView::slotNextKeyframe()
{
    Q_EMIT m_view->nextKeyframe();
}

void CollapsibleEffectView::slotPreviousKeyframe()
{
    Q_EMIT m_view->previousKeyframe();
}

void CollapsibleEffectView::addRemoveKeyframe()
{
    Q_EMIT m_view->addRemoveKeyframe();
}

void CollapsibleEffectView::slotHideKeyframes(bool hide)
{
    m_model->setKeyframesHidden(hide);
}

void CollapsibleEffectView::sendStandardCommand(int command)
{
    m_view->sendStandardCommand(command);
}
