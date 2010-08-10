/***************************************************************************
                          effecstackview.cpp  -  description
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
#include "ui_constval_ui.h"
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

#include <KDebug>
#include <KLocale>
#include <KFileDialog>

#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>


class Boolval: public QWidget, public Ui::Boolval_UI
{
};

class Constval: public QWidget, public Ui::Constval_UI
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
        m_monitor(monitor)
{
    m_baseWidget = new QWidget(this);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameStyle(QFrame::NoFrame);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));

    setWidget(m_baseWidget);
    setWidgetResizable(true);
    m_vbox = new QVBoxLayout(m_baseWidget);
    m_vbox->setContentsMargins(0, 0, 0, 0);
    m_vbox->setSpacing(0);
    //wid->show();
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
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = i18n(na.toElement().text().toUtf8().data());

        if (type == "geometry" && !KdenliveSettings::on_monitor_effects()) {
            Geometryval *geom = ((Geometryval*)m_valueItems[paramName+"geometry"]);
            geom->setFrameSize(m_frameSize);
            break;
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
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = i18n(na.toElement().text().toUtf8().data());

        if (type == "geometry" && !KdenliveSettings::on_monitor_effects()) {
            Geometryval *geom = ((Geometryval*)m_valueItems[paramName+"geometry"]);
            geom->updateTimecodeFormat();
            break;
        }
        if (type == "position") {
            PositionEdit *posi = ((PositionEdit*)m_valueItems[paramName+"position"]);
            posi->updateTimecodeFormat();
            break;
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
}

void EffectStackEdit::transferParamDesc(const QDomElement d, int pos, int in, int out, bool isEffect)
{
    clearAllItems();
    if (m_keyframeEditor) delete m_keyframeEditor;
    m_keyframeEditor = NULL;
    m_params = d;
    m_in = in;
    m_out = out;
    if (m_params.isNull()) {
        kDebug() << "// EMPTY EFFECT STACK";
        return;
    }

    /*QDomDocument doc;
    doc.appendChild(doc.importNode(m_params, true));
    kDebug() << "IMPORTED TRANS: " << doc.toString();*/

    QDomNodeList namenode = m_params.elementsByTagName("parameter");
    QDomElement e = m_params.toElement();
    const int minFrame = e.attribute("start").toInt();
    const int maxFrame = e.attribute("end").toInt();


    for (int i = 0; i < namenode.count() ; i++) {
        QDomElement pa = namenode.item(i).toElement();
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attribute("type");
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        QWidget * toFillin = new QWidget(m_baseWidget);
        QString value = pa.attribute("value").isNull() ?
                        pa.attribute("default") : pa.attribute("value");

        /** Currently supported parameter types are:
            * constant (=double): a slider with an integer value (use the "factor" attribute to divide the value so that you can get a double
            * list: a combobox containing a list of values to choose
            * bool: a checkbox
            * complex: designed for keyframe parameters, but old and not finished, do not use
            * geometry: a rectangle that can be moved & resized, with possible keyframes, used in composite transition
            * keyframe: a list widget with a list of entries (position and value)
            * color: a color chooser button
            * position: a slider representing the position of a frame in the current clip
            * curve: a single curve representing multiple points
            * wipe: a widget designed for the wipe transition, allowing to choose a position (left, right, top,...)
        */

        if (type == "double" || type == "constant") {
            int min;
            int max;
            if (pa.attribute("min").startsWith('%'))
                min = (int) ProfilesDialog::getStringEval(m_profile, pa.attribute("min"));
            else
                min = pa.attribute("min").toInt();
            if (pa.attribute("max").startsWith('%'))
                max = (int) ProfilesDialog::getStringEval(m_profile, pa.attribute("max"));
            else
                max = pa.attribute("max").toInt();
            createSliderItem(paramName, (int)(value.toDouble() + 0.5) , min, max, pa.attribute("suffix", QString()));
            delete toFillin;
            toFillin = NULL;
        } else if (type == "list") {
            Listval *lsval = new Listval;
            lsval->setupUi(toFillin);
            QStringList listitems = pa.attribute("paramlist").split(',');
            QStringList listitemsdisplay = pa.attribute("paramlistdisplay").split(',');
            if (listitemsdisplay.count() != listitems.count()) listitemsdisplay = listitems;
            //lsval->list->addItems(listitems);
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
            lsval->title->setTitle(paramName);
            m_valueItems[paramName] = lsval;
            connect(lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT(collectAllParameters()));
            m_uiItems.append(lsval);
        } else if (type == "bool") {
            Boolval *bval = new Boolval;
            bval->setupUi(toFillin);
            bval->checkBox->setCheckState(value == "0" ? Qt::Unchecked : Qt::Checked);
            bval->checkBox->setText(paramName);
            m_valueItems[paramName] = bval;
            connect(bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT(collectAllParameters()));
            m_uiItems.append(bval);
        } else if (type == "complex") {
            /*QStringList names=nodeAtts.namedItem("name").nodeValue().split(';');
            QStringList max=nodeAtts.namedItem("max").nodeValue().split(';');
            QStringList min=nodeAtts.namedItem("min").nodeValue().split(';');
            QStringList val=value.split(';');
            kDebug() << "in complex"<<names.size() << " " << max.size() << " " << min.size() << " " << val.size()  ;
            if ( (names.size() == max.size() ) &&
                 (names.size()== min.size()) &&
                 (names.size()== val.size()) )
            {
             for (int i=0;i< names.size();i++){
              createSliderItem(names[i],val[i].toInt(),min[i].toInt(),max[i].toInt());
             };
            }*/
            ComplexParameter *pl = new ComplexParameter;
            pl->setupParam(d, pa.attribute("name"), 0, 100);
            m_vbox->addWidget(pl);
            m_valueItems[paramName+"complex"] = pl;
            connect(pl, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
        } else if (type == "geometry") {
            if (KdenliveSettings::on_monitor_effects()) {
                GeometryWidget *geometry = new GeometryWidget(m_monitor, pos, isEffect, this);
                if (minFrame == maxFrame)
                    geometry->setupParam(pa, m_in, m_out);
                else
                    geometry->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(geometry);
                m_valueItems[paramName+"geometry"] = geometry;
                connect(geometry, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                connect(geometry, SIGNAL(checkMonitorPosition(int)), this, SIGNAL(checkMonitorPosition(int)));
            } else {
                Geometryval *geo = new Geometryval(m_profile, m_timecode, m_frameSize, pos);
                if (minFrame == maxFrame)
                    geo->setupParam(pa, m_in, m_out);
                else
                    geo->setupParam(pa, minFrame, maxFrame);
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"geometry"] = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
            }
        } else if (type == "keyframe" || type == "simplekeyframe") {
            // keyframe editor widget
            kDebug() << "min: " << m_in << ", MAX: " << m_out;
            if (m_keyframeEditor == NULL) {
                KeyframeEdit *geo = new KeyframeEdit(pa, m_in, m_in + m_out, pa.attribute("min").toInt(), pa.attribute("max").toInt(), m_timecode, e.attribute("active_keyframe", "-1").toInt());
                m_vbox->addWidget(geo);
                m_valueItems[paramName+"keyframe"] = geo;
                m_keyframeEditor = geo;
                connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
                connect(geo, SIGNAL(seekToPos(int)), this, SIGNAL(seekTimeline(int)));
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
                pos = m_out - (pos - m_in);
            }
            PositionEdit *posedit = new PositionEdit(paramName, pos, 1, m_out, m_timecode);
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
    m_vbox->addStretch();
}

wipeInfo EffectStackEdit::getWipeInfo(QString value)
{
    wipeInfo info;
    QString start = value.section(';', 0, 0);
    QString end = value.section(';', 1, 1).section('=', 1, 1);

    if (start.startsWith("-100%,0"))
        info.start = LEFT;
    else if (start.startsWith("100%,0"))
        info.start = RIGHT;
    else if (start.startsWith("0%,100%"))
        info.start = DOWN;
    else if (start.startsWith("0%,-100%"))
        info.start = UP;
    else
        info.start = CENTER;

    if (start.count(':') == 2)
        info.startTransparency = start.section(':', -1).toInt();
    else
        info.startTransparency = 100;

    if (end.startsWith("-100%,0"))
        info.end = LEFT;
    else if (end.startsWith("100%,0"))
        info.end = RIGHT;
    else if (end.startsWith("0%,100%"))
        info.end = DOWN;
    else if (end.startsWith("0%,-100%"))
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
        start = "-100%,0%:100%x100%";
        break;
    case RIGHT:
        start = "100%,0%:100%x100%";
        break;
    case DOWN:
        start = "0%,100%:100%x100%";
        break;
    case UP:
        start = "0%,-100%:100%x100%";
        break;
    default:
        start = "0%,0%:100%x100%";
        break;
    }
    start.append(':' + QString::number(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = "-100%,0%:100%x100%";
        break;
    case RIGHT:
        end = "100%,0%:100%x100%";
        break;
    case DOWN:
        end = "0%,100%:100%x100%";
        break;
    case UP:
        end = "0%,-100%:100%x100%";
        break;
    default:
        end = "0%,0%:100%x100%";
        break;
    }
    end.append(':' + QString::number(info.endTransparency));
    return QString(start + ";-1=" + end);
}

void EffectStackEdit::collectAllParameters()
{
    if (m_valueItems.isEmpty() || m_params.isNull()) return;
    const QDomElement oldparam = m_params.cloneNode().toElement();
    QDomElement newparam = oldparam.cloneNode().toElement();
    QDomNodeList namenode = newparam.elementsByTagName("parameter");

    for (int i = 0; i < namenode.count() ; i++) {
        QDomNode pa = namenode.item(i);
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (type == "complex")
            paramName.append("complex");
        else if (type == "position")
            paramName.append("position");
        else if (type == "geometry")
            paramName.append("geometry");
        else if (type == "keyframe")
            paramName.append("keyframe");
        if (type != "simplekeyframe" && !m_valueItems.contains(paramName)) {
            kDebug() << "// Param: " << paramName << " NOT FOUND";
            continue;
        }

        QString setValue;
        if (type == "double" || type == "constant") {
            QSlider* slider = ((Constval*)m_valueItems.value(paramName))->horizontalSlider;
            setValue = QString::number(slider->value());
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
                GeometryWidget *geometry = ((GeometryWidget*)m_valueItems.value(paramName));
                namenode.item(i).toElement().setAttribute("value", geometry->getValue());
            } else {
                Geometryval *geom = ((Geometryval*)m_valueItems.value(paramName));
                namenode.item(i).toElement().setAttribute("value", geom->getValue());
            }
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
                EffectsList::setParameter(newparam, "in", QString::number(m_out + m_in - pos));
                EffectsList::setParameter(newparam, "out", QString::number(m_out + m_in));
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
                EffectsList::setParameter(newparam, in, QString::number(points.at(j).x()));
                EffectsList::setParameter(newparam, out, QString::number(points.at(j).y()));
            }
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
            QString realName = i18n(na.toElement().text().toUtf8().data());
            QString val = m_keyframeEditor->getValue(realName);
            kDebug() << "SET VALUE: " << val;
            namenode.item(i).toElement().setAttribute("keyframes", val);
        } else if (type == "url") {
            KUrlRequester *req = ((Urlval*)m_valueItems.value(paramName))->urlwidget;
            setValue = req->url().path();
        }

        if (!setValue.isNull())
            pa.attributes().namedItem("value").setNodeValue(setValue);

    }
    emit parameterChanged(oldparam, newparam);
}

void EffectStackEdit::createSliderItem(const QString& name, int val , int min, int max, const QString suffix)
{
    QWidget* toFillin = new QWidget(m_baseWidget);
    Constval *ctval = new Constval;
    ctval->setupUi(toFillin);
    ctval->horizontalSlider->setMinimum(min);
    ctval->horizontalSlider->setMaximum(max);
    if (!suffix.isEmpty()) ctval->spinBox->setSuffix(suffix);
    ctval->spinBox->setMinimum(min);
    ctval->spinBox->setMaximum(max);
    ctval->horizontalSlider->setPageStep((int)(max - min) / 10);
    ctval->horizontalSlider->setValue(val);
    ctval->label->setText(name);
    m_valueItems[name] = ctval;
    m_uiItems.append(ctval);
    connect(ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT(collectAllParameters()));
    m_vbox->addWidget(toFillin);
}

void EffectStackEdit::slotSliderMoved(int)
{
    collectAllParameters();
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
    blockSignals(false);
}
