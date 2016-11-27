/***************************************************************************
                          geomeytrval.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "geometryval.h"
#include "graphicsscenerectmove.h"
#include "kdenlivesettings.h"
#include "utils/KoIconUtils.h"
#include "klocalizedstring.h"

#include <QDebug>
#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGraphicsRectItem>
#include <QMenu>
#include <QTimer>

#include <mlt++/Mlt.h>

Geometryval::Geometryval(const Mlt::Profile *profile, const Timecode &t, const QPoint &frame_size, int startPoint, QWidget* parent) :
        QWidget(parent),
        m_profile(profile),
        m_paramRect(Q_NULLPTR),
        m_geom(Q_NULLPTR),
        m_path(Q_NULLPTR),
        m_fixedMode(false),
        m_frameSize(frame_size),
        m_startPoint(startPoint),
        m_timePos(t)
{
    setupUi(this);
    toolbarlayout->addWidget(&m_timePos);
    toolbarlayout->insertStretch(-1);

    QVBoxLayout* vbox = new QVBoxLayout(widget);
    m_sceneview = new QGraphicsView(this);
    m_sceneview->setBackgroundBrush(QBrush(Qt::black));
    vbox->addWidget(m_sceneview);
    vbox->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vbox2 = new QVBoxLayout(keyframeWidget);
    m_helper = new KeyframeHelper(this);
    vbox2->addWidget(m_helper);
    vbox2->setContentsMargins(0, 0, 0, 0);

    connect(m_helper, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));
    connect(m_helper, SIGNAL(keyframeMoved(int)), this, SLOT(slotKeyframeMoved(int)));
    connect(m_helper, SIGNAL(addKeyframe(int)), this, SLOT(slotAddFrame(int)));
    connect(m_helper, SIGNAL(removeKeyframe(int)), this, SLOT(slotDeleteFrame(int)));

    m_scene = new GraphicsSceneRectMove(this);
    m_scene->setTool(TITLE_SELECT);
    m_sceneview->setScene(m_scene);
    m_dar = (m_profile->height() * m_profile->dar()) / (double) m_profile->width();

    m_realWidth = (int)(profile->height() * profile->dar() + 0.5);
    QGraphicsRectItem *frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_realWidth, profile->height()));
    frameBorder->setZValue(-1100);
    frameBorder->setBrush(QColor(255, 255, 0, 30));
    frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    m_scene->addItem(frameBorder);

    buttonNext->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")));
    buttonNext->setToolTip(i18n("Go to next keyframe"));
    buttonPrevious->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")));
    buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    buttonAdd->setIcon(KoIconUtils::themedIcon(QStringLiteral("document-new")));
    buttonAdd->setToolTip(i18n("Add keyframe"));
    buttonDelete->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-delete")));
    buttonDelete->setToolTip(i18n("Delete keyframe"));

    m_configMenu = new QMenu(i18n("Misc..."), this);
    buttonMenu->setMenu(m_configMenu);
    buttonMenu->setPopupMode(QToolButton::MenuButtonPopup);

    m_editOptions = m_configMenu->addAction(KoIconUtils::themedIcon(QStringLiteral("system-run")), i18n("Show/Hide options"));
    m_editOptions->setCheckable(true);
    buttonMenu->setDefaultAction(m_editOptions);
    connect(m_editOptions, SIGNAL(triggered()), this, SLOT(slotSwitchOptions()));
    slotSwitchOptions();

    m_reset = m_configMenu->addAction(KoIconUtils::themedIcon(QStringLiteral("view-refresh")), i18n("Reset"), this, SLOT(slotResetPosition()));

    m_syncAction = m_configMenu->addAction(i18n("Sync timeline cursor"), this, SLOT(slotSyncCursor()));
    m_syncAction->setCheckable(true);
    m_syncAction->setChecked(KdenliveSettings::transitionfollowcursor());

    //scene->setSceneRect(0, 0, profile->width * 2, profile->height * 2);
    //view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    const double sc = 100.0 / profile->height() * 0.8;
    QRectF srect = m_sceneview->sceneRect();
    m_sceneview->setSceneRect(srect.x(), -srect.height() / 3 + 10, srect.width(), srect.height() + srect.height() / 3 * 2 - 10);
    m_scene->setZoom(sc);
    m_sceneview->centerOn(frameBorder);
    m_sceneview->setMouseTracking(true);
    connect(buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
    connect(buttonDelete , SIGNAL(clicked()) , this , SLOT(slotDeleteFrame()));
    connect(buttonAdd , SIGNAL(clicked()) , this , SLOT(slotAddFrame()));
    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateTransitionProperties()));

    buttonhcenter->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-hor")));
    buttonhcenter->setToolTip(i18n("Align item horizontally"));
    buttonvcenter->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-vert")));
    buttonvcenter->setToolTip(i18n("Align item vertically"));
    buttontop->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-top")));
    buttontop->setToolTip(i18n("Align item to top"));
    buttonbottom->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-bottom")));
    buttonbottom->setToolTip(i18n("Align item to bottom"));
    buttonright->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-right")));
    buttonright->setToolTip(i18n("Align item to right"));
    buttonleft->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-align-left")));
    buttonleft->setToolTip(i18n("Align item to left"));

    connect(buttonhcenter, SIGNAL(clicked()), this, SLOT(slotAlignHCenter()));
    connect(buttonvcenter, SIGNAL(clicked()), this, SLOT(slotAlignVCenter()));
    connect(buttontop, SIGNAL(clicked()), this, SLOT(slotAlignTop()));
    connect(buttonbottom, SIGNAL(clicked()), this, SLOT(slotAlignBottom()));
    connect(buttonright, SIGNAL(clicked()), this, SLOT(slotAlignRight()));
    connect(buttonleft, SIGNAL(clicked()), this, SLOT(slotAlignLeft()));
    connect(spinX, SIGNAL(valueChanged(int)), this, SLOT(slotGeometryX(int)));
    connect(spinY, SIGNAL(valueChanged(int)), this, SLOT(slotGeometryY(int)));
    connect(spinWidth, SIGNAL(valueChanged(int)), this, SLOT(slotGeometryWidth(int)));
    connect(spinHeight, SIGNAL(valueChanged(int)), this, SLOT(slotGeometryHeight(int)));
    connect(spinResize, SIGNAL(editingFinished()), this, SLOT(slotResizeCustom()));
    connect(buttonResize, SIGNAL(clicked()), this, SLOT(slotResizeOriginal()));

    connect(this, SIGNAL(parameterChanged()), this, SLOT(slotUpdateGeometry()));
}


Geometryval::~Geometryval()
{
    m_scene->disconnect();
    delete m_syncAction;
    delete m_configMenu;
    delete m_paramRect;
    delete m_path;
    delete m_helper;
    delete m_geom;
    delete m_sceneview;
    delete m_scene;
}


void Geometryval::slotAlignHCenter()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos((m_realWidth - m_paramRect->rect().width()) / 2, m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignVCenter()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(m_paramRect->pos().x(), (m_profile->height() - m_paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignTop()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(m_paramRect->pos().x(), 0);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignBottom()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(m_paramRect->pos().x(), m_profile->height() - m_paramRect->rect().height());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignLeft()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(0, m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignRight()
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(m_realWidth - m_paramRect->rect().width(), m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResizeOriginal()
{
    if (!keyframeSelected())
        return;
    if (m_frameSize.isNull())
        m_paramRect->setRect(0, 0, m_realWidth, m_profile->height());
    else
        m_paramRect->setRect(0, 0, m_frameSize.x(), m_frameSize.y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResizeCustom()
{
    if (!keyframeSelected())
        return;
    int value = spinResize->value();
    m_paramRect->setRect(0, 0, m_realWidth * value / 100, m_profile->height() * value / 100);
    slotUpdateTransitionProperties();
}

void Geometryval::slotTransparencyChanged(int transp)
{
    int pos = m_timePos.getValue();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    item.mix(transp);
    m_paramRect->setBrush(QColor(255, 0, 0, transp));
    m_geom->insert(item);
    emit parameterChanged();
}

void Geometryval::slotSyncCursor()
{
    KdenliveSettings::setTransitionfollowcursor(m_syncAction->isChecked());
}

void Geometryval::updateTimecodeFormat()
{
    m_timePos.slotUpdateTimeCodeFormat();
}

void Geometryval::slotPositionChanged(int pos, bool seek)
{
    if (pos == -1) {
        pos = m_timePos.getValue();
    }
    if (seek && KdenliveSettings::transitionfollowcursor()) emit seekToPos(pos + m_startPoint);
    m_timePos.setValue(pos);
    //spinPos->setValue(pos);
    m_helper->blockSignals(true);
    m_helper->setValue(pos);
    m_helper->blockSignals(false);
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor, adjust buttons
        buttonAdd->setEnabled(true);
        buttonDelete->setEnabled(false);
        widget->setEnabled(false);
        spinTransp->setEnabled(false);
        frameOptions->setEnabled(false);
        m_reset->setEnabled(false);
    } else {
        buttonAdd->setEnabled(false);
        buttonDelete->setEnabled(true);
        widget->setEnabled(true);
        spinTransp->setEnabled(true);
        frameOptions->setEnabled(true);
        m_reset->setEnabled(true);
    }

    m_paramRect->setPos(item.x() * m_dar, item.y());
    m_paramRect->setRect(0, 0, item.w() * m_dar, item.h());
    spinTransp->setValue(item.mix());
    m_paramRect->setBrush(QColor(255, 0, 0, item.mix()));
    slotUpdateGeometry();
}

void Geometryval::slotDeleteFrame(int pos)
{
    // check there is more than one keyframe
    Mlt::GeometryItem item;
    int frame = m_timePos.getValue();

    if (pos == -1) pos = frame;
    int error = m_geom->next_key(&item, pos + 1);
    if (error) {
        error = m_geom->prev_key(&item, pos - 1);
        if (error || item.frame() == pos) return;
    }

    m_geom->remove(frame);
    buttonAdd->setEnabled(true);
    buttonDelete->setEnabled(false);
    widget->setEnabled(false);
    spinTransp->setEnabled(false);
    frameOptions->setEnabled(false);
    m_reset->setEnabled(false);
    m_helper->update();
    slotPositionChanged(pos, false);
    updateTransitionPath();
    emit parameterChanged();
}

void Geometryval::slotAddFrame(int pos)
{
    int frame = m_timePos.getValue();
    if (pos == -1) pos = frame;
    Mlt::GeometryItem item;
    item.frame(pos);
    QRectF r = m_paramRect->rect().normalized();
    QPointF rectpos = m_paramRect->pos();
    item.x(rectpos.x() / m_dar);
    item.y(rectpos.y());
    item.w(r.width() / m_dar);
    item.h(r.height());
    item.mix(spinTransp->value());
    m_geom->insert(item);
    buttonAdd->setEnabled(false);
    buttonDelete->setEnabled(true);
    widget->setEnabled(true);
    spinTransp->setEnabled(true);
    frameOptions->setEnabled(true);
    m_reset->setEnabled(true);
    m_helper->update();
    emit parameterChanged();
}

void Geometryval::slotNextFrame()
{
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, m_helper->value() + 1);
    int pos;
    //qDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) {
        // Go to end
        pos = m_helper->frameLength;
    } else pos = item.frame();
    m_timePos.setValue(pos);
    slotPositionChanged();
}

void Geometryval::slotPreviousFrame()
{
    Mlt::GeometryItem item;
    int error = m_geom->prev_key(&item, m_helper->value() - 1);
    //qDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    m_timePos.setValue(pos);
    slotPositionChanged();
}


QString Geometryval::getValue() const
{
    return m_geom->serialise();
}

void Geometryval::setupParam(const QDomElement par, int minFrame, int maxFrame)
{
    QString val = par.attribute(QStringLiteral("value"));
    if (par.attribute(QStringLiteral("fixed")) == QLatin1String("1")) {
        m_fixedMode = true;
        buttonPrevious->setHidden(true);
        buttonNext->setHidden(true);
        buttonDelete->setHidden(true);
        buttonAdd->setHidden(true);
        spinTransp->setMaximum(500);
        label_pos->setHidden(true);
        m_helper->setHidden(true);
        m_timePos.setHidden(true);
    }
    if (par.attribute(QStringLiteral("opacity")) == QLatin1String("false")) {
        label_opacity->setHidden(true);
        spinTransp->setHidden(true);
    }
    if (m_geom)
        m_geom->parse(val.toUtf8().data(), maxFrame - minFrame, m_profile->width(), m_profile->height());
    else
        m_geom = new Mlt::Geometry(val.toUtf8().data(), maxFrame - minFrame, m_profile->width(), m_profile->height());

    ////qDebug() << " / / UPDATING TRANSITION VALUE: " << m_geom->serialise();
    //read param her and set rect
    if (!m_fixedMode) {
        m_helper->setKeyGeometry(m_geom, minFrame, maxFrame);
        m_helper->update();
        /*QDomDocument doc;
        doc.appendChild(doc.importNode(par, true));
        //qDebug() << "IMPORTED TRANS: " << doc.toString();*/
        if (m_path == Q_NULLPTR) {
            m_path = new QGraphicsPathItem();
            m_path->setPen(QPen(Qt::red));
            m_scene->addItem(m_path);
        }
        updateTransitionPath();
    }
    Mlt::GeometryItem item;

    m_geom->fetch(&item, 0);
    delete m_paramRect;
    m_paramRect = new QGraphicsRectItem(QRectF(0, 0, item.w() * m_dar, item.h()));
    m_paramRect->setPos(item.x() * m_dar, item.y());
    m_paramRect->setZValue(0);
    m_paramRect->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    m_paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));
    m_scene->addItem(m_paramRect);
    slotPositionChanged(0, false);
    slotUpdateGeometry();
    if (!m_fixedMode) {
        m_timePos.setRange(0, maxFrame - minFrame - 1);
        connect(&m_timePos, SIGNAL(timeCodeEditingFinished()), this , SLOT(slotPositionChanged()));
    }
    connect(spinTransp, SIGNAL(valueChanged(int)), this , SLOT(slotTransparencyChanged(int)));
}

void Geometryval::slotSyncPosition(int relTimelinePos)
{
    if (m_timePos.maximum() > 0 && KdenliveSettings::transitionfollowcursor()) {
        relTimelinePos = qMax(0, relTimelinePos);
        relTimelinePos = qMin(relTimelinePos, m_timePos.maximum());
        if (relTimelinePos != m_timePos.getValue())
            slotPositionChanged(relTimelinePos, false);
    }
}

void Geometryval::updateTransitionPath()
{
    if (m_fixedMode) return;
    Mlt::GeometryItem item;
    int pos = 0;
    int counter = 0;
    QPainterPath path;
    while (true) {
        if (m_geom->next_key(&item, pos) == 1) break;
        pos = item.frame();
        if (counter == 0) {
            path.moveTo(item.x() * m_dar + item.w() * m_dar / 2, item.y() + item.h() / 2);
        } else {
            path.lineTo(item.x() * m_dar + item.w() * m_dar / 2, item.y() + item.h() / 2);
        }
        counter++;
        pos++;
    }
    m_path->setPath(path);
}

void Geometryval::slotUpdateTransitionProperties()
{
    int pos = m_timePos.getValue();
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, pos);
    if (error || item.frame() != pos) {
        // no keyframe under cursor
        return;
    }
    QRectF r = m_paramRect->rect().normalized();
    QPointF rectpos = m_paramRect->pos();
    item.x(rectpos.x() / m_dar);
    item.y(rectpos.y());
    item.w(r.width() / m_dar);
    item.h(r.height());
    m_geom->insert(item);
    updateTransitionPath();
    emit parameterChanged();
}

void Geometryval::slotResetPosition()
{
    spinX->setValue(0);
    spinY->setValue(0);

    if (m_frameSize.isNull()) {
        spinWidth->setValue(m_realWidth);
        spinHeight->setValue(m_profile->height());
    } else {
        spinWidth->setValue(m_frameSize.x());
        spinHeight->setValue(m_frameSize.y());
    }
}

void Geometryval::setFrameSize(const QPoint &p)
{
    m_frameSize = p;
}


void Geometryval::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateTransitionProperties();
    QTimer::singleShot(100, this, SIGNAL(parameterChanged()));
}

void Geometryval::slotSwitchOptions()
{
    if (frameOptions->isHidden()) {
        frameOptions->setHidden(false);
        m_editOptions->setChecked(true);
    } else {
        frameOptions->setHidden(true);
        m_editOptions->setChecked(false);
    }
    //adjustSize();
}

void Geometryval::slotGeometryX(int value)
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(value, spinY->value());
    slotUpdateTransitionProperties();
}

void Geometryval::slotGeometryY(int value)
{
    if (!keyframeSelected())
        return;
    m_paramRect->setPos(spinX->value(), value);
    slotUpdateTransitionProperties();
}

void Geometryval::slotGeometryWidth(int value)
{
    if (!keyframeSelected())
        return;
    m_paramRect->setRect(0, 0, value, spinHeight->value());
    slotUpdateTransitionProperties();
}

void Geometryval::slotGeometryHeight(int value)
{
    if (!keyframeSelected())
        return;
    m_paramRect->setRect(0, 0, spinWidth->value(), value);
    slotUpdateTransitionProperties();
}

void Geometryval::slotUpdateGeometry()
{
    QRectF r = m_paramRect->rect().normalized();

    spinX->blockSignals(true);
    spinY->blockSignals(true);
    spinWidth->blockSignals(true);
    spinHeight->blockSignals(true);
    spinResize->blockSignals(true);

    spinX->setValue(m_paramRect->pos().x());
    spinY->setValue(m_paramRect->pos().y());
    spinWidth->setValue(r.width());
    spinHeight->setValue(r.height());
    spinResize->setValue(m_paramRect->rect().width() * 100 / m_realWidth);

    spinX->blockSignals(false);
    spinY->blockSignals(false);
    spinWidth->blockSignals(false);
    spinHeight->blockSignals(false);
    spinResize->blockSignals(false);
}

bool Geometryval::keyframeSelected()
{
    Mlt::GeometryItem item;
    int pos = m_timePos.getValue();
    return !(m_geom->fetch(&item, pos) || item.key() == false);
}


void Geometryval::slotUpdateRange(int inPoint, int outPoint)
{
    m_helper->setKeyGeometry(m_geom, inPoint, outPoint);
    m_helper->update();
    m_timePos.setRange(0, outPoint - inPoint - 1);
}


