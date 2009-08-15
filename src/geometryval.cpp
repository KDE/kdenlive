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
    m_ui.setupUi(this);
    QVBoxLayout* vbox = new QVBoxLayout(m_ui.widget);
    QGraphicsView *view = new QGraphicsView(this);
    view->setBackgroundBrush(QBrush(Qt::black));
    vbox->addWidget(view);
    vbox->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vbox2 = new QVBoxLayout(m_ui.keyframeWidget);
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
    QGraphicsRectItem *m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, profile.width, profile.height));
    m_frameBorder->setZValue(-1100);
    m_frameBorder->setBrush(QColor(255, 255, 0, 30));
    m_frameBorder->setPen(QPen(QBrush(QColor(255, 255, 255, 255)), 1.0, Qt::DashLine));
    m_scene->addItem(m_frameBorder);

    m_ui.buttonNext->setIcon(KIcon("media-skip-forward"));
    m_ui.buttonNext->setToolTip(i18n("Go to next keyframe"));
    m_ui.buttonPrevious->setIcon(KIcon("media-skip-backward"));
    m_ui.buttonPrevious->setToolTip(i18n("Go to previous keyframe"));
    m_ui.buttonAdd->setIcon(KIcon("document-new"));
    m_ui.buttonAdd->setToolTip(i18n("Add keyframe"));
    m_ui.buttonDelete->setIcon(KIcon("edit-delete"));
    m_ui.buttonDelete->setToolTip(i18n("Delete keyframe"));

    m_configMenu = new QMenu(i18n("Misc..."), this);
    m_ui.buttonMenu->setIcon(KIcon("system-run"));
    m_ui.buttonMenu->setMenu(m_configMenu);
    m_ui.buttonMenu->setPopupMode(QToolButton::QToolButton::InstantPopup);


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
    view->centerOn(m_frameBorder);
    connect(m_ui.buttonNext , SIGNAL(clicked()) , this , SLOT(slotNextFrame()));
    connect(m_ui.buttonPrevious , SIGNAL(clicked()) , this , SLOT(slotPreviousFrame()));
    connect(m_ui.buttonDelete , SIGNAL(clicked()) , this , SLOT(slotDeleteFrame()));
    connect(m_ui.buttonAdd , SIGNAL(clicked()) , this , SLOT(slotAddFrame()));
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
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos((m_profile.width - m_paramRect->rect().width()) / 2, (m_profile.height - m_paramRect->rect().height()) / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignHCenter()
{
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos((m_profile.width - m_paramRect->rect().width()) / 2, m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotAlignVCenter()
{
    int pos = m_ui.spinPos->value();
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
    int pos = m_ui.spinPos->value();
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
    int pos = m_ui.spinPos->value();
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
    int pos = m_ui.spinPos->value();
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
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setPos(m_profile.width - m_paramRect->rect().width(), m_paramRect->pos().y());
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize50()
{
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_profile.width / 2, m_profile.height / 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize100()
{
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_profile.width, m_profile.height);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResize200()
{
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    m_paramRect->setRect(0, 0, m_profile.width * 2, m_profile.height * 2);
    slotUpdateTransitionProperties();
}

void Geometryval::slotResizeOriginal()
{
    if (m_frameSize.isNull()) slotResize100();
    int pos = m_ui.spinPos->value();
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
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor
        return;
    }
    int scale = m_paramRect->rect().width() * 100 / m_profile.width;
    bool ok;
    scale =  QInputDialog::getInteger(this, i18n("Resize..."), i18n("Scale"), scale, 1, 2147483647, 10, &ok);
    if (!ok) return;
    m_paramRect->setRect(0, 0, m_profile.width * scale / 100, m_profile.height * scale / 100);
    slotUpdateTransitionProperties();
}

void Geometryval::slotTransparencyChanged(int transp)
{
    int pos = m_ui.spinPos->value();
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
    m_ui.spinPos->setValue(pos);
    m_helper->setValue(pos);
    Mlt::GeometryItem item;
    int error = m_geom->fetch(&item, pos);
    if (error || item.key() == false) {
        // no keyframe under cursor, adjust buttons
        m_ui.buttonAdd->setEnabled(true);
        m_ui.buttonDelete->setEnabled(false);
        m_ui.widget->setEnabled(false);
        m_ui.spinTransp->setEnabled(false);
        m_scaleMenu->setEnabled(false);
        m_alignMenu->setEnabled(false);
        m_editGeom->setEnabled(false);
    } else {
        m_ui.buttonAdd->setEnabled(false);
        m_ui.buttonDelete->setEnabled(true);
        m_ui.widget->setEnabled(true);
        m_ui.spinTransp->setEnabled(true);
        m_scaleMenu->setEnabled(true);
        m_alignMenu->setEnabled(true);
        m_editGeom->setEnabled(true);
    }
    m_paramRect->setPos(item.x(), item.y());
    m_paramRect->setRect(0, 0, item.w(), item.h());
    m_ui.spinTransp->setValue(item.mix());
    m_paramRect->setBrush(QColor(255, 0, 0, item.mix()));
}

void Geometryval::slotDeleteFrame(int pos)
{
    // check there is more than one keyframe
    Mlt::GeometryItem item;
    if (pos == -1) pos = m_ui.spinPos->value();
    int error = m_geom->next_key(&item, pos + 1);
    if (error) {
        error = m_geom->prev_key(&item, pos - 1);
        if (error || item.frame() == pos) return;
    }

    m_geom->remove(m_ui.spinPos->value());
    m_ui.buttonAdd->setEnabled(true);
    m_ui.buttonDelete->setEnabled(false);
    m_ui.widget->setEnabled(false);
    m_ui.spinTransp->setEnabled(false);
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
    if (pos == -1) pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    item.frame(pos);
    item.x(m_paramRect->pos().x());
    item.y(m_paramRect->pos().y());
    item.w(m_paramRect->rect().width());
    item.h(m_paramRect->rect().height());
    item.mix(m_ui.spinTransp->value());
    m_geom->insert(item);
    m_ui.buttonAdd->setEnabled(false);
    m_ui.buttonDelete->setEnabled(true);
    m_ui.widget->setEnabled(true);
    m_ui.spinTransp->setEnabled(true);
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
        m_ui.spinPos->setValue(m_ui.spinPos->maximum());
        return;
    }
    int pos = item.frame();
    m_ui.spinPos->setValue(pos);
}

void Geometryval::slotPreviousFrame()
{
    Mlt::GeometryItem item;
    int error = m_geom->prev_key(&item, m_helper->value() - 1);
    kDebug() << "// SEEK TO NEXT KFR: " << error;
    if (error) return;
    int pos = item.frame();
    m_ui.spinPos->setValue(pos);
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
        m_ui.buttonPrevious->setHidden(true);
        m_ui.buttonNext->setHidden(true);
        m_ui.buttonDelete->setHidden(true);
        m_ui.buttonAdd->setHidden(true);
        m_ui.spinTransp->setMaximum(500);
        m_ui.label_pos->setHidden(true);
        m_helper->setHidden(true);
        m_ui.spinPos->setHidden(true);

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
        m_ui.spinPos->setMaximum(maxFrame - minFrame - 1);
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
    m_paramRect = new QGraphicsRectItem(QRectF(0, 0, item.w(), item.h()));
    m_paramRect->setPos(item.x(), item.y());
    m_paramRect->setZValue(0);

    m_paramRect->setPen(QPen(QBrush(QColor(255, 0, 0, 255)), 1.0));
    m_scene->addItem(m_paramRect);
    slotPositionChanged(0, false);
    if (!m_fixedMode) {
        connect(m_ui.spinPos, SIGNAL(valueChanged(int)), this , SLOT(slotPositionChanged(int)));
    }
    connect(m_ui.spinTransp, SIGNAL(valueChanged(int)), this , SLOT(slotTransparencyChanged(int)));
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
            path.moveTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        } else {
            path.lineTo(item.x() + item.w() / 2, item.y() + item.h() / 2);
        }
        counter++;
        pos++;
    }
    m_path->setPath(path);
}

void Geometryval::slotUpdateTransitionProperties()
{
    int pos = m_ui.spinPos->value();
    Mlt::GeometryItem item;
    int error = m_geom->next_key(&item, pos);
    if (error || item.frame() != pos) {
        // no keyframe under cursor
        return;
    }
    QRectF r = m_paramRect->rect().normalized();
    item.x(m_paramRect->pos().x());
    item.y(m_paramRect->pos().y());
    item.w(r.width());
    item.h(r.height());
    m_geom->insert(item);
    updateTransitionPath();
    emit parameterChanged();
}

void Geometryval::slotGeometry()
{
    int pos = m_ui.spinPos->value();
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
        m_view.value_width->setValue(m_profile.width);
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

