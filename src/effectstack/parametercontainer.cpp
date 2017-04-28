/***************************************************************************
 *   Copyright (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "parametercontainer.h"

#include "dragvalue.h"

#include "widgets/animationwidget.h"
#include "widgets/curves/bezier/beziersplineeditor.h"
#include "widgets/curves/curveparamwidget.h"
#include "widgets/boolparamwidget.h"
#include "widgets/choosecolorwidget.h"
#include "widgets/cornerswidget.h"
#include "widgets/doubleparameterwidget.h"
#include "widgets/draggablelabel.h"
#include "widgets/geometrywidget.h"
#include "widgets/keyframeedit.h"
#include "widgets/curves/cubic/kis_cubic_curve.h"
#include "widgets/curves/cubic/kis_curve_widget.h"
#include "widgets/listparamwidget.h"
#include "widgets/lumaliftgain.h"
#include "widgets/positionwidget.h"
#include "widgets/selectivecolor.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "colortools.h"
#include "dialogs/clipcreationdialog.h"
#include "mltcontroller/effectscontroller.h"
#include "utils/KoIconUtils.h"
#include "onmonitoritems/rotoscoping/rotowidget.h"

#include "ui_wipeval_ui.h"
#include "ui_urlval_ui.h"
#include "ui_keywordval_ui.h"
#include "ui_fontval_ui.h"

#include <KUrlRequester>
#include "klocalizedstring.h"

#include <QMap>
#include <QString>
#include <QImage>
#include "kdenlive_debug.h"
#include <QClipboard>
#include <QDrag>
#include <QMimeData>


MySpinBox::MySpinBox(QWidget *parent):
    QSpinBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void MySpinBox::focusInEvent(QFocusEvent *e)
{
    setFocusPolicy(Qt::WheelFocus);
    e->accept();
}

void MySpinBox::focusOutEvent(QFocusEvent *e)
{
    setFocusPolicy(Qt::StrongFocus);
    e->accept();
}


class Wipeval: public QWidget, public Ui::Wipeval_UI
{
};

class Urlval: public QWidget, public Ui::Urlval_UI
{
};

class Keywordval: public QWidget, public Ui::Keywordval_UI
{
};

class Fontval: public QWidget, public Ui::Fontval_UI
{
};

ParameterContainer::ParameterContainer(const QDomElement &effect, const ItemInfo &info, EffectMetaInfo *metaInfo, QWidget *parent) :
    m_info(info),
    m_keyframeEditor(nullptr),
    m_geometryWidget(nullptr),
    m_animationWidget(nullptr),
    m_metaInfo(metaInfo),
    m_effect(effect),
    m_acceptDrops(false),
    m_monitorEffectScene(MonitorSceneDefault),
    m_conditionParameter(false)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    setObjectName(QStringLiteral("ParameterContainer"));

    m_in = info.cropStart.frames(KdenliveSettings::project_fps());
    m_out = (info.cropStart + info.cropDuration).frames(KdenliveSettings::project_fps()) - 1;

    QDomNodeList namenode = effect.childNodes();
    QDomElement e = effect.toElement();

    int minFrame = e.attribute(QStringLiteral("start")).toInt();
    int maxFrame = e.attribute(QStringLiteral("end")).toInt();
    // In transitions, maxFrame is in fact one frame after the end of transition
    if (maxFrame > 0) {
        maxFrame --;
    }

    bool disable = effect.attribute(QStringLiteral("disable")) == QLatin1String("1") && KdenliveSettings::disable_effect_parameters();
    parent->setEnabled(!disable);

    bool stretch = true;
    m_vbox = new QVBoxLayout(parent);
    m_vbox->setContentsMargins(4, 0, 4, 0);
    m_vbox->setSpacing(2);
    /*
    QDomElement clone = effect.cloneNode(true).toElement();
    QDomDocument doc;
    doc.appendChild(doc.importNode(clone, true));
    qCDebug(KDENLIVE_LOG)<<"-------------------------------------\n"<<"LOADED TRANS: "<<doc.toString()<<"\n-------------------";*/

    // Conditional effect (display / enable some parameters only if a parameter is present
    if (effect.hasAttribute(QStringLiteral("condition"))) {
        QString condition = effect.attribute(QStringLiteral("condition"));
        QString conditionParam = EffectsList::parameter(effect, condition);
        m_conditionParameter = !conditionParam.isEmpty();
    }

    if (effect.attribute(QStringLiteral("id")) == QLatin1String("movit.lift_gamma_gain") || effect.attribute(QStringLiteral("id")) == QLatin1String("lift_gamma_gain")) {
        // We use a special custom widget here
        LumaLiftGain *gainWidget = new LumaLiftGain(namenode, parent);
        m_vbox->addWidget(gainWidget);
        m_valueItems[effect.attribute(QStringLiteral("id"))] = gainWidget;

        connect(gainWidget, &LumaLiftGain::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
    } else if (effect.attribute(QStringLiteral("id")) == QLatin1String("avfilter.selectivecolor")) {
        SelectiveColor *cmykAdjust = new SelectiveColor(effect);
        connect(cmykAdjust, &SelectiveColor::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
        m_vbox->addWidget(cmykAdjust);
        m_valueItems[effect.attribute(QStringLiteral("id"))] = cmykAdjust;
    } else for (int i = 0; i < namenode.count(); ++i) {
            QDomElement pa = namenode.item(i).toElement();
            if (pa.tagName() == QLatin1String("conditionalinfo")) {
                // Conditional info
                KMessageWidget *mw = new KMessageWidget;
                mw->setWordWrap(true);
                mw->setMessageType(KMessageWidget::Information);
                mw->setText(i18n(pa.text().toUtf8().data()));
                mw->setCloseButtonVisible(false);
                mw->setVisible(!m_conditionParameter);
                m_vbox->addWidget(mw);
                m_conditionalWidgets << mw;
            }
            if (pa.tagName() != QLatin1String("parameter")) {
                continue;
            }
            QString type = pa.attribute(QStringLiteral("type"));
            if (type == QLatin1String("fixed")) {
                // Fixed parameters are not exposed in the UI
                continue;
            }
            QDomElement na = pa.firstChildElement(QStringLiteral("name"));
            QDomElement commentElem = pa.firstChildElement(QStringLiteral("comment"));
            QString paramName = na.isNull() ? pa.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
            QString comment;
            if (!commentElem.isNull()) {
                comment = i18n(commentElem.text().toUtf8().data());
            }
            QWidget *toFillin = new QWidget(parent);
            QString value = pa.attribute(QStringLiteral("value")).isNull() ?
                            pa.attribute(QStringLiteral("default")) : pa.attribute(QStringLiteral("value"));

            /** See effects/README for info on the different types */
            if (type == QLatin1String("double") || type == QLatin1String("constant")) {
                double min;
                double max;
                m_acceptDrops = true;
                if (pa.attribute(QStringLiteral("min")).contains(QLatin1Char('%'))) {
                    min = EffectsController::getStringEval(m_metaInfo->monitor->profileInfo(), pa.attribute(QStringLiteral("min")), m_metaInfo->frameSize);
                } else {
                    min = locale.toDouble(pa.attribute(QStringLiteral("min")));
                }
                if (pa.attribute(QStringLiteral("max")).contains(QLatin1Char('%'))) {
                    max = EffectsController::getStringEval(m_metaInfo->monitor->profileInfo(), pa.attribute(QStringLiteral("max")), m_metaInfo->frameSize);
                } else {
                    max = locale.toDouble(pa.attribute(QStringLiteral("max")));
                }

                DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, locale.toDouble(value), min, max,
                        locale.toDouble(pa.attribute(QStringLiteral("default"))), comment, -1, pa.attribute(QStringLiteral("suffix")), pa.attribute(QStringLiteral("decimals")).toInt(), false, parent);
                doubleparam->setFocusPolicy(Qt::StrongFocus);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    doubleparam->setEnabled(false);
                    m_conditionalWidgets << doubleparam;
                }
                m_vbox->addWidget(doubleparam);
                m_valueItems[paramName] = doubleparam;
                connect(doubleparam, &DoubleParameterWidget::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
            } else if (type == QLatin1String("list")) {
                ListParamWidget *lswid = new ListParamWidget(paramName, comment, parent);
                lswid->setFocusPolicy(Qt::StrongFocus);
                QString items = pa.attribute(QStringLiteral("paramlist"));
                QStringList listitems;
                if (items == QLatin1String("%lumaPaths")) {
                    // Special case: Luma files
                    // Create thumbnails
                    lswid->setIconSize(QSize(50, 30));
                    if (m_metaInfo->monitor->profileInfo().profileSize.width() > 1000) {
                        // HD project
                        listitems = MainWindow::m_lumaFiles.value(QStringLiteral("HD"));
                    } else {
                        listitems = MainWindow::m_lumaFiles.value(QStringLiteral("PAL"));
                    }
                    lswid->addItem(i18n("None (Dissolve)"));
                    for (int j = 0; j < listitems.count(); ++j) {
                        QString entry = listitems.at(j);
                        lswid->addItem(listitems.at(j).section(QLatin1Char('/'), -1), entry);
                        if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
                            if (!MainWindow::m_lumacache.contains(entry)) {
                                QImage pix(entry);
                                MainWindow::m_lumacache.insert(entry, pix.scaled(50, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                            }
                            lswid->setItemIcon(j + 1, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                        }
                    }
                    lswid->setCurrentText(pa.attribute(QStringLiteral("default")));
                    if (!value.isEmpty() && listitems.contains(value)) {
                        lswid->setCurrentIndex(listitems.indexOf(value) + 1);
                    }
                } else {
                    listitems = items.split(QLatin1Char(';'));
                    if (listitems.count() == 1) {
                        // probably custom effect created before change to ';' as separator
                        listitems = pa.attribute(QStringLiteral("paramlist")).split(QLatin1Char(','));
                    }
                    QDomElement list = pa.firstChildElement(QStringLiteral("paramlistdisplay"));
                    QStringList listitemsdisplay;
                    if (!list.isNull()) {
                        listitemsdisplay = i18n(list.text().toUtf8().data()).split(QLatin1Char(','));
                    } else {
                        listitemsdisplay = i18n(pa.attribute("paramlistdisplay").toUtf8().data()).split(QLatin1Char(','));
                    }
                    if (listitemsdisplay.count() != listitems.count()) {
                        listitemsdisplay = listitems;
                    }
                    for (int j = 0; j < listitems.count(); ++j) {
                        lswid->addItem(listitemsdisplay.at(j), listitems.at(j));
                    }
                    if (!value.isEmpty() && listitems.contains(value)) {
                        lswid->setCurrentIndex(listitems.indexOf(value));
                    }
                }
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    lswid->setEnabled(false);
                    m_conditionalWidgets << lswid;
                }
                m_valueItems[paramName] = lswid;
                connect(lswid, SIGNAL(valueChanged()) , this, SLOT(slotCollectAllParameters()));
                if (!comment.isEmpty()) {
                    connect(this, SIGNAL(showComments(bool)), lswid, SLOT(slotShowComment(bool)));
                }
                m_vbox->addWidget(lswid);
            } else if (type == QLatin1String("bool")) {
                bool checked = (value == QLatin1String("1"));
                BoolParamWidget *bwid = new BoolParamWidget(paramName, comment, checked, parent);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    bwid->setEnabled(false);
                    m_conditionalWidgets << bwid;
                }
                m_valueItems[paramName] = bwid;
                connect(bwid, SIGNAL(valueChanged()), this, SLOT(slotCollectAllParameters()));
                connect(this, SIGNAL(showComments(bool)), bwid, SLOT(slotShowComment(bool)));
                m_vbox->addWidget(bwid);
            } else if (type == QLatin1String("switch")) {
                bool checked = (value == pa.attribute("min"));
                BoolParamWidget *bwid = new BoolParamWidget(paramName, comment, checked, parent);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    bwid->setEnabled(false);
                    m_conditionalWidgets << bwid;
                }
                m_valueItems[paramName] = bwid;
                connect(bwid, SIGNAL(valueChanged()), this, SLOT(slotCollectAllParameters()));
                connect(this, SIGNAL(showComments(bool)), bwid, SLOT(slotShowComment(bool)));
                m_vbox->addWidget(bwid);
            } else if (type.startsWith(QLatin1String("animated"))) {
                m_acceptDrops = true;
                if (type == QLatin1String("animatedrect")) {
                    m_monitorEffectScene = MonitorSceneGeometry;
                }
                if (m_animationWidget) {
                    m_animationWidget->addParameter(pa);
                } else {
                    m_animationWidget = new AnimationWidget(m_metaInfo, info.startPos.frames(KdenliveSettings::project_fps()), m_in, m_out, effect.attribute(QStringLiteral("in")).toInt(), effect.attribute(QStringLiteral("id")), pa, parent);
                    connect(m_animationWidget, &AnimationWidget::seekToPos, this, &ParameterContainer::seekTimeline);
                    connect(m_animationWidget, &AnimationWidget::setKeyframes, this, &ParameterContainer::importKeyframes);
                    connect(this, &ParameterContainer::syncEffectsPos, m_animationWidget, &AnimationWidget::slotSyncPosition);
                    connect(this, SIGNAL(initScene(int)), m_animationWidget, SLOT(slotPositionChanged(int)));
                    connect(m_animationWidget, &AnimationWidget::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                    connect(this, SIGNAL(showComments(bool)), m_animationWidget, SLOT(slotShowComment(bool)));
                    m_vbox->addWidget(m_animationWidget);
                    if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                        m_animationWidget->setEnabled(false);
                        m_conditionalWidgets << m_animationWidget;
                    }
                }
            } else if (type == QLatin1String("geometry")) {
                m_acceptDrops = true;
                m_monitorEffectScene = MonitorSceneGeometry;
                bool useOffset = false;
                if (	effect.tagName() == QLatin1String("effect") && effect.hasAttribute(QStringLiteral("kdenlive:sync_in_out")) && effect.attribute(QStringLiteral("kdenlive:sync_in_out")).toInt() == 0) {
                    useOffset = true;
                }
                m_geometryWidget = new GeometryWidget(m_metaInfo, info.startPos.frames(KdenliveSettings::project_fps()), effect.hasAttribute(QStringLiteral("showrotation")), useOffset, parent);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    m_geometryWidget->setEnabled(false);
                    m_conditionalWidgets << m_geometryWidget;
                }
                connect(m_geometryWidget, SIGNAL(valueChanged()), this, SLOT(slotCollectAllParameters()));
                if (minFrame == maxFrame) {
                    m_geometryWidget->setupParam(pa, m_in, m_out);
                    connect(this, SIGNAL(updateRange(int,int)), m_geometryWidget, SLOT(slotUpdateRange(int,int)));
                } else {
                    m_geometryWidget->setupParam(pa, minFrame, maxFrame);
                }
                m_vbox->addWidget(m_geometryWidget);
                m_valueItems[paramName+QStringLiteral("geometry")] = m_geometryWidget;
                connect(m_geometryWidget, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(m_geometryWidget, SIGNAL(importClipKeyframes()), this, SIGNAL(importClipKeyframes()));
                connect(this, SIGNAL(syncEffectsPos(int)), m_geometryWidget, SLOT(slotSyncPosition(int)));
                connect(this, SIGNAL(initScene(int)), m_geometryWidget, SLOT(slotInitScene(int)));
                connect(this, &ParameterContainer::updateFrameInfo, m_geometryWidget, &GeometryWidget::setFrameSize);
            } else if (type == QLatin1String("addedgeometry")) {
                // this is a parameter that should be linked to the geometry widget, for example rotation, shear, ...
                if (m_geometryWidget) {
                    m_geometryWidget->addParameter(pa);
                }
            } else if (type == QLatin1String("keyframe") || type == QLatin1String("simplekeyframe")) {
                // keyframe editor widget
                if (m_keyframeEditor == nullptr) {
                    KeyframeEdit *geo;
                    if (pa.attribute(QStringLiteral("widget")) == QLatin1String("corners")) {
                        // we want a corners-keyframe-widget
                        int relativePos = (m_metaInfo->monitor->position() - info.startPos).frames(KdenliveSettings::project_fps());
                        CornersWidget *corners = new CornersWidget(m_metaInfo->monitor, pa, m_in, m_out, relativePos, m_metaInfo->monitor->timecode(), e.attribute(QStringLiteral("active_keyframe"), QStringLiteral("-1")).toInt(), parent);
                        connect(this, &ParameterContainer::updateRange, corners, &KeyframeEdit::slotUpdateRange);
                        m_monitorEffectScene = MonitorSceneCorners;
                        connect(this, &ParameterContainer::updateFrameInfo, corners, &CornersWidget::setFrameSize);
                        connect(this, &ParameterContainer::syncEffectsPos, corners, &CornersWidget::slotSyncPosition);
                        geo = static_cast<KeyframeEdit *>(corners);
                    } else {
                        geo = new KeyframeEdit(pa, m_in, m_out, m_metaInfo->monitor->timecode(), e.attribute(QStringLiteral("active_keyframe"), QStringLiteral("-1")).toInt());
                        connect(this, &ParameterContainer::updateRange, geo, &KeyframeEdit::slotUpdateRange);
                    }
                    if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                        geo->setEnabled(false);
                        m_conditionalWidgets << geo;
                    }
                    m_vbox->addWidget(geo);
                    m_valueItems[paramName + QStringLiteral("keyframe")] = geo;
                    m_keyframeEditor = geo;
                    connect(geo, &KeyframeEdit::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                    connect(geo, &KeyframeEdit::seekToPos, this, &ParameterContainer::seekTimeline);
                    connect(this, &ParameterContainer::showComments, geo, &KeyframeEdit::slotShowComment);
                } else {
                    // we already have a keyframe editor, so just add another column for the new param
                    m_keyframeEditor->addParameter(pa);
                }
            } else if (type == QLatin1String("color")) {
                if (pa.hasAttribute(QStringLiteral("paramprefix"))) {
                    value.remove(0, pa.attribute(QStringLiteral("paramprefix")).size());
                }
                if (value.startsWith('#')) {
                    value = value.replace('#', QLatin1String("0x"));
                }
                ChooseColorWidget *choosecolor = new ChooseColorWidget(paramName, value, comment, pa.hasAttribute(QStringLiteral("alpha")), parent);
                m_vbox->addWidget(choosecolor);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    choosecolor->setEnabled(false);
                    m_conditionalWidgets << choosecolor;
                }
                m_valueItems[paramName] = choosecolor;
                connect(choosecolor, &ChooseColorWidget::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                connect(choosecolor, &ChooseColorWidget::disableCurrentFilter, this, &ParameterContainer::disableCurrentFilter);
            } else if (type == QLatin1String("position")) {
                int pos = value.toInt();
                auto id = effect.attribute(QStringLiteral("id"));
                if (id == QLatin1String("fadein") ||
                    id == QLatin1String("fade_from_black")) {
                    pos = pos - m_in;
                } else if (id == QLatin1String("fadeout") ||
                           id == QLatin1String("fade_to_black")) {
                    // fadeout position starts from clip end
                    pos = m_out - pos;
                } else {
                    qCDebug(KDENLIVE_LOG) << "Error: Invalid position effect id" << endl;
                }
                PositionWidget *posedit = new PositionWidget(paramName, pos, 0, m_out - m_in, m_metaInfo->monitor->timecode(), comment);
                connect(this, SIGNAL(updateRange(int, int)), posedit, SLOT(setRange(int, int)));
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    posedit->setEnabled(false);
                    m_conditionalWidgets << posedit;
                }
                m_vbox->addWidget(posedit);
                m_valueItems[paramName + QStringLiteral("position")] = posedit;
                connect(posedit, &PositionWidget::valueChanged,
                        this,    &ParameterContainer::slotCollectAllParameters);
            } else if (type == QLatin1String("curve")) {
                QList<QPointF> points;
                int number;
                double version = 0;
                QDomElement versionnode = effect.firstChildElement(QStringLiteral("version"));
                if (!versionnode.isNull()) {
                    version = locale.toDouble(versionnode.text());
                }
                if (version > 0.2) {
                    // Rounding gives really weird results. (int) (10 * 0.3) gives 2! So for now, add 0.5 to get correct result
                    number = locale.toDouble(EffectsList::parameter(e, pa.attribute(QStringLiteral("number")))) * 10 + 0.5;
                } else {
                    number = EffectsList::parameter(e, pa.attribute(QStringLiteral("number"))).toInt();
                }
                QString inName = pa.attribute(QStringLiteral("inpoints"));
                QString outName = pa.attribute(QStringLiteral("outpoints"));
                int start = pa.attribute(QStringLiteral("min")).toInt();
                for (int j = start; j <= number; ++j) {
                    QString in = inName;
                    in.replace(QLatin1String("%i"), QString::number(j));
                    QString out = outName;
                    out.replace(QLatin1String("%i"), QString::number(j));
                    points << QPointF(locale.toDouble(EffectsList::parameter(e, in)), locale.toDouble(EffectsList::parameter(e, out)));
                }
                QString curve_value = "";
                if (!points.isEmpty()) {
                    curve_value = KisCubicCurve(points).toString();
                }
                using Widget_t = CurveParamWidget<KisCurveWidget>;
                Widget_t *curve = new Widget_t(curve_value,parent);
                curve->setMaxPoints(pa.attribute(QStringLiteral("max")).toInt());
                m_vbox->addWidget(curve);
                connect(curve, &Widget_t::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                connect(this, &ParameterContainer::showComments, curve, &Widget_t::slotShowComment);
                m_valueItems[paramName] = curve;

                QString depends = pa.attribute(QStringLiteral("depends"));
                if (!depends.isEmpty()) {
                    meetDependency(paramName, type, EffectsList::parameter(e, depends));
                }
            } else if (type == QLatin1String("bezier_spline")) {
                // BezierSplineWidget *widget = new BezierSplineWidget(value, parent);
                using Widget_t = CurveParamWidget<BezierSplineEditor>;
                Widget_t *widget = new Widget_t(value,parent);
                stretch = false;
                m_vbox->addWidget(widget);
                m_valueItems[paramName] = widget;
                connect(widget, &Widget_t::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                QString depends = pa.attribute(QStringLiteral("depends"));
                if (!depends.isEmpty()) {
                    meetDependency(paramName, type, EffectsList::parameter(e, depends));
                }
            } else if (type == QLatin1String("roto-spline")) {
                m_monitorEffectScene = MonitorSceneRoto;
                RotoWidget *roto = new RotoWidget(value.toLatin1(), m_metaInfo->monitor, info, m_metaInfo->monitor->timecode(), parent);
                connect(roto, &RotoWidget::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                connect(roto, &RotoWidget::seekToPos, this, &ParameterContainer::seekTimeline);
                connect(this, &ParameterContainer::syncEffectsPos, roto, &RotoWidget::slotSyncPosition);
                m_vbox->addWidget(roto);
                m_valueItems[paramName] = roto;
            } else if (type == QLatin1String("wipe")) {
                Wipeval *wpval = new Wipeval;
                wpval->setupUi(toFillin);
                // Make sure button shows its pressed state even if widget loses focus
                QColor bg = QPalette().highlight().color();
                toFillin->setStyleSheet(QStringLiteral("QPushButton:checked {background-color:rgb(%1,%2,%3);}").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
                wipeInfo w = getWipeInfo(value);
                switch (w.start) {
                case UP:
                    wpval->start_up->setChecked(true);
                    break;
                case DOWN:
                    wpval->start_down->setChecked(true);
                    break;
                case RIGHT:
                    wpval->start_right->setChecked(true);
                    break;
                case LEFT:
                    wpval->start_left->setChecked(true);
                    break;
                default:
                    wpval->start_center->setChecked(true);
                    break;
                }
                switch (w.end) {
                case UP:
                    wpval->end_up->setChecked(true);
                    break;
                case DOWN:
                    wpval->end_down->setChecked(true);
                    break;
                case RIGHT:
                    wpval->end_right->setChecked(true);
                    break;
                case LEFT:
                    wpval->end_left->setChecked(true);
                    break;
                default:
                    wpval->end_center->setChecked(true);
                    break;
                }
                wpval->start_transp->setValue(w.startTransparency);
                wpval->end_transp->setValue(w.endTransparency);
                m_valueItems[paramName] = wpval;
                connect(wpval->end_up, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->end_down, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->end_left, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->end_right, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->end_center, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_up, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_down, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_left, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_right, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_center, &QAbstractButton::clicked, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->start_transp, &QAbstractSlider::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                connect(wpval->end_transp, &QAbstractSlider::valueChanged, this, &ParameterContainer::slotCollectAllParameters);
                //wpval->title->setTitle(na.toElement().text());
                m_uiItems.append(wpval);
            } else if (type == QLatin1String("url")) {
                Urlval *cval = new Urlval;
                cval->setupUi(toFillin);
                cval->label->setText(paramName);
                cval->setToolTip(comment);
                cval->widgetComment->setHidden(true);
                QString filter = pa.attribute(QStringLiteral("filter"));
                if (!filter.isEmpty()) {
                    cval->urlwidget->setFilter(filter);
                }
                m_valueItems[paramName] = cval;
                cval->urlwidget->setUrl(QUrl(value));
                connect(cval->urlwidget, SIGNAL(returnPressed()), this, SLOT(slotCollectAllParameters()));
                connect(cval->urlwidget, &KUrlRequester::urlSelected, this, &ParameterContainer::slotCollectAllParameters);
                if (m_conditionParameter && pa.hasAttribute(QStringLiteral("conditional"))) {
                    cval->setEnabled(false);
                    m_conditionalWidgets << cval;
                }
                m_uiItems.append(cval);
            } else if (type == QLatin1String("keywords")) {
                Keywordval *kval = new Keywordval;
                kval->setupUi(toFillin);
                kval->label->setText(paramName);
                kval->lineeditwidget->setText(value);
                kval->setToolTip(comment);
                QDomElement klistelem = pa.firstChildElement(QStringLiteral("keywords"));
                QDomElement kdisplaylistelem = pa.firstChildElement(QStringLiteral("keywordsdisplay"));
                QStringList keywordlist;
                QStringList keyworddisplaylist;
                if (!klistelem.isNull()) {
                    keywordlist = klistelem.text().split(QLatin1Char(';'));
                    keyworddisplaylist = i18n(kdisplaylistelem.text().toUtf8().data()).split(QLatin1Char(';'));
                }
                if (keyworddisplaylist.count() != keywordlist.count()) {
                    keyworddisplaylist = keywordlist;
                }
                for (int j = 0; j < keywordlist.count(); ++j) {
                    kval->comboboxwidget->addItem(keyworddisplaylist.at(j), keywordlist.at(j));
                }
                // Add disabled user prompt at index 0
                kval->comboboxwidget->insertItem(0, i18n("<select a keyword>"), "");
                kval->comboboxwidget->model()->setData(kval->comboboxwidget->model()->index(0, 0), QVariant(Qt::NoItemFlags), Qt::UserRole - 1);
                kval->comboboxwidget->setCurrentIndex(0);
                m_valueItems[paramName] = kval;
                connect(kval->lineeditwidget, &QLineEdit::editingFinished, this, &ParameterContainer::slotCollectAllParameters);
                connect(kval->comboboxwidget, SIGNAL(activated(QString)), this, SLOT(slotCollectAllParameters()));
                m_uiItems.append(kval);
            } else if (type == QLatin1String("fontfamily")) {
                Fontval *fval = new Fontval;
                fval->setupUi(toFillin);
                fval->name->setText(paramName);
                fval->fontfamilywidget->setCurrentFont(QFont(value));
                m_valueItems[paramName] = fval;
                connect(fval->fontfamilywidget, &QFontComboBox::currentFontChanged, this, &ParameterContainer::slotCollectAllParameters);
                m_uiItems.append(fval);
            } else if (type == QLatin1String("filterjob")) {
                QVBoxLayout *l = new QVBoxLayout(toFillin);
                QPushButton *button = new QPushButton(toFillin);
                button->setProperty("realName", paramName);
                if (effect.hasAttribute(QStringLiteral("condition"))) {
                    if (m_conditionParameter) {
                        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
                        QString conditionalName = na.attribute(QStringLiteral("conditional"));
                        if (!conditionalName.isEmpty()) {
                            paramName = conditionalName;
                        }
                    }
                }
                button->setText(paramName);
                l->addWidget(button);
                m_valueItems[paramName] = button;
                connect(button, &QAbstractButton::pressed, this, &ParameterContainer::slotStartFilterJobAction);
            } else if (type == QLatin1String("readonly")) {
                QHBoxLayout *lay = new QHBoxLayout(toFillin);
                DraggableLabel *lab = new DraggableLabel(QStringLiteral("<a href=\"%1\">").arg(pa.attribute(QStringLiteral("name"))) + paramName + QStringLiteral("</a>"));
                lab->setObjectName(pa.attribute(QStringLiteral("name")));
                connect(lab, &QLabel::linkActivated, this, &ParameterContainer::copyData);
                connect(lab, &DraggableLabel::startDrag, this, &ParameterContainer::makeDrag);
                lay->setContentsMargins(0, 0, 0, 0);
                lay->addWidget(lab);
                lab->setVisible(m_conditionParameter);
                lab->setProperty("revert", 1);
                m_conditionalWidgets << lab;
            } else {
                delete toFillin;
                toFillin = nullptr;
            }

            if (toFillin) {
                m_vbox->addWidget(toFillin);
            }
        }

    if (effect.hasAttribute(QStringLiteral("sync_in_out"))) {
        QCheckBox *cb = new QCheckBox(i18n("sync keyframes with clip start"));
        cb->setChecked(effect.attribute(QStringLiteral("sync_in_out")) == QLatin1String("1"));
        m_vbox->addWidget(cb);
        connect(cb, &QCheckBox::toggled, this, &ParameterContainer::toggleSync);
    }
    if (stretch) {
        m_vbox->addStretch(2);
    }

    if (m_keyframeEditor) {
        m_keyframeEditor->checkVisibleParam();
    }

    if (m_animationWidget) {
        m_animationWidget->finishSetup();
    }

    // Make sure all doubleparam spinboxes have the same width, looks much better
    QList<DoubleParameterWidget *> allWidgets = findChildren<DoubleParameterWidget *>();
    int minSize = 0;
    for (int i = 0; i < allWidgets.count(); ++i) {
        if (minSize < allWidgets.at(i)->spinSize()) {
            minSize = allWidgets.at(i)->spinSize();
        }
    }
    for (int i = 0; i < allWidgets.count(); ++i) {
        allWidgets.at(i)->setSpinSize(minSize);
    }
}

ParameterContainer::~ParameterContainer()
{
    clearLayout(m_vbox);
    delete m_vbox;
}

void ParameterContainer::copyData(const QString &name)
{
    QString value = EffectsList::parameter(m_effect, name);
    if (value.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(value);
}

void ParameterContainer::makeDrag(const QString &name)
{
    QString value = EffectsList::parameter(m_effect, name);
    if (value.isEmpty()) {
        return;
    }
    value.prepend(name + QLatin1Char('='));
    QDrag *dr = new QDrag(qApp);
    // The data to be transferred by the drag and drop operation is contained in a QMimeData object
    QMimeData *mimeData = new QMimeData;
    QByteArray data;
    data.append(value.toUtf8());
    mimeData->setData(QStringLiteral("kdenlive/geometry"),  data);
    // Assign ownership of the QMimeData object to the QDrag object.
    dr->setMimeData(mimeData);
    // Start the drag and drop operation
    dr->exec(Qt::CopyAction);
}

void ParameterContainer::toggleSync(bool enable)
{
    const QDomElement oldparam = m_effect.cloneNode().toElement();
    m_effect.setAttribute(QStringLiteral("sync_in_out"), enable ? "1" : "0");
    if (!enable) {
        int oldIn = m_effect.attribute(QStringLiteral("in")).toInt();
        // geometry / animation attached to 0
        if (oldIn > 0) {
            if (m_animationWidget) {
                // Switch keyframes offset
                m_animationWidget->offsetAnimation(oldIn);
                // Save updated animation to xml effect
                QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
                QMap<QString, QString> values = m_animationWidget->getAnimation();
                for (int i = 0; i < namenode.count(); ++i) {
                    QDomElement pa = namenode.item(i).toElement();
                    QString paramName = pa.attribute(QStringLiteral("name"));
                    if (values.count() > 1) {
                        pa.setAttribute(QStringLiteral("intimeline"), m_animationWidget->isActive(paramName) ? "1" : "0");
                    }
                    const QString val = values.value(paramName);
                    if (!val.isEmpty()) {
                        pa.setAttribute(QStringLiteral("value"), val);
                    }
                }
            }
            if (m_geometryWidget) {
                // Switch keyframes offset
                QString updated = m_geometryWidget->offsetAnimation(oldIn, true);
                QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
                for (int i = 0; i < namenode.count(); ++i) {
                    QDomElement pa = namenode.item(i).toElement();
                    if (pa.attribute(QStringLiteral("type")) == QLatin1String("geometry")) {
                        pa.setAttribute(QStringLiteral("value"), updated);
                    }
                }
            }
        }
        m_effect.removeAttribute(QStringLiteral("in"));
        m_effect.removeAttribute(QStringLiteral("out"));
    } else {
        // geometry / animation attached to clip's crop start
        if (m_in > 0) {
            if (m_animationWidget) {
                // Switch keyframes offset
                m_animationWidget->offsetAnimation(-m_in);
                // Save updated animation to xml effect
                QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
                QMap<QString, QString> values = m_animationWidget->getAnimation();
                for (int i = 0; i < namenode.count(); ++i) {
                    QDomElement pa = namenode.item(i).toElement();
                    QString paramName = pa.attribute(QStringLiteral("name"));
                    if (values.count() > 1) {
                        pa.setAttribute(QStringLiteral("intimeline"), m_animationWidget->isActive(paramName) ? "1" : "0");
                    }
                    const QString val = values.value(paramName);
                    if (!val.isEmpty()) {
                        pa.setAttribute(QStringLiteral("value"), val);
                    }
                }
            }
            if (m_geometryWidget) {
                // Switch keyframes offset
                QString updated = m_geometryWidget->offsetAnimation(-m_in, false);
                QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
                for (int i = 0; i < namenode.count(); ++i) {
                    QDomElement pa = namenode.item(i).toElement();
                    if (pa.attribute(QStringLiteral("type")) == QLatin1String("geometry")) {
                        pa.setAttribute(QStringLiteral("value"), updated);
                    }
                }
            }
        }
        m_effect.setAttribute(QStringLiteral("in"), QString::number(m_in));
        m_effect.setAttribute(QStringLiteral("out"), QString::number(m_out));
    }
    emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
}

void ParameterContainer::meetDependency(const QString &name, const QString &type, const QString &value)
{
    if (type == QLatin1String("curve")) {
        using Widget_t = CurveParamWidget<KisCurveWidget>;
        Widget_t *curve = static_cast<Widget_t *>(m_valueItems[name]);
        if (curve) {
            QLocale locale;
            double mode = locale.toDouble(value);
            if (mode < 1.) {
                mode *= 10; //hack to deal with new versions of the effect that set mode in [0,1]
            }
            curve->setMode((Widget_t::CurveModes)int(mode + 0.5));
        }
    } else if (type == QLatin1String("bezier_spline")) {
        using Widget_t = CurveParamWidget<BezierSplineEditor>;
        Widget_t *widget = static_cast<Widget_t *>(m_valueItems[name]);
        if (widget) {
            QLocale locale;
            widget->setMode((Widget_t::CurveModes)((int)(locale.toDouble(value) * 10 + 0.5)));
        }
    }
}

wipeInfo ParameterContainer::getWipeInfo(QString value)
{
    wipeInfo info;
    // Convert old geometry values that used a comma as separator
    if (value.contains(QLatin1Char(','))) {
        value.replace(',', '/');
    }
    QString start = value.section(QLatin1Char(';'), 0, 0);
    QString end = value.section(QLatin1Char(';'), 1, 1).section(QLatin1Char('='), 1, 1);
    if (start.startsWith(QLatin1String("-100%/0"))) {
        info.start = LEFT;
    } else if (start.startsWith(QLatin1String("100%/0"))) {
        info.start = RIGHT;
    } else if (start.startsWith(QLatin1String("0%/100%"))) {
        info.start = DOWN;
    } else if (start.startsWith(QLatin1String("0%/-100%"))) {
        info.start = UP;
    } else {
        info.start = CENTER;
    }

    if (start.count(':') == 2) {
        info.startTransparency = start.section(QLatin1Char(':'), -1).toInt();
    } else {
        info.startTransparency = 100;
    }

    if (end.startsWith(QLatin1String("-100%/0"))) {
        info.end = LEFT;
    } else if (end.startsWith(QLatin1String("100%/0"))) {
        info.end = RIGHT;
    } else if (end.startsWith(QLatin1String("0%/100%"))) {
        info.end = DOWN;
    } else if (end.startsWith(QLatin1String("0%/-100%"))) {
        info.end = UP;
    } else {
        info.end = CENTER;
    }

    if (end.count(':') == 2) {
        info.endTransparency = end.section(QLatin1Char(':'), -1).toInt();
    } else {
        info.endTransparency = 100;
    }

    return info;
}

void ParameterContainer::updateTimecodeFormat()
{
    if (m_keyframeEditor) {
        m_keyframeEditor->updateTimecodeFormat();
    }

    if (m_animationWidget) {
        m_animationWidget->updateTimecodeFormat();
    }

    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomNode pa = namenode.item(i);
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QString type = pa.attributes().namedItem(QStringLiteral("type")).nodeValue();
        QString paramName = na.isNull() ? pa.attributes().namedItem(QStringLiteral("name")).nodeValue() : i18n(na.text().toUtf8().data());

        if (type == QLatin1String("geometry")) {
            if (m_geometryWidget) {
                m_geometryWidget->updateTimecodeFormat();
            }
            break;
        } else if (type == QLatin1String("position")) {
            PositionWidget *posi = qobject_cast<PositionWidget *>(m_valueItems[paramName]);
            posi->updateTimecodeFormat();
            break;
        } else if (type == QLatin1String("roto-spline")) {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems[paramName]);
            widget->updateTimecodeFormat();
        }
    }
}

void ParameterContainer::slotCollectAllParameters()
{
    if ((m_valueItems.isEmpty() && !m_animationWidget && !m_geometryWidget) || m_effect.isNull()) {
        return;
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    const QDomElement oldparam = m_effect.cloneNode().toElement();
    //QDomElement newparam = oldparam.cloneNode().toElement();

    if (m_effect.attribute(QStringLiteral("id")) == QLatin1String("movit.lift_gamma_gain") || m_effect.attribute(QStringLiteral("id")) == QLatin1String("lift_gamma_gain")) {
        LumaLiftGain *gainWidget = static_cast<LumaLiftGain *>(m_valueItems.value(m_effect.attribute(QStringLiteral("id"))));
        gainWidget->updateEffect(m_effect);
        emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
        return;
    }
    if (m_effect.attribute(QStringLiteral("tag")) == QLatin1String("avfilter.selectivecolor")) {
        SelectiveColor *cmykAdjust = static_cast<SelectiveColor *>(m_valueItems.value(m_effect.attribute(QStringLiteral("id"))));
        cmykAdjust->updateEffect(m_effect);
        emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
        return;
    }

    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));

    // special case, m_animationWidget can hold several parameters
    if (m_animationWidget) {
        QMap<QString, QString> values = m_animationWidget->getAnimation();
        for (int i = 0; i < namenode.count(); ++i) {
            QDomElement pa = namenode.item(i).toElement();
            QString paramName = pa.attribute(QStringLiteral("name"));
            if (values.count() > 1) {
                pa.setAttribute(QStringLiteral("intimeline"), m_animationWidget->isActive(paramName) ? "1" : "0");
            }
            const QString val = values.value(paramName);
            if (!val.isEmpty()) {
                pa.setAttribute(QStringLiteral("value"), val);
            }
        }
    }

    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QString type = pa.attribute(QStringLiteral("type"));
        QString paramName = na.isNull() ? pa.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
        if (type == QLatin1String("complex")) {
            paramName.append(QStringLiteral("complex"));
        } else if (type == QLatin1String("position")) {
            paramName.append(QStringLiteral("position"));
        } else if (type == QLatin1String("geometry")) {
            paramName.append(QStringLiteral("geometry"));
        } else if (type == QLatin1String("keyframe")) {
            paramName.append(QStringLiteral("keyframe"));
        } else if (type == QLatin1String("animated")) {
            continue;
        }
        if (type != QLatin1String("animatedrect") && type != QLatin1String("simplekeyframe") && type != QLatin1String("fixed") && type != QLatin1String("addedgeometry") && !m_valueItems.contains(paramName)) {
            qCDebug(KDENLIVE_LOG) << "// Param: " << paramName << " NOT FOUND";
            continue;
        }

        QString setValue;
        if (type == QLatin1String("double") || type == QLatin1String("constant")) {
            DoubleParameterWidget *doubleparam = static_cast<DoubleParameterWidget *>(m_valueItems.value(paramName));
            if (doubleparam) {
                setValue = locale.toString(doubleparam->getValue());
            }
        } else if (type == QLatin1String("list")) {
            ListParamWidget *val = qobject_cast<ListParamWidget *>(m_valueItems.value(paramName));
            if (val) {
                setValue = val->getValue();
                // special case, list value is allowed to be empty
                pa.setAttribute(QStringLiteral("value"), setValue);
                setValue.clear();
            }
        } else if (type == QLatin1String("bool")) {
            BoolParamWidget *val = qobject_cast<BoolParamWidget *>(m_valueItems.value(paramName));
            if (val) {
                setValue = val->getValue() ? "1" : "0" ;
            }
        } else if (type == QLatin1String("switch")) {
            BoolParamWidget *val = qobject_cast<BoolParamWidget *>(m_valueItems.value(paramName));
            if (val) {
                setValue = val->getValue() ? pa.attribute("max") : pa.attribute("min") ;
            }
        } else if (type == QLatin1String("color")) {
            ChooseColorWidget *choosecolor = static_cast<ChooseColorWidget *>(m_valueItems.value(paramName));
            if (choosecolor) {
                setValue = choosecolor->getColor();
            }
            if (pa.hasAttribute(QStringLiteral("paramprefix"))) {
                setValue.prepend(pa.attribute(QStringLiteral("paramprefix")));
            }
        } else if (type == QLatin1String("geometry")) {
            if (m_geometryWidget) {
                namenode.item(i).toElement().setAttribute(QStringLiteral("value"), m_geometryWidget->getValue());
            }
        } else if (type == QLatin1String("addedgeometry")) {
            if (m_geometryWidget) {
                namenode.item(i).toElement().setAttribute(QStringLiteral("value"), m_geometryWidget->getExtraValue(namenode.item(i).toElement().attribute(QStringLiteral("name"))));
            }
        } else if (type == QLatin1String("position")) {
            PositionWidget *pedit = qobject_cast<PositionWidget *>(m_valueItems.value(paramName));
            int pos = 0;
            if (pedit) {
                pos = pedit->getPosition();
            }
            auto id = m_effect.attribute(QStringLiteral("id"));
            auto effect_in = m_in;
            auto effect_out = m_out;
            if (id == QLatin1String("fadein") || id == QLatin1String("fade_from_black")) {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                effect_out = m_in + pos;
            } else if (id == QLatin1String("fadeout") ||
                       id == QLatin1String("fade_to_black")) {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                effect_in = m_out - pos;
            }
            EffectsList::setParameter(m_effect, QStringLiteral("in"),
                                      QString::number(effect_in));
            EffectsList::setParameter(m_effect, QStringLiteral("out"),
                                      QString::number(effect_out));
        } else if (type == QLatin1String("curve")) {
            using Widget_t = CurveParamWidget<KisCurveWidget>;
            Widget_t *curve = static_cast<Widget_t *>(m_valueItems.value(paramName));
            // KisCurveWidget *curve = static_cast<KisCurveWidget *>(m_valueItems.value(paramName));
            if (curve) {
                QList<QPointF> points = curve->getPoints();
                QString number = pa.attribute(QStringLiteral("number"));
                QString inName = pa.attribute(QStringLiteral("inpoints"));
                QString outName = pa.attribute(QStringLiteral("outpoints"));
                int off = pa.attribute(QStringLiteral("min")).toInt();
                int end = pa.attribute(QStringLiteral("max")).toInt();
                double version = 0;
                QDomElement versionnode = m_effect.firstChildElement(QStringLiteral("version"));
                if (!versionnode.isNull()) {
                    version = locale.toDouble(versionnode.text());
                }
                if (version > 0.2) {
                    EffectsList::setParameter(m_effect, number, locale.toString(points.count() / 10.));
                } else {
                    EffectsList::setParameter(m_effect, number, QString::number(points.count()));
                }
                for (int j = 0; (j < points.count() && j + off <= end); ++j) {
                    QString in = inName;
                    in.replace(QLatin1String("%i"), QString::number(j + off));
                    QString out = outName;
                    out.replace(QLatin1String("%i"), QString::number(j + off));
                    EffectsList::setParameter(m_effect, in, locale.toString(points.at(j).x()));
                    EffectsList::setParameter(m_effect, out, locale.toString(points.at(j).y()));
                }
            }
            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty()) {
                meetDependency(paramName, type, EffectsList::parameter(m_effect, depends));
            }
        } else if (type == QLatin1String("bezier_spline")) {
            using Widget_t = CurveParamWidget<BezierSplineEditor>;
            Widget_t *widget = static_cast<Widget_t *>(m_valueItems.value(paramName));
            if (widget) {
                setValue = widget->toString();
            }
            QString depends = pa.attribute(QStringLiteral("depends"));
            if (!depends.isEmpty()) {
                meetDependency(paramName, type, EffectsList::parameter(m_effect, depends));
            }
        } else if (type == QLatin1String("roto-spline")) {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems.value(paramName));
            if (widget) {
                setValue = widget->getSpline();
            }
        } else if (type == QLatin1String("wipe")) {
            Wipeval *wp = static_cast<Wipeval *>(m_valueItems.value(paramName));
            if (wp) {
                wipeInfo info;
                if (wp->start_left->isChecked()) {
                    info.start = LEFT;
                } else if (wp->start_right->isChecked()) {
                    info.start = RIGHT;
                } else if (wp->start_up->isChecked()) {
                    info.start = UP;
                } else if (wp->start_down->isChecked()) {
                    info.start = DOWN;
                } else if (wp->start_center->isChecked()) {
                    info.start = CENTER;
                } else {
                    info.start = LEFT;
                }
                info.startTransparency = wp->start_transp->value();

                if (wp->end_left->isChecked()) {
                    info.end = LEFT;
                } else if (wp->end_right->isChecked()) {
                    info.end = RIGHT;
                } else if (wp->end_up->isChecked()) {
                    info.end = UP;
                } else if (wp->end_down->isChecked()) {
                    info.end = DOWN;
                } else if (wp->end_center->isChecked()) {
                    info.end = CENTER;
                } else {
                    info.end = RIGHT;
                }
                info.endTransparency = wp->end_transp->value();

                setValue = getWipeString(info);
            }
        } else if ((type == QLatin1String("simplekeyframe") || type == QLatin1String("keyframe")) && m_keyframeEditor) {
            QString realName = i18n(na.toElement().text().toUtf8().data());
            QString val = m_keyframeEditor->getValue(realName);
            pa.setAttribute(m_keyframeEditor->getTag(), val);

            if (m_keyframeEditor->isVisibleParam(realName)) {
                pa.setAttribute(QStringLiteral("intimeline"), QStringLiteral("1"));
            } else if (pa.hasAttribute(QStringLiteral("intimeline"))) {
                pa.setAttribute(QStringLiteral("intimeline"), QStringLiteral("0"));
            }
        } else if (type == QLatin1String("url")) {
            KUrlRequester *req = static_cast<Urlval *>(m_valueItems.value(paramName))->urlwidget;
            if (req) {
                setValue = req->url().toLocalFile();
            }
        } else if (type == QLatin1String("keywords")) {
            Keywordval *val = static_cast<Keywordval *>(m_valueItems.value(paramName));
            if (val) {
                QLineEdit *line = val->lineeditwidget;
                KComboBox *combo = val->comboboxwidget;
                if (combo->currentIndex()) {
                    QString comboval = combo->itemData(combo->currentIndex()).toString();
                    line->insert(comboval);
                    combo->setCurrentIndex(0);
                }
                setValue = line->text();
            }
        } else if (type == QLatin1String("fontfamily")) {
            Fontval *val = static_cast<Fontval *>(m_valueItems.value(paramName));
            if (val) {
                QFontComboBox *fontfamily = val->fontfamilywidget;
                setValue = fontfamily->currentFont().family();
            }
        }
        if (!setValue.isNull()) {
            pa.setAttribute(QStringLiteral("value"), setValue);
        }
    }
    emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
}

QString ParameterContainer::getWipeString(wipeInfo info)
{

    QString start;
    QString end;
    switch (info.start) {
    case LEFT:
        start = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        start = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        start = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        start = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        start = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    start.append(':' + QString::number(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        end = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        end = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        end = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        end = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    end.append(':' + QString::number(info.endTransparency));
    return QString(start + QStringLiteral(";-1=") + end);
}

void ParameterContainer::updateParameter(const QString &key, const QString &value)
{
    m_effect.setAttribute(key, value);
}

void ParameterContainer::slotStartFilterJobAction()
{
    if (m_conditionParameter) {
        // job has already been run, reset
        QString resetParam = m_effect.attribute(QStringLiteral("condition"));
        if (!resetParam.isEmpty()) {
            QPushButton *button = (QPushButton *) QObject::sender();
            if (button) {
                button->setText(button->property("realName").toString());
            }
            const QDomElement oldparam = m_effect.cloneNode().toElement();
            EffectsList::setParameter(m_effect, resetParam, QString());
            emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
            // Re-enable locked parameters
            foreach (QWidget *w, m_conditionalWidgets) {
                if (w->property("revert").toInt() == 1) {
                    // This widget should only be displayed if we have a result, so hide
                    w->setVisible(false);
                    continue;
                }
                if (!w->isVisible()) {
                    w->setVisible(true);
                }
                w->setEnabled(true);
            }
            m_conditionParameter = false;
            return;
        }
    }
    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QString type = pa.attribute(QStringLiteral("type"));
        if (type == QLatin1String("filterjob")) {
            QMap<QString, QString> filterParams;
            QMap<QString, QString> consumerParams;
            filterParams.insert(QStringLiteral("filter"), pa.attribute(QStringLiteral("filtertag")));
            consumerParams.insert(QStringLiteral("consumer"), pa.attribute(QStringLiteral("consumer")));
            QString filterattributes = pa.attribute(QStringLiteral("filterparams"));
            if (filterattributes.contains(QStringLiteral("%position"))) {
                int filterStart = qBound(m_in, (int)(m_metaInfo->monitor->position() - m_info.startPos).frames(KdenliveSettings::project_fps()) + m_in, m_out);
                filterattributes.replace(QLatin1String("%position"), QString::number(filterStart));
            }

            // Fill filter params
            QStringList filterList = filterattributes.split(QLatin1Char(' '));
            QString param;
            for (int j = 0; j < filterList.size(); ++j) {
                param = filterList.at(j);
                if (param != QLatin1String("%params")) {
                    filterParams.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
                }
            }
            if (filterattributes.contains(QStringLiteral("%params"))) {
                // Replace with current geometry
                EffectsParameterList parameters;
                QDomNodeList params = m_effect.elementsByTagName(QStringLiteral("parameter"));
                EffectsController::adjustEffectParameters(parameters, params, m_metaInfo->monitor->profileInfo());
                for (int j = 0; j < parameters.count(); ++j) {
                    filterParams.insert(parameters.at(j).name(), parameters.at(j).value());
                }
            }
            // Fill consumer params
            QString consumerattributes = pa.attribute(QStringLiteral("consumerparams"));
            QStringList consumerList = consumerattributes.split(QLatin1Char(' '));
            for (int j = 0; j < consumerList.size(); ++j) {
                param = consumerList.at(j);
                if (param != QLatin1String("%params")) {
                    consumerParams.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
                }
            }

            // Fill extra params
            QMap<QString, QString> extraParams;
            QDomNodeList jobparams = pa.elementsByTagName(QStringLiteral("jobparam"));
            for (int j = 0; j < jobparams.count(); ++j) {
                QDomElement e = jobparams.item(j).toElement();
                extraParams.insert(e.attribute(QStringLiteral("name")), e.text().toUtf8());
            }
            extraParams.insert(QStringLiteral("offset"), QString::number(m_in));
            emit startFilterJob(filterParams, consumerParams, extraParams);
            break;
        }
    }
}

void ParameterContainer::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while ((item = layout->takeAt(0))) {
        if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

MonitorSceneType ParameterContainer::needsMonitorEffectScene() const
{
    return m_monitorEffectScene;
}

void ParameterContainer::setKeyframes(const QString &tag, const QString &data)
{
    const QDomElement oldparam = m_effect.cloneNode().toElement();
    QDomNodeList namenode = m_effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.attribute(QStringLiteral("name")) == tag) {
            pa.setAttribute(QStringLiteral("value"), data);
            break;
        }
    }
    if (m_geometryWidget) {
        // Reload keyframes
        m_geometryWidget->reload(tag, data);
    }
    if (m_animationWidget) {
        // Reload keyframes
        m_animationWidget->reload(tag, data);
    }
    emit parameterChanged(oldparam, m_effect, m_effect.attribute(QStringLiteral("kdenlive_ix")).toInt());
}

void ParameterContainer::setRange(int inPoint, int outPoint)
{
    m_in = inPoint;
    m_out = outPoint;
    emit updateRange(m_in, m_out);
}

QPoint ParameterContainer::range() const
{
    QPoint range(m_in, m_out);
    return range;
}

int ParameterContainer::contentHeight() const
{
    return m_vbox->sizeHint().height();
}

void ParameterContainer::refreshFrameInfo()
{
    emit updateFrameInfo(m_metaInfo->frameSize, m_metaInfo->stretchFactor);
}

void ParameterContainer::setActiveKeyframe(int frame)
{
    if (m_animationWidget) {
        m_animationWidget->setActiveKeyframe(frame);
    }
}

void ParameterContainer::connectMonitor(bool activate)
{
    if (m_animationWidget) {
        m_animationWidget->connectMonitor(activate);
    }
    if (m_geometryWidget) {
        m_geometryWidget->connectMonitor(activate);
    }
}

bool ParameterContainer::doesAcceptDrops() const
{
    return m_acceptDrops;
}

