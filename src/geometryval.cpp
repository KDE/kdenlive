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


Geometryval::Geometryval(const MltVideoProfile profile, QWidget* parent): QWidget(parent), m_profile(profile), m_geom(NULL), m_path(NULL), paramRect(NULL), m_fixedMode(false) {
    ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(ui.widget);
    QGraphicsView *view = new QGraphicsView(this);
    view->setBackgroundBrush(QBrush(Qt::black));
    vbox->addWidget(view);
    vbox->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vbox2 = new QVBoxLayout(ui.keyframeWidget);
    m_helper = new KeyframeHelper(this);
    vbox2->addWidget(m_helper);
    vbox2->setContentsMargins(0, 0, 0, 0);

    connect(m_helper, SIGNAL(positionChanged(int)), this, SLOT(slotPositionChanged(int)));

    scene = new GraphicsSceneRectMove(this);
    scene->setTool(TITLE_SELECT);
    view->setScene(scene);
    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, profile.width, profile.height));
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(255, 255, 0, 30));
    m_frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    scene->addItem(m_frameBorder);

    ui.buttonNext->setIcon(KIcon("media-skip-forward"));
    ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    ui.buttonPrevious->setIcon(KIcon("media-skip-backward"));
    ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    ui.buttonAdd->setIcon(KIcon("document-new"));
    ui.buttonAdd->setToolTip(i18n("Add keyframe"));
    ui.buttonDelete->setIcon(KIcon("edit-delete"));
    ui.buttonDelete->setToolTip(i18n("Delete keyframe"));

    QMenu *configMenu = new QMenu(i18n("Misc..."), this);
    ui.buttonMenu->setIcon(KIcon("system-run"));
    ui.buttonMenu->setMenu(configMenu);
    ui.buttonMenu->setPopupMode(QToolButton::QToolButton::InstantPopup);


    m_scaleMenu = new QMenu(i18n("Resize..."), this);
    configMenu->addMenu(m_scaleMenu);
    m_scaleMenu->addAction(i18n("50%"), this, SLOT(slotResize50()));
    m_scaleMenu->addAction(i18n("100%"), this, SLOT(slotResize100()));
    m_scaleMenu->addAction(i18n("200%"), this, SLOT(slotResize200()));


    m_alignMenu = new QMenu(i18n("Align..."), this);
    configMenu->addMenu(m_alignMenu);
    m_alignMenu->addAction(i18n("Center"), this, SLOT(slotAlignCenter()));
    m_alignMenu->addAction(i18n("Hor. Center"), this, SLOT(slotAlignHCenter()));
    m_alignMenu->addAction(i18n("Vert. Center"), this, SLOT(slotAlignVCenter()));
    m_alignMenu->addAction(i18n("Right"), this, SLOT(slotAlignRight()));
    m_alignMenu->addAction(i18n("Left"), this, SLOT(slotAlignLeft()));
    m_alignMenu->addAction(i18n("Top"), this, SLOT(slotAlignTop()));
    m_alignMenu->addAction(i18n("Bottom"), this, SLOT(slotAlignBottom()));


    m_syncAction = configMenu->addAction(i18n("Sync timeline cursor"), this, SLOT(slotSyncCursor()));
    m_syncAction->setCheckable(true);
    m_syncAction->setChecked(KdenliveSettings::transitionfollowcursor());

    //scene->setSceneRect(0, 0, profile.width * 2, profile.height * 2);
    //view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
    const double sc = 100.0 / profile.height * 0.8;
    QRectF srect = view->sceneRect();
    view->setSceneRect(srect.x(), -srect.height() / 3 + 10, srect.width(), srect.height() + srect.height() / 3 * 2 - 10);
    scene->setZoom(sc);
    view->centerOn(m_frameBorder);
    connect(ui.buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(ui.buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
    connect(ui.buttonDelete , SIGNAL(clicked()) , this , SLOT(slotDeleteFrame()));
    connect(ui.buttonAdd , SIGNAL(clicked()) , this , SLOT(slotAddFrame()));
    connect(scene, SIGNAL(actionFinished()), this, SLOT(slotUpdateTransitionProperties()));
}

void Geometryval::slotAlignCenter() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos((m_profile.width - paramRect->rect().width()) / 2, (m_profile.height - paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignHCenter() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos((m_profile.width - paramRect->rect().width()) / 2, paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignVCenter() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos(paramRect->pos().x(), (m_profile.height - paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignTop() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos(paramRect->pos().x(), 0);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignBottom() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos(paramRect->pos().x(), m_profile.height - paramRect->rect().height());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignLeft() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos(0, paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignRight() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setPos(m_profile.width - paramRect->rect().width(), paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize50() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setRect(0, 0, m_profile.width / 2, m_profile.height / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize100() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setRect(0, 0, m_profile.width, m_profile.height);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize200() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    paramRect->setRect(0, 0, m_profile.width * 2, m_profile.height * 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotTransparencyChanged(int transp) {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    item.mix(transp);
    paramRect->setBrush(QColor(255, 0, 0, transp));
    m_geom->insert(item);
    emit parameterChanged();
}

void Geometryval::slotSyncCursor() {
    KdenliveSettings::setTransitionfollowcursor(m_syncAction->isChecked());
}

void Geometryval::slotPositionChanged(int pos, bool seek) {
    if (seek && KdenliveSettings::transitionfollowcursor()) emit seekToPos(pos);
    ui.spinPos->setValue(pos);
    m_helper->setValue(pos);
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor, adjust buttons
        ui.buttonAdd->setEnabled(true);
        ui.buttonDelete->setEnabled(false);
        ui.widget->setEnabled(false);
        ui.spinTransp->setEnabled(false);
        m_scaleMenu->setEnabled(false);
        m_alignMenu->setEnabled(false);
    } else {
        ui.buttonAdd->setEnabled(false);
        ui.buttonDelete->setEnabled(true);
        ui.widget->setEnabled(true);
        ui.spinTransp->setEnabled(true);
        m_scaleMenu->setEnabled(true);
        m_alignMenu->setEnabled(true);
    }
    paramRect->setPos(item.x(), item.y());
    paramRect->setRect(0, 0, item.w(), item.h());
    ui.spinTransp->setValue(item.mix());
    paramRect->setBrush(QColor(255, 0, 0, item.mix()));
}

void Geometryval::slotDeleteFrame() {
    // check there is more than one keyframe
    Mlt::GeometryItem item;
    const int pos = ui.spinPos->value();
    int error = m_geom->next_key(&item, pos + 1);
    if (error) {
        error = m_geom->prev_key(&item, pos - 1);
        if (error || item.frame() == pos) return;
    }

    m_geom->remove(ui.spinPos->value());
    ui.buttonAdd->setEnabled(true);
    ui.buttonDelete->setEnabled(false);
    ui.widget->setEnabled(false);
    ui.spinTransp->setEnabled(false);
    m_scaleMenu->setEnabled(false);
    m_alignMenu->setEnabled(false);
    m_helper->update();
    slotPositionChanged(pos, false);
    updateTransitionPath();
    emit parameterChanged();
}

void Geometryval::slotAddFrame() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    item.frame(pos);
    item.x(paramRect->pos().x());
    item.y(paramRect->pos().y());
    item.w(paramRect->rect().width());
    item.h(paramRect->rect().height());
    item.mix(ui.spinTransp->value());
    m_geom->insert(item);
    ui.buttonAdd->setEnabled(false);
    ui.buttonDelete->setEnabled(true);
    ui.widget->setEnabled(true);
    ui.spinTransp->setEnabled(true);
    m_scaleMenu->setEnabled(true);
    m_alignMenu->setEnabled(true);
    m_helper->update();
    emit parameterChanged();
}

void Geometryval::slotNextFrame() {
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, m_helper->value() + 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) {
        // Go to end
        ui.spinPos->setValue(ui.spinPos->maximum());
        return;
    }
    int pos = item.frame();
    ui.spinPos->setValue(pos);
}

void Geometryval::slotPreviousFrame() {
    Mlt::GeometryItem item;
    int error = m_geom->prev_key(&item, m_helper->value() - 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    ui.spinPos->setValue(pos);
}


QDomElement Geometryval::getParamDesc() {
    param.setAttribute("value", m_geom->serialise());
    kDebug() << " / / UPDATING TRANSITION VALUE: " << param.attribute("value");
    return param;
}

void Geometryval::setupParam(const QDomElement& par, int minFrame, int maxFrame) {
    param = par;
    QString val = par.attribute("value");
    if (par.attribute("fixed") == "1") {
        m_fixedMode = true;
        ui.buttonPrevious->setHidden(true);
        ui.buttonNext->setHidden(true);
        ui.buttonDelete->setHidden(true);
        ui.buttonAdd->setHidden(true);
        ui.spinTransp->setMaximum(500);
        ui.label_pos->setHidden(true);
        m_helper->setHidden(true);
        ui.spinPos->setHidden(true);
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
        ui.spinPos->setMaximum(maxFrame - minFrame - 1);
        if (m_path == NULL) {
            m_path = new QGraphicsPathItem();
            m_path->setPen(QPen(Qt::red));
            scene->addItem(m_path);
        }
        updateTransitionPath();
    }
    Mlt::GeometryItem item;

    m_geom->fetch(&item, 0);
    if (paramRect) delete paramRect;
    paramRect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    paramRect->setPos(item.x(), item.y());
    paramRect->setZValue(0);

    paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));
    scene->addItem(paramRect);
    slotPositionChanged(0, false);
    if (!m_fixedMode) {
        connect(ui.spinPos, SIGNAL(valueChanged(int)), this , SLOT(slotPositionChanged(int)));
    }
    connect(ui.spinTransp, SIGNAL(valueChanged(int)), this , SLOT(slotTransparencyChanged(int)));
}

void Geometryval::updateTransitionPath() {
    if (m_fixedMode) return;
    Mlt::GeometryItem item;
    int pos = 0;
    int counter = 0;
    QPainterPath path;
    while (true) {
        if (m_geom->next_key(&item, pos) == 1) break;
        pos = item.frame();
        if (counter == 0) {
            path.moveTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        } else {
            path.lineTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        }
        counter++;
        pos++;
    }
    m_path->setPath(path);
}

void Geometryval::slotUpdateTransitionProperties() {
    int pos = ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, pos);
    if (error || item.frame() != pos) {
        // no keyframe under cursor
        return;
    }
    item.x(paramRect->pos().x());
    item.y(paramRect->pos().y());
    item.w(paramRect->rect().width());
    item.h(paramRect->rect().height());
    m_geom->insert(item);
    updateTransitionPath();
    emit parameterChanged();
}
