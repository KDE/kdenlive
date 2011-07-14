/***************************************************************************
                          effecstackedit.cpp  -  description
                             -------------------
    begin                : Feb 15 2008
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

#include "effectstackedit.h"
#include "ui_listval_ui.h"
#include "ui_boolval_ui.h"
#include "ui_wipeval_ui.h"
#include "ui_urlval_ui.h"
#include "complexparameter.h"
#include "geometryval.h"
#include "positionedit.h"
#include "projectlist.h"
#include "effectslist.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"
#include "kis_curve_widget.h"
#include "kis_cubic_curve.h"
#include "choosecolorwidget.h"
#include "geometrywidget.h"
#include "colortools.h"
#include "doubleparameterwidget.h"
#include "cornerswidget.h"
#include "beziercurve/beziersplinewidget.h"
#ifdef QJSON
#include "rotoscoping/rotowidget.h"
#endif

#include <KDebug>
#include <KLocale>
#include <KFileDialog>
#include <KColorScheme>

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>

// For QDomNode debugging (output into files); leaving here as sample code.
//#define DEBUG_ESE


class Boolval: public QWidget, public Ui::Boolval_UI
{
};

class Listval: public QWidget, public Ui::Listval_UI
{
};

class Wipeval: public QWidget, public Ui::Wipeval_UI
{
};

class Urlval: public QWidget, public Ui::Urlval_UI
{
};

QMap<QString, QImage> EffectStackEdit::iconCache;

EffectStackEdit::EffectStackEdit(Monitor *monitor, QWidget *parent) :
    QScrollArea(parent),
    m_in(0),
    m_out(0),
    m_frameSize(QPoint()),
    m_keyframeEditor(NULL),
    m_monitor(monitor),
    m_geometryWidget(NULL)
{
    m_baseWidget = new QWidget(this);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
    
    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::View, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QColor dark_bg = scheme.shade(KColorScheme::DarkShade);
    QColor selected_bg = scheme.decoration(KColorScheme::FocusColor).color();
    QColor hover_bg = scheme.decoration(KColorScheme::HoverColor).color();    
    QColor light_bg = scheme.shade(KColorScheme::LightShade);
    
    QString stylesheet(QString("QProgressBar:horizontal {border: 1px solid %1;border-radius:0px;border-top-left-radius: 4px;border-bottom-left-radius: 4px;border-right: 0px;background:%4;padding: 0px;text-align:left center}\
                                QProgressBar:horizontal#dragOnly {background: %1} QProgressBar:horizontal:hover#dragOnly {background: %3} QProgressBar:horizontal:hover {border: 1px solid %3;border-right: 0px;}\
                                QProgressBar::chunk:horizontal {background: %1;} QProgressBar::chunk:horizontal:hover {background: %3;}\
                                QProgressBar:horizontal[inTimeline=\"true\"] { border: 1px solid %2;border-right: 0px;background: %4;padding: 0px;text-align:left center } QProgressBar::chunk:horizontal[inTimeline=\"true\"] {background: %2;}\
                                QAbstractSpinBox#dragBox {border: 1px solid %1;border-top-right-radius: 4px;border-bottom-right-radius: 4px;padding-right:0px;} QAbstractSpinBox::down-button#dragBox {width:0px;padding:0px;}\
                                QAbstractSpinBox::up-button#dragBox {width:0px;padding:0px;} QAbstractSpinBox[inTimeline=\"true\"]#dragBox { border: 1px solid %2;} QAbstractSpinBox:hover#dragBox {border: 1px solid %3;} ")
                                .arg(dark_bg.name()).arg(selected_bg.name()).arg(hover_bg.name()).arg(light_bg.name()));
    setStyleSheet(stylesheet);
    
    setWidget(m_baseWidget);
    setWidgetResizable(true);
    m_vbox = new QVBoxLayout(m_baseWidget);
    m_vbox->setContentsMargins(0, 0, 0, 0);
    m_vbox->setSpacing(2);
}

EffectStackEdit::~EffectStackEdit()
{
    iconCache.clear();
    delete m_baseWidget;
}

void EffectStackEdit::setFrameSize(QPoint p)
{
    m_frameSize = p;
    QDomNodeList namenode = m_params.elementsByTagName("parameter");
    for (int i = 0; i < namenode.count() ; i++) {
        QDomNode pa = namenode.item(i);
        QDomElement na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = na.isNull() ? pa.attributes().namedItem("name").nodeValue() : i18n(na.text().toUtf8().data());

        if (type == "geometry") {
            if (!KdenliveSettings::on_monitor_effects()) {
                Geometryval *geom = ((Geometryval*)m_valueItems[paramName+"geometry"]);
                geom->setFrameSize(m_frameSize);
                break;
            }
            else {
                if (m_geometryWidget) m_geometryWidget->setFrameSize(m_frameSize);
                break;
            }
        }
    }
}

void EffectStackEdit::updateTimecodeFormat()
{
    if (m_keyframeEditor)
        m_keyframeEditor->updateTimecodeFormat();

    QDomNodeList namenode = m_params.elementsByTagName("parameter");
    for (int i = 0; i < namenode.count() ; i++) {
        QDomNode pa = namenode.item(i);
        QDomElement na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = na.isNull() ? pa.attributes().namedItem("name").nodeValue() : i18n(na.text().toUtf8().data());

        if (type == "geometry") {
            if (KdenliveSettings::on_monitor_effects()) {
                if (m_geometryWidget) m_geometryWidget->updateTimecodeFormat();
            } else {
                Geometryval *geom = ((Geometryval*)m_valueItems[paramName+"geometry"]);
                geom->updateTimecodeFormat();
            }
            break;
        } else if (type == "position") {
            PositionEdit *posi = ((PositionEdit*)m_valueItems[paramName+"position"]);
            posi->updateTimecodeFormat();
            break;
        } else if (type == "roto-spline") {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems[paramName]);
            widget->updateTimecodeFormat();
        }
    }
}

void EffectStackEdit::meetDependency(const QString& name, QString type, QString value)
{
    if (type == "curve") {
        KisCurveWidget *curve = (KisCurveWidget*)m_valueItems[name];
        if (curve) {
            int color = value.toInt();
            curve->setPixmap(QPixmap::fromImage(ColorTools::rgbCurvePlane(curve->size(), (ColorTools::ColorsRGB)(color == 3 ? 4 : color), 0.8)));
        }
    } else if (type == "bezier_spline") {
        BezierSplineWidget *widget = (BezierSplineWidget*)m_valueItems[name];
        if (widget) {
            widget->setMode((BezierSplineWidget::CurveModes)((int)(value.toDouble() * 10)));
        }
    }
}

void EffectStackEdit::updateProjectFormat(MltVideoProfile profile, Timecode t)
{
    m_profile = profile;
    m_timecode = t;
}

void EffectStackEdit::updateParameter(const QString &name, const QString &value)
{
    m_params.setAttribute(name, value);

    if (name == "disable") {
        // if effect is disabled, disable parameters widget
        bool enabled = value.toInt() == 0 || !KdenliveSettings::disable_effect_parameters();
        setEnabled(enabled);
        emit effectStateChanged(enabled);
    }
}

void EffectStackEdit::transferParamDesc(const QDomElement d, ItemInfo info, bool isEffect)
{
    clearAllItems();
    if (m_keyframeEditor) delete m_keyframeEditor;
    m_keyframeEditor = NULL;
    m_params = d;
    m_in = isEffect ? info.cropStart.frames(KdenliveSettings::project_fps()) : info.startPos.frames(KdenliveSettings::project_fps());
    m_out = isEffect ? (info.cropStart + info.cropDuration).frames(KdenliveSettings::project_fps()) - 1 : info.endPos.frames(KdenliveSettings::project_fps());
    if (m_params.isNull()) {
//         kDebug() << "// EMPTY EFFECT STACK";
        return;
    }

    QDomNodeList namenode = m_params.elementsByTagName("parameter");
#ifdef DEBUG_ESE
    QFile debugFile("/tmp/namenodes.txt");
    if (debugFile.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream out(&debugFile);
        QTextStream out2(stdout);
        for (int i = 0; i < namenode.size(); i++) {
            out << i << ": \n";
            namenode.at(i).save(out, 2);
            out2 << i << ": \n";
            namenode.at(i).save(out2, 2);
        }
    }
#endif
    QDomElement e = m_params.toElement();
    int minFrame = e.attribute("start").toInt();
    int maxFrame = e.attribute("end").toInt();
    // In transitions, maxFrame is in fact one frame after the end of transition
    if (maxFrame > 0) maxFrame --;

    bool disable = d.attribute("disable") == "1" && KdenliveSettings::disable_effect_parameters();
    setEnabled(!disable);

    bool stretch = true;


    for (int i = 0; i < namenode.count() ; i++) {
        QDomElement pa = namenode.item(i).toElement();
        QDomElement na = pa.firstChildElement("name");
        QDomElement commentElem = pa.firstChildElement("comment");
        QString type = pa.attribute("type");
        QString paramName = na.isNull() ? pa.attribute("name") : i18n(na.text().toUtf8().data());
        QString comment;
        if (!commentElem.isNull())
            comment = i18n(commentElem.text().toUtf8().data());
        QWidget * toFillin = new QWidget(m_baseWidget);
        QString value = pa.attribute("value").isNull() ?
                        pa.attribute("default") : pa.attribute("value");


        /** See effects/README for info on the different types */

        if (type == "double" || type == "constant") {
            double min;
            double max;
            if (pa.attribute("min").contains('%'))
                min = ProfilesDialog::getStringEval(m_profile, pa.attribute("min"), m_frameSize);
            else
                min = pa.attribute("min").toDouble();
            if (pa.attribute("max").contains('%'))
                max = ProfilesDialog::getStringEval(m_profile, pa.attribute("max"), m_frameSize);
            else
                max = pa.attribute("max").toDouble();

            DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, value.toDouble(), min, max,
                    pa.attribute("default").toDouble(), comment, -1, pa.attribute("suffix"), pa.attribute("decimals").toInt(), this);
            m_vbox->addWidget(doubleparam);
            m_valueItems[paramName] = doubleparam;
            connect(doubleparam, SIGNAL(valueChanged(double)), this, SLOT(collectAllParameters()));
            connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
        } else if (type == "list") {
            Listval *lsval = new Listval;
            lsval->setupUi(toFillin);
            QStringList listitems = pa.attribute("paramlist").split(';');
            QDomElement list = pa.firstChildElement("paramlistdisplay");
            QStringList listitemsdisplay;
            if (!list.isNull()) listitemsdisplay = i18n(list.text().toUtf8().data()).split(',');
            else listitemsdisplay = i18n(pa.attribute("paramlistdisplay").toUtf8().data()).split(',');
            if (listitemsdisplay.count() != listitems.count())
                listitemsdisplay = listitems;
            lsval->list->setIconSize(QSize(30, 30));
            for (int i = 0; i < listitems.count(); i++) {
                lsval->list->addItem(listitemsdisplay.at(i), listitems.at(i));
                QString entry = listitems.at(i);
                if (!entry.isEmpty() && (entry.endsWith(".png") || entry.endsWith(".pgm"))) {
                    if (!EffectStackEdit::iconCache.contains(entry)) {
                        QImage pix(entry);
                        EffectStackEdit::iconCache[entry] = pix.scaled(30, 30);
                    }
                    lsval->list->setItemIcon(i, QPixmap::fromImage(iconCache[entry]));
                }
            }
            if (!value.isEmpty()) lsval->list->setCurrentIndex(listitems.indexOf(value));
            lsval->name->setText(paramName);
            lsval->labelComment->setText(comment);
            lsval->widgetComment->setHidden(true);
            m_valueItems[paramName] = lsval;
            connect(lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT(collectAllParameters()));
            if (!comment.isEmpty())
                connect(this, SIGNAL(showComments(bool)), lsval->widgetComment, SLOT(setVisible(bool)));
            m_uiItems.append(lsval);
        } else if (type == "bool") {
            Boolval *bval = new Boolval;
            bval->setupUi(toFillin);
            bval->checkBox->setCheckState(value == "0" ? Qt::Unchecked : Qt::Checked);
            bval->name->setText(paramName);
            bval->labelComment->setText(comment);
            bval->widgetComment->setHidden(true);
            m_valueItems[paramName] = bval;
            connect(bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT(collectAllParameters()));
            if (!comment.isEmpty())
                connect(this, SIGNAL(showComments(bool)), bval->widgetComment, SLOT(setVisible(bool)));
            m_uiItems.append(bval);
        } else if (type == "complex") {
            ComplexParameter *pl = new ComplexParameter;
            pl->setupParam(d, pa.attribute("name"), 0, 100);
            m_vbox->addWidget(pl);
            m_valueItems[paramName+"complex"] = pl;
            connect(pl, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
        } else if (type == "geometry") {
            if (KdenliveSettings::on_monitor_effects()) {
                m_geometryWidget = new GeometryWidget(m_monitor, m_timecode, isEffect ? 0 : qMax(0, (int)info.startPos.frames(KdenliveSettings::project_fps())), isEffect, m_params.hasAttribute("showrotation"), this);
                m_geometryWidget->setFrameSize(m_frameSize);
                m_geometryWidget->slotShowScene(!disable);
                // connect this before setupParam to make sure the monitor scene shows up at startup
                connect(m_geometryWidget, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
                connect(m_geometryWidget, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                if (minFrame == maxFrame)
                    m_geometryWidget->setupParam(pa, m_in, m_out);
                else
                    m_geometryWidget->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(m_geometryWidget);
                m_valueItems[paramName+"geometry"] = m_geometryWidget;
                connect(m_geometryWidget, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(syncEffectsPos(int)), m_geometryWidget, SLOT(slotSyncPosition(int)));
                connect(this, SIGNAL(effectStateChanged(bool)), m_geometryWidget, SLOT(slotShowScene(bool)));
            } else {
                Geometryval *geo = new Geometryval(m_profile, m_timecode, m_frameSize, isEffect ? 0 : qMax(0, (int)info.startPos.frames(KdenliveSettings::project_fps())));
                if (minFrame == maxFrame)
                    geo->setupParam(pa, m_in, m_out);
                else
                    geo->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"geometry"] = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(syncEffectsPos(int)), geo, SLOT(slotSyncPosition(int)));
            }
        } else if (type == "addedgeometry") {
            // this is a parameter that should be linked to the geometry widget, for example rotation, shear, ...
            if (m_geometryWidget) m_geometryWidget->addParameter(pa);
        } else if (type == "keyframe" || type == "simplekeyframe") {
            // keyframe editor widget
            if (m_keyframeEditor == NULL) {
                KeyframeEdit *geo;
                if (pa.attribute("widget") == "corners") {
                    // we want a corners-keyframe-widget
                    CornersWidget *corners = new CornersWidget(m_monitor, pa, m_in, m_out, m_timecode, e.attribute("active_keyframe", "-1").toInt(), this);
                    corners->slotShowScene(!disable);
                    connect(corners, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
                    connect(this, SIGNAL(effectStateChanged(bool)), corners, SLOT(slotShowScene(bool)));
                    connect(this, SIGNAL(syncEffectsPos(int)), corners, SLOT(slotSyncPosition(int)));
                    geo = static_cast<KeyframeEdit *>(corners);
                } else {
                    geo = new KeyframeEdit(pa, m_in, m_out, m_timecode, e.attribute("active_keyframe", "-1").toInt());
                }
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"keyframe"] = geo;
                m_keyframeEditor = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
                connect(this, SIGNAL(showComments(bool)), geo, SIGNAL(showComments(bool)));
            } else {
                // we already have a keyframe editor, so just add another column for the new param
                m_keyframeEditor->addParameter(pa);
            }
        } else if (type == "color") {
            if (value.startsWith('#'))
                value = value.replace('#', "0x");
            bool ok;
            ChooseColorWidget *choosecolor = new ChooseColorWidget(paramName, QColor(value.toUInt(&ok, 16)), this);
            m_vbox->addWidget(choosecolor);
            m_valueItems[paramName] = choosecolor;
            connect(choosecolor, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
            connect(choosecolor, SIGNAL(modified()) , this, SLOT(collectAllParameters()));
        } else if (type == "position") {
            int pos = value.toInt();
            if (d.attribute("id") == "fadein" || d.attribute("id") == "fade_from_black") {
                pos = pos - m_in;
            } else if (d.attribute("id") == "fadeout" || d.attribute("id") == "fade_to_black") {
                // fadeout position starts from clip end
                pos = m_out - pos;
            }
            PositionEdit *posedit = new PositionEdit(paramName, pos, 0, m_out - m_in, m_timecode);
            m_vbox->addWidget(posedit);
            m_valueItems[paramName+"position"] = posedit;
            connect(posedit, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
        } else if (type == "curve") {
            KisCurveWidget *curve = new KisCurveWidget(this);
            curve->setMaxPoints(pa.attribute("max").toInt());
            QList<QPointF> points;
            int number = EffectsList::parameter(e, pa.attribute("number")).toInt();
            QString inName = pa.attribute("inpoints");
            QString outName = pa.attribute("outpoints");
            int start = pa.attribute("min").toInt();
            for (int j = start; j <= number; j++) {
                QString in = inName;
                in.replace("%i", QString::number(j));
                QString out = outName;
                out.replace("%i", QString::number(j));
                points << QPointF(EffectsList::parameter(e, in).toDouble(), EffectsList::parameter(e, out).toDouble());
            }
            if (!points.isEmpty())
                curve->setCurve(KisCubicCurve(points));
            QSpinBox *spinin = new QSpinBox();
            spinin->setRange(0, 1000);
            QSpinBox *spinout = new QSpinBox();
            spinout->setRange(0, 1000);
            curve->setupInOutControls(spinin, spinout, 0, 1000);
            m_vbox->addWidget(curve);
            m_vbox->addWidget(spinin);
            m_vbox->addWidget(spinout);

            connect(curve, SIGNAL(modified()), this, SLOT(collectAllParameters()));
            m_valueItems[paramName] = curve;

            QString depends = pa.attribute("depends");
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(e, depends));
        } else if (type == "bezier_spline") {
            BezierSplineWidget *widget = new BezierSplineWidget(value, this);
            stretch = false;
            m_vbox->addWidget(widget);
            m_valueItems[paramName] = widget;
            connect(widget, SIGNAL(modified()), this, SLOT(collectAllParameters()));
            QString depends = pa.attribute("depends");
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(e, depends));
#ifdef QJSON
        } else if (type == "roto-spline") {
            RotoWidget *roto = new RotoWidget(value, m_monitor, info, m_timecode, this);
            roto->slotShowScene(!disable);
            connect(roto, SIGNAL(valueChanged()), this, SLOT(collectAllParameters()));
            connect(roto, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
            connect(roto, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
            connect(this, SIGNAL(syncEffectsPos(int)), roto, SLOT(slotSyncPosition(int)));
            connect(this, SIGNAL(effectStateChanged(bool)), roto, SLOT(slotShowScene(bool)));
            m_vbox->addWidget(roto);
            m_valueItems[paramName] = roto;
#endif
        } else if (type == "wipe") {
            Wipeval *wpval = new Wipeval;
            wpval->setupUi(toFillin);
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
            connect(wpval->end_up, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->end_down, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->end_left, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->end_right, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->end_center, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_up, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_down, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_left, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_right, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_center, SIGNAL(clicked()), this, SLOT(collectAllParameters()));
            connect(wpval->start_transp, SIGNAL(valueChanged(int)), this, SLOT(collectAllParameters()));
            connect(wpval->end_transp, SIGNAL(valueChanged(int)), this, SLOT(collectAllParameters()));
            //wpval->title->setTitle(na.toElement().text());
            m_uiItems.append(wpval);
        } else if (type == "url") {
            Urlval *cval = new Urlval;
            cval->setupUi(toFillin);
            cval->label->setText(paramName);
            cval->urlwidget->fileDialog()->setFilter(ProjectList::getExtensions());
            m_valueItems[paramName] = cval;
            cval->urlwidget->setUrl(KUrl(value));
            connect(cval->urlwidget, SIGNAL(returnPressed()) , this, SLOT(collectAllParameters()));
            connect(cval->urlwidget, SIGNAL(urlSelected(const KUrl&)) , this, SLOT(collectAllParameters()));
            m_uiItems.append(cval);
        } else {
            delete toFillin;
            toFillin = NULL;
        }

        if (toFillin)
            m_vbox->addWidget(toFillin);
    }

    if (stretch)
        m_vbox->addStretch();

    if (m_keyframeEditor)
        m_keyframeEditor->checkVisibleParam();
    
    // Make sure all doubleparam spinboxes have the same width, looks much better
    QList<DoubleParameterWidget *> allWidgets = findChildren<DoubleParameterWidget *>();
    int minSize = 0;
    for (int i = 0; i < allWidgets.count(); i++) {
        if (minSize < allWidgets.at(i)->spinSize()) minSize = allWidgets.at(i)->spinSize();
    }
    for (int i = 0; i < allWidgets.count(); i++) {
        allWidgets.at(i)->setSpinSize(minSize);
    }
}

wipeInfo EffectStackEdit::getWipeInfo(QString value)
{
    wipeInfo info;
    // Convert old geometry values that used a comma as separator
    if (value.contains(',')) value.replace(',','/');
    QString start = value.section(';', 0, 0);
    QString end = value.section(';', 1, 1).section('=', 1, 1);
    if (start.startsWith("-100%/0"))
        info.start = LEFT;
    else if (start.startsWith("100%/0"))
        info.start = RIGHT;
    else if (start.startsWith("0%/100%"))
        info.start = DOWN;
    else if (start.startsWith("0%/-100%"))
        info.start = UP;
    else
        info.start = CENTER;

    if (start.count(':') == 2)
        info.startTransparency = start.section(':', -1).toInt();
    else
        info.startTransparency = 100;

    if (end.startsWith("-100%/0"))
        info.end = LEFT;
    else if (end.startsWith("100%/0"))
        info.end = RIGHT;
    else if (end.startsWith("0%/100%"))
        info.end = DOWN;
    else if (end.startsWith("0%/-100%"))
        info.end = UP;
    else
        info.end = CENTER;

    if (end.count(':') == 2)
        info.endTransparency = end.section(':', -1).toInt();
    else
        info.endTransparency = 100;

    return info;
}

QString EffectStackEdit::getWipeString(wipeInfo info)
{

    QString start;
    QString end;
    switch (info.start) {
    case LEFT:
        start = "-100%/0%:100%x100%";
        break;
    case RIGHT:
        start = "100%/0%:100%x100%";
        break;
    case DOWN:
        start = "0%/100%:100%x100%";
        break;
    case UP:
        start = "0%/-100%:100%x100%";
        break;
    default:
        start = "0%/0%:100%x100%";
        break;
    }
    start.append(':' + QString::number(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = "-100%/0%:100%x100%";
        break;
    case RIGHT:
        end = "100%/0%:100%x100%";
        break;
    case DOWN:
        end = "0%/100%:100%x100%";
        break;
    case UP:
        end = "0%/-100%:100%x100%";
        break;
    default:
        end = "0%/0%:100%x100%";
        break;
    }
    end.append(':' + QString::number(info.endTransparency));
    return QString(start + ";-1=" + end);
}

void EffectStackEdit::collectAllParameters()
{
    if (m_valueItems.isEmpty() || m_params.isNull()) return;
    QLocale locale;
    const QDomElement oldparam = m_params.cloneNode().toElement();
    QDomElement newparam = oldparam.cloneNode().toElement();
    QDomNodeList namenode = newparam.elementsByTagName("parameter");

    for (int i = 0; i < namenode.count() ; i++) {
        QDomNode pa = namenode.item(i);
        QDomElement na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = na.isNull() ? pa.attributes().namedItem("name").nodeValue() : i18n(na.text().toUtf8().data());
        if (type == "complex")
            paramName.append("complex");
        else if (type == "position")
            paramName.append("position");
        else if (type == "geometry")
            paramName.append("geometry");
        else if (type == "keyframe")
            paramName.append("keyframe");
        if (type != "simplekeyframe" && type != "fixed" && type != "addedgeometry" && !m_valueItems.contains(paramName)) {
            kDebug() << "// Param: " << paramName << " NOT FOUND";
            continue;
        }

        QString setValue;
        if (type == "double" || type == "constant") {
            DoubleParameterWidget *doubleparam = (DoubleParameterWidget*)m_valueItems.value(paramName);
            setValue = locale.toString(doubleparam->getValue());
        } else if (type == "list") {
            KComboBox *box = ((Listval*)m_valueItems.value(paramName))->list;
            setValue = box->itemData(box->currentIndex()).toString();
        } else if (type == "bool") {
            QCheckBox *box = ((Boolval*)m_valueItems.value(paramName))->checkBox;
            setValue = box->checkState() == Qt::Checked ? "1" : "0" ;
        } else if (type == "color") {
            ChooseColorWidget *choosecolor = ((ChooseColorWidget*)m_valueItems.value(paramName));
            setValue = choosecolor->getColor().name();
        } else if (type == "complex") {
            ComplexParameter *complex = ((ComplexParameter*)m_valueItems.value(paramName));
            namenode.item(i) = complex->getParamDesc();
        } else if (type == "geometry") {
            if (KdenliveSettings::on_monitor_effects()) {
                if (m_geometryWidget) namenode.item(i).toElement().setAttribute("value", m_geometryWidget->getValue());
            } else {
                Geometryval *geom = ((Geometryval*)m_valueItems.value(paramName));
                namenode.item(i).toElement().setAttribute("value", geom->getValue());
            }
        } else if (type == "addedgeometry") {
            namenode.item(i).toElement().setAttribute("value", m_geometryWidget->getExtraValue(namenode.item(i).toElement().attribute("name")));
        } else if (type == "position") {
            PositionEdit *pedit = ((PositionEdit*)m_valueItems.value(paramName));
            int pos = pedit->getPosition();
            setValue = QString::number(pos);
            if (newparam.attribute("id") == "fadein" || newparam.attribute("id") == "fade_from_black") {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                EffectsList::setParameter(newparam, "in", QString::number(m_in));
                EffectsList::setParameter(newparam, "out", QString::number(m_in + pos));
                setValue.clear();
            } else if (newparam.attribute("id") == "fadeout" || newparam.attribute("id") == "fade_to_black") {
                // Make sure duration is not longer than clip
                /*if (pos > m_out) {
                    pos = m_out;
                    pedit->setPosition(pos);
                }*/
                EffectsList::setParameter(newparam, "in", QString::number(m_out - pos));
                EffectsList::setParameter(newparam, "out", QString::number(m_out));
                setValue.clear();
            }
        } else if (type == "curve") {
            KisCurveWidget *curve = ((KisCurveWidget*)m_valueItems.value(paramName));
            QList<QPointF> points = curve->curve().points();
            QString number = pa.attributes().namedItem("number").nodeValue();
            QString inName = pa.attributes().namedItem("inpoints").nodeValue();
            QString outName = pa.attributes().namedItem("outpoints").nodeValue();
            int off = pa.attributes().namedItem("min").nodeValue().toInt();
            int end = pa.attributes().namedItem("max").nodeValue().toInt();
            EffectsList::setParameter(newparam, number, QString::number(points.count()));
            for (int j = 0; (j < points.count() && j + off <= end); j++) {
                QString in = inName;
                in.replace("%i", QString::number(j + off));
                QString out = outName;
                out.replace("%i", QString::number(j + off));
                EffectsList::setParameter(newparam, in, locale.toString(points.at(j).x()));
                EffectsList::setParameter(newparam, out, locale.toString(points.at(j).y()));
            }
            QString depends = pa.attributes().namedItem("depends").nodeValue();
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(newparam, depends));
        } else if (type == "bezier_spline") {
            BezierSplineWidget *widget = (BezierSplineWidget*)m_valueItems.value(paramName);
            setValue = widget->spline();
            QString depends = pa.attributes().namedItem("depends").nodeValue();
            if (!depends.isEmpty())
                meetDependency(paramName, type, EffectsList::parameter(newparam, depends));
#ifdef QJSON
        } else if (type == "roto-spline") {
            RotoWidget *widget = static_cast<RotoWidget *>(m_valueItems.value(paramName));
            setValue = widget->getSpline();
#endif
        } else if (type == "wipe") {
            Wipeval *wp = (Wipeval*)m_valueItems.value(paramName);
            wipeInfo info;
            if (wp->start_left->isChecked())
                info.start = LEFT;
            else if (wp->start_right->isChecked())
                info.start = RIGHT;
            else if (wp->start_up->isChecked())
                info.start = UP;
            else if (wp->start_down->isChecked())
                info.start = DOWN;
            else if (wp->start_center->isChecked())
                info.start = CENTER;
            else
                info.start = LEFT;
            info.startTransparency = wp->start_transp->value();

            if (wp->end_left->isChecked())
                info.end = LEFT;
            else if (wp->end_right->isChecked())
                info.end = RIGHT;
            else if (wp->end_up->isChecked())
                info.end = UP;
            else if (wp->end_down->isChecked())
                info.end = DOWN;
            else if (wp->end_center->isChecked())
                info.end = CENTER;
            else
                info.end = RIGHT;
            info.endTransparency = wp->end_transp->value();

            setValue = getWipeString(info);
        } else if ((type == "simplekeyframe" || type == "keyframe") && m_keyframeEditor) {
            QDomElement elem = pa.toElement();
            QString realName = i18n(na.toElement().text().toUtf8().data());
            QString val = m_keyframeEditor->getValue(realName);
            elem.setAttribute("keyframes", val);

            if (m_keyframeEditor->isVisibleParam(realName))
                elem.setAttribute("intimeline", "1");
            else if (elem.hasAttribute("intimeline"))
                elem.removeAttribute("intimeline");
        } else if (type == "url") {
            KUrlRequester *req = ((Urlval*)m_valueItems.value(paramName))->urlwidget;
            setValue = req->url().path();
        }

        if (!setValue.isNull())
            pa.attributes().namedItem("value").setNodeValue(setValue);

    }
    emit parameterChanged(oldparam, newparam);
}

void EffectStackEdit::clearAllItems()
{
    blockSignals(true);
    m_valueItems.clear();
    m_uiItems.clear();
    /*while (!m_items.isEmpty()) {
        QWidget *die = m_items.takeFirst();
        die->disconnect();
        delete die;
    }*/
    //qDeleteAll(m_uiItems);
    QLayoutItem *child;
    while ((child = m_vbox->takeAt(0)) != 0) {
        QWidget *wid = child->widget();
        delete child;
        if (wid) delete wid;
    }
    m_keyframeEditor = NULL;
    m_geometryWidget = NULL;
    blockSignals(false);
}

void EffectStackEdit::slotSyncEffectsPos(int pos)
{
    emit syncEffectsPos(pos);
}
