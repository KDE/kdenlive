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

#include <KDebug>

#include <QGraphicsView>
#include <QVBoxLayout>
#include <QGraphicsRectItem>
#include <QMenu>
#include <QInputDialog>


Geometryval::Geometryval(const MltVideoProfile profile, QPoint frame_size, QWidget* parent) :
        QWidget(parent),
        m_profile(profile),
        m_paramRect(NULL),
        m_geom(NULL),
        m_path(NULL),
        m_fixedMode(false),
        m_frameSize(frame_size)
{
    setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(widget);
    QGraphicsView *view = new QGraphicsView(this);
    view->setBackgroundBrush(QBrush(Qt::black));
    vbox->addWidget(view);
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
    view->setScene(m_scene);
    m_dar = (m_profile.height * m_profile.display_aspect_num / (double) m_profile.display_aspect_den) / (double) m_profile.width;

    m_realWidth = (int)(profile.height * profile.display_aspect_num / (double) profile.display_aspect_den);
    QGraphicsRectItem *frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_realWidth, profile.height));
    frameBorder->setZValue(-1100);
    frameBorder->setBrush(QColor(255, 255, 0, 30));
    frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    m_scene->addItem(frameBorder);

    buttonNext->setIcon(KIcon("media-skip-forward"));
    buttonNext->setToolTip(i18n("Go to next keyframe"));
    buttonPrevious->setIcon(KIcon("media-skip-backward"));
    buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    buttonAdd->setIcon(KIcon("document-new"));
    buttonAdd->setToolTip(i18n("Add keyframe"));
    buttonDelete->setIcon(KIcon("edit-delete"));
    buttonDelete->setToolTip(i18n("Delete keyframe"));

    m_configMenu = new QMenu(i18n("Misc..."), this);
    buttonMenu->setIcon(KIcon("system-run"));
    buttonMenu->setMenu(m_configMenu);
    buttonMenu->setPopupMode(QToolButton::QToolButton::InstantPopup);


    m_editGeom = m_configMenu->addAction(i18n("Edit keyframe"), this, SLOT(slotGeometry()));

    m_scaleMenu = new QMenu(i18n("Resize..."), this);
    m_configMenu->addMenu(m_scaleMenu);
    m_scaleMenu->addAction(i18n("50%"), this, SLOT(slotResize50()));
    m_scaleMenu->addAction(i18n("100%"), this, SLOT(slotResize100()));
    m_scaleMenu->addAction(i18n("200%"), this, SLOT(slotResize200()));
    m_scaleMenu->addAction(i18n("Original size"), this, SLOT(slotResizeOriginal()));
    m_scaleMenu->addAction(i18n("Custom"), this, SLOT(slotResizeCustom()));

    m_alignMenu = new QMenu(i18n("Align..."), this);
    m_configMenu->addMenu(m_alignMenu);
    m_alignMenu->addAction(i18n("Center"), this, SLOT(slotAlignCenter()));
    m_alignMenu->addAction(i18n("Hor. Center"), this, SLOT(slotAlignHCenter()));
    m_alignMenu->addAction(i18n("Vert. Center"), this, SLOT(slotAlignVCenter()));
    m_alignMenu->addAction(i18n("Right"), this, SLOT(slotAlignRight()));
    m_alignMenu->addAction(i18n("Left"), this, SLOT(slotAlignLeft()));
    m_alignMenu->addAction(i18n("Top"), this, SLOT(slotAlignTop()));
    m_alignMenu->addAction(i18n("Bottom"), this, SLOT(slotAlignBottom()));


    m_syncAction = m_configMenu->addAction(i18n("Sync timeline cursor"), this, SLOT(slotSyncCursor()));
    m_syncAction->setCheckable(true);
    m_syncAction->setChecked(KdenliveSettings::transitionfollowcursor());

    //scene->setSceneRect(0, 0, profile.width * 2, profile.height * 2);
    //view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    const double sc = 100.0 / profile.height * 0.8;
    QRectF srect = view->sceneRect();
    view->setSceneRect(srect.x(), -srect.height() / 3 + 10, srect.width(), srect.height() + srect.height() / 3 * 2 - 10);
    m_scene->setZoom(sc);
    view->centerOn(frameBorder);
    connect(buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
    connect(buttonDelete , SIGNAL(clicked()) , this , SLOT(slotDeleteFrame()));
    connect(buttonAdd , SIGNAL(clicked()) , this , SLOT(slotAddFrame()));
    connect(m_scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateTransitionProperties()));
    connect(m_scene, SIGNAL(doubleClickEvent()), this, SLOT(slotGeometry()));

}


Geometryval::~Geometryval()
{
    m_scene->disconnect();
    delete m_scaleMenu;
    delete m_alignMenu;
    delete m_editGeom;
    delete m_syncAction;
    delete m_configMenu;
    delete m_paramRect;
    delete m_path;
    delete m_helper;
    delete m_geom;
    delete m_scene;
}


void Geometryval::slotAlignCenter()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos((m_realWidth - m_paramRect->rect().width()) / 2, (m_profile.height - m_paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignHCenter()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos((m_realWidth - m_paramRect->rect().width()) / 2, m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignVCenter()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(m_paramRect->pos().x(), (m_profile.height - m_paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignTop()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(m_paramRect->pos().x(), 0);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignBottom()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(m_paramRect->pos().x(), m_profile.height - m_paramRect->rect().height());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignLeft()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(0, m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignRight()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(m_realWidth - m_paramRect->rect().width(), m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize50()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_realWidth / 2, m_profile.height / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize100()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_realWidth, m_profile.height);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize200()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_realWidth * 2, m_profile.height * 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResizeOriginal()
{
    if (m_frameSize.isNull()) slotResize100();
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_frameSize.x(), m_frameSize.y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResizeCustom()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    int scale = m_paramRect->rect().width() * 100 / m_realWidth;
    bool ok;
    scale =  QInputDialog::getInteger(this, i18n("Resize..."), i18n("Scale"), scale, 1, 2147483647, 10, &ok);
    if (!ok) return;
    m_paramRect->setRect(0, 0, m_realWidth * scale / 100, m_profile.height * scale / 100);
    slotUpdateTransitionProperties();
}

void Geometryval::slotTransparencyChanged(int transp)
{
    int pos = spinPos->value();
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

void Geometryval::slotPositionChanged(int pos, bool seek)
{
    if (seek && KdenliveSettings::transitionfollowcursor()) emit seekToPos(pos);
    spinPos->setValue(pos);
    m_helper->setValue(pos);
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor, adjust buttons
        buttonAdd->setEnabled(true);
        buttonDelete->setEnabled(false);
        widget->setEnabled(false);
        spinTransp->setEnabled(false);
        m_scaleMenu->setEnabled(false);
        m_alignMenu->setEnabled(false);
        m_editGeom->setEnabled(false);
    } else {
        buttonAdd->setEnabled(false);
        buttonDelete->setEnabled(true);
        widget->setEnabled(true);
        spinTransp->setEnabled(true);
        m_scaleMenu->setEnabled(true);
        m_alignMenu->setEnabled(true);
        m_editGeom->setEnabled(true);
    }

    m_paramRect->setPos(item.x() * m_dar, item.y());
    m_paramRect->setRect(0, 0, item.w() * m_dar, item.h());
    spinTransp->setValue(item.mix());
    m_paramRect->setBrush(QColor(255, 0, 0, item.mix()));
}

void Geometryval::slotDeleteFrame(int pos)
{
    // check there is more than one keyframe
    Mlt::GeometryItem item;
    if (pos == -1) pos = spinPos->value();
    int error = m_geom->next_key(&item, pos + 1);
    if (error) {
        error = m_geom->prev_key(&item, pos - 1);
        if (error || item.frame() == pos) return;
    }

    m_geom->remove(spinPos->value());
    buttonAdd->setEnabled(true);
    buttonDelete->setEnabled(false);
    widget->setEnabled(false);
    spinTransp->setEnabled(false);
    m_scaleMenu->setEnabled(false);
    m_alignMenu->setEnabled(false);
    m_editGeom->setEnabled(false);
    m_helper->update();
    slotPositionChanged(pos, false);
    updateTransitionPath();
    emit parameterChanged();
}

void Geometryval::slotAddFrame(int pos)
{
    if (pos == -1) pos = spinPos->value();
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
    m_scaleMenu->setEnabled(true);
    m_alignMenu->setEnabled(true);
    m_editGeom->setEnabled(true);
    m_helper->update();
    emit parameterChanged();
}

void Geometryval::slotNextFrame()
{
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, m_helper->value() + 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) {
        // Go to end
        spinPos->setValue(spinPos->maximum());
        return;
    }
    int pos = item.frame();
    spinPos->setValue(pos);
}

void Geometryval::slotPreviousFrame()
{
    Mlt::GeometryItem item;
    int error = m_geom->prev_key(&item, m_helper->value() - 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    spinPos->setValue(pos);
}


QString Geometryval::getValue() const
{
    return m_geom->serialise();
}

void Geometryval::setupParam(const QDomElement par, int minFrame, int maxFrame)
{
    QString val = par.attribute("value");
    if (par.attribute("fixed") == "1") {
        m_fixedMode = true;
        buttonPrevious->setHidden(true);
        buttonNext->setHidden(true);
        buttonDelete->setHidden(true);
        buttonAdd->setHidden(true);
        spinTransp->setMaximum(500);
        label_pos->setHidden(true);
        m_helper->setHidden(true);
        spinPos->setHidden(true);

    }
    char *tmp = (char *) qstrdup(val.toUtf8().data());
    if (m_geom) m_geom->parse(tmp, maxFrame - minFrame, m_profile.width, m_profile.height);
    else m_geom = new Mlt::Geometry(tmp, maxFrame - minFrame, m_profile.width, m_profile.height);
    delete[] tmp;

    //kDebug() << " / / UPDATING TRANSITION VALUE: " << m_geom->serialise();
    //read param her and set rect
    if (!m_fixedMode) {
        m_helper->setKeyGeometry(m_geom, maxFrame - minFrame - 1);
        m_helper->update();
        /*QDomDocument doc;
        doc.appendChild(doc.importNode(par, true));
        kDebug() << "IMPORTED TRANS: " << doc.toString();*/
        spinPos->setMaximum(maxFrame - minFrame - 1);
        if (m_path == NULL) {
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
    if (!m_fixedMode) {
        connect(spinPos, SIGNAL(valueChanged(int)), this , SLOT(slotPositionChanged(int)));
    }
    connect(spinTransp, SIGNAL(valueChanged(int)), this , SLOT(slotTransparencyChanged(int)));
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
    int pos = spinPos->value();
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

void Geometryval::slotGeometry()
{
    int pos = spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    QRectF r = m_paramRect->rect().normalized();

    QDialog d(this);
    m_view.setupUi(&d);
    d.setWindowTitle(i18n("Frame Geometry"));
    m_view.value_x->setMaximum(10000);
    m_view.value_x->setMinimum(-10000);
    m_view.value_y->setMaximum(10000);
    m_view.value_y->setMinimum(-10000);
    m_view.value_width->setMaximum(500000);
    m_view.value_width->setMinimum(1);
    m_view.value_height->setMaximum(500000);
    m_view.value_height->setMinimum(1);

    m_view.value_x->setValue(m_paramRect->pos().x());
    m_view.value_y->setValue(m_paramRect->pos().y());
    m_view.value_width->setValue(r.width());
    m_view.value_height->setValue(r.height());
    connect(m_view.button_reset , SIGNAL(clicked()) , this , SLOT(slotResetPosition()));

    if (d.exec() == QDialog::Accepted) {
        m_paramRect->setPos(m_view.value_x->value(), m_view.value_y->value());
        m_paramRect->setRect(0, 0, m_view.value_width->value(), m_view.value_height->value());
        slotUpdateTransitionProperties();
    }
}

void Geometryval::slotResetPosition()
{
    m_view.value_x->setValue(0);
    m_view.value_y->setValue(0);

    if (m_frameSize.isNull()) {
        m_view.value_width->setValue(m_realWidth);
        m_view.value_height->setValue(m_profile.height);
    } else {
        m_view.value_width->setValue(m_frameSize.x());
        m_view.value_height->setValue(m_frameSize.y());
    }
}

void Geometryval::setFrameSize(QPoint p)
{
    m_frameSize = p;
}


void Geometryval::slotKeyframeMoved(int pos)
{
    slotPositionChanged(pos);
    slotUpdateTransitionProperties();
}

