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
#include "ui_colorval_ui.h"
#include "ui_positionval_ui.h"
#include "ui_wipeval_ui.h"
#include "ui_keyframeeditor_ui.h"
#include "complexparameter.h"
#include "geometryval.h"
#include "keyframeedit.h"
#include "effectslist.h"
#include "kdenlivesettings.h"
#include "profilesdialog.h"

#include <KDebug>
#include <KLocale>

#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>


class Boolval: public EffectStackEdit::UiItem, public Ui::Boolval_UI
{
};

class Colorval: public EffectStackEdit::UiItem, public Ui::Colorval_UI
{
};

class Constval: public EffectStackEdit::UiItem, public Ui::Constval_UI
{
};

class Listval: public EffectStackEdit::UiItem, public Ui::Listval_UI
{
};

class Positionval: public EffectStackEdit::UiItem, public Ui::Positionval_UI
{
};

class Wipeval: public EffectStackEdit::UiItem, public Ui::Wipeval_UI
{
};


QMap<QString, QImage> EffectStackEdit::iconCache;

EffectStackEdit::EffectStackEdit(QWidget *parent) :
        QWidget(parent),
        m_in(0),
        m_out(0),
        m_frameSize(QPoint())
{
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    QVBoxLayout *vbox1 = new QVBoxLayout(parent);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);

    QScrollArea *area = new QScrollArea;
    QWidget *wid = new QWidget(parent);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    area->setFrameStyle(QFrame::NoFrame);
    wid->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    area->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));

    vbox1->addWidget(area);
    area->setWidget(wid);
    area->setWidgetResizable(true);
    m_vbox = new QVBoxLayout(wid);
    m_vbox->setContentsMargins(0, 0, 0, 0);
    m_vbox->setSpacing(0);
    wid->show();

}

EffectStackEdit::~EffectStackEdit()
{
    iconCache.clear();
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

        if (type == "geometry") {
            Geometryval *geom = ((Geometryval*)m_valueItems[paramName+"geometry"]);
            geom->setFrameSize(m_frameSize);
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

void EffectStackEdit::transferParamDesc(const QDomElement& d, int in, int out)
{
    kDebug() << "in";
    m_params = d;
    m_in = in;
    m_out = out;
    clearAllItems();
    if (m_params.isNull()) return;

    QDomDocument doc;
    doc.appendChild(doc.importNode(m_params, true));
    //kDebug() << "IMPORTED TRANS: " << doc.toString();
    QDomNodeList namenode = m_params.elementsByTagName("parameter");
    QDomElement e = m_params.toElement();
    const int minFrame = e.attribute("start").toInt();
    const int maxFrame = e.attribute("end").toInt();


    for (int i = 0; i < namenode.count() ; i++) {
        kDebug() << "in form";
        QDomElement pa = namenode.item(i).toElement();
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attribute("type");
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        QWidget * toFillin = new QWidget;
        QString value = pa.attribute("value").isNull() ?
                        pa.attribute("default") : pa.attribute("value");

        if (type == "geometry") {
            /*pa.setAttribute("namedesc", "X;Y;Width;Height;Transparency");
            pa.setAttribute("format", "%d%,%d%:%d%x%d%:%d");
            pa.setAttribute("min", "-500;-500;0;0;0");
            pa.setAttribute("max", "500;500;200;200;100");*/
        } else if (type == "complex") {
            //pa.setAttribute("namedesc",pa.attribute("name"));

        }


        //TODO constant, list, bool, complex , color, geometry, position
        if (type == "double" || type == "constant") {
            int min;
            int max;
            if (pa.attribute("min").startsWith('%')) {
                min = (int) ProfilesDialog::getStringEval(m_profile, pa.attribute("min"));
            } else min = pa.attribute("min").toInt();
            if (pa.attribute("max").startsWith('%')) {
                max = (int) ProfilesDialog::getStringEval(m_profile, pa.attribute("max"));
            } else max = pa.attribute("max").toInt();
            createSliderItem(paramName, (int)(value.toDouble() + 0.5) , min, max);
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
            m_items.append(pl);
        } else if (type == "geometry") {
            Geometryval *geo = new Geometryval(m_profile, m_frameSize);
            geo->setupParam(pa, minFrame, maxFrame);
            m_vbox->addWidget(geo);
            m_valueItems[paramName+"geometry"] = geo;
            connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
            connect(geo, SIGNAL(seekToPos(int)), this, SLOT(slotSeekToPos(int)));
            m_items.append(geo);
        } else if (type == "keyframe") {
            // keyframe editor widget
            kDebug() << "min: " << m_in << ", MAX: " << m_out;
            KeyframeEdit *geo = new KeyframeEdit(pa, m_out - m_in, m_timecode);
            //geo->setupParam(100, pa.attribute("min").toInt(), pa.attribute("max").toInt(), pa.attribute("keyframes"));
            //connect(geo, SIGNAL(seekToPos(int)), this, SLOT(slotSeekToPos(int)));
            //geo->setupParam(pa, minFrame, maxFrame);
            m_vbox->addWidget(geo);
            m_valueItems[paramName+"keyframe"] = geo;
            connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
            m_items.append(geo);
        } else if (type == "color") {
            Colorval *cval = new Colorval;
            cval->setupUi(toFillin);
            bool ok;
            if (value.startsWith('#')) value = value.replace('#', "0x");
            cval->kcolorbutton->setColor(value.toUInt(&ok, 16));
            //kDebug() << "color: " << value << ", " << value.toUInt(&ok, 16);
            cval->label->setText(paramName);
            m_valueItems[paramName] = cval;
            connect(cval->kcolorbutton, SIGNAL(clicked()) , this, SLOT(collectAllParameters()));
            m_uiItems.append(cval);
        } else if (type == "position") {
            Positionval *pval = new Positionval;
            pval->setupUi(toFillin);
            int pos = value.toInt();
            if (d.attribute("id") == "fadein" || d.attribute("id") == "fade_from_black") {
                pos = pos - m_in;
            } else if (d.attribute("id") == "fadeout" || d.attribute("id") == "fade_to_black") {
                // fadeout position starts from clip end
                pos = m_out - (pos - m_in);
            }
            pval->krestrictedline->setText(m_timecode.getTimecodeFromFrames(pos));
            pval->label->setText(paramName);
            m_valueItems[paramName + "position"] = pval;
            connect(pval->krestrictedline, SIGNAL(editingFinished()), this, SLOT(collectAllParameters()));
            m_uiItems.append(pval);
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
        } else {
            delete toFillin;
            toFillin = NULL;
        }

        if (toFillin) {
            m_items.append(toFillin);
            m_vbox->addWidget(toFillin);
        }
    }
    m_vbox->addStretch();
}

void EffectStackEdit::slotSeekToPos(int pos)
{
    emit seekTimeline(m_in + pos);
}

wipeInfo EffectStackEdit::getWipeInfo(QString value)
{
    wipeInfo info;
    QString start = value.section(';', 0, 0);
    QString end = value.section(';', 1, 1).section('=', 1, 1);
    if (start.startsWith("-100%,0")) info.start = LEFT;
    else if (start.startsWith("100%,0")) info.start = RIGHT;
    else if (start.startsWith("0%,100%")) info.start = DOWN;
    else if (start.startsWith("0%,-100%")) info.start = UP;
    else info.start = CENTER;
    if (start.count(':') == 2) info.startTransparency = start.section(':', -1).toInt();
    else info.startTransparency = 100;

    if (end.startsWith("-100%,0")) info.end = LEFT;
    else if (end.startsWith("100%,0")) info.end = RIGHT;
    else if (end.startsWith("0%,100%")) info.end = DOWN;
    else if (end.startsWith("0%,-100%")) info.end = UP;
    else info.end = CENTER;
    if (end.count(':') == 2) info.endTransparency = end.section(':', -1).toInt();
    else info.endTransparency = 100;
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
    if (m_valueItems.isEmpty()) return;
    QDomElement oldparam = m_params.cloneNode().toElement();
    QDomNodeList namenode = m_params.elementsByTagName("parameter");

    for (int i = 0; i < namenode.count() ; i++) {
        QDomNode pa = namenode.item(i);
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString paramName = i18n(na.toElement().text().toUtf8().data());
        if (type == "complex") paramName.append("complex");
        else if (type == "position") paramName.append("position");
        else if (type == "geometry") paramName.append("geometry");
        else if (type == "keyframe") paramName.append("keyframe");
        if (!m_valueItems.contains(paramName)) {
            kDebug() << "// Param: " << paramName << " NOT FOUND";
            return;
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
            KColorButton *color = ((Colorval*)m_valueItems.value(paramName))->kcolorbutton;
            setValue = color->color().name();
        } else if (type == "complex") {
            ComplexParameter *complex = ((ComplexParameter*)m_valueItems.value(paramName));
            namenode.item(i) = complex->getParamDesc();
        } else if (type == "geometry") {
            Geometryval *geom = ((Geometryval*)m_valueItems.value(paramName));
            namenode.item(i) = geom->getParamDesc();
        } else if (type == "position") {
            KRestrictedLine *line = ((Positionval*)m_valueItems.value(paramName))->krestrictedline;
            int pos = m_timecode.getFrameCount(line->text(), KdenliveSettings::project_fps());
            setValue = QString::number(pos);
            if (m_params.attribute("id") == "fadein" || m_params.attribute("id") == "fade_from_black") {
                // Make sure duration is not longer than clip
                if (pos > m_out) {
                    pos = m_out;
                    line->setText(m_timecode.getTimecodeFromFrames(pos));
                }
                EffectsList::setParameter(m_params, "in", QString::number(m_in));
                EffectsList::setParameter(m_params, "out", QString::number(m_in + pos));
                setValue.clear();
            } else if (m_params.attribute("id") == "fadeout" || m_params.attribute("id") == "fade_to_black") {
                // Make sure duration is not longer than clip
                if (pos > m_out) {
                    pos = m_out;
                    line->setText(m_timecode.getTimecodeFromFrames(pos));
                }
                EffectsList::setParameter(m_params, "in", QString::number(m_out + m_in - pos));
                EffectsList::setParameter(m_params, "out", QString::number(m_out + m_in));
                setValue.clear();
            }
        } else if (type == "wipe") {
            Wipeval *wp = (Wipeval*)m_valueItems.value(paramName);
            wipeInfo info;
            if (wp->start_left->isChecked()) info.start = LEFT;
            else if (wp->start_right->isChecked()) info.start = RIGHT;
            else if (wp->start_up->isChecked()) info.start = UP;
            else if (wp->start_down->isChecked()) info.start = DOWN;
            else if (wp->start_center->isChecked()) info.start = CENTER;
            else info.start = LEFT;
            info.startTransparency = wp->start_transp->value();
            if (wp->end_left->isChecked()) info.end = LEFT;
            else if (wp->end_right->isChecked()) info.end = RIGHT;
            else if (wp->end_up->isChecked()) info.end = UP;
            else if (wp->end_down->isChecked()) info.end = DOWN;
            else if (wp->end_center->isChecked()) info.end = CENTER;
            else info.end = RIGHT;
            info.endTransparency = wp->end_transp->value();
            setValue = getWipeString(info);
        }

        if (!setValue.isNull()) {
            pa.attributes().namedItem("value").setNodeValue(setValue);
        }
    }
    emit parameterChanged(oldparam, m_params);
}

void EffectStackEdit::createSliderItem(const QString& name, int val , int min, int max)
{
    QWidget* toFillin = new QWidget;
    Constval *ctval = new Constval;
    ctval->setupUi(toFillin);
    ctval->horizontalSlider->setMinimum(min);
    ctval->horizontalSlider->setMaximum(max);
    ctval->spinBox->setMinimum(min);
    ctval->spinBox->setMaximum(max);
    ctval->horizontalSlider->setPageStep((int)(max - min) / 10);
    ctval->horizontalSlider->setValue(val);
    ctval->label->setText(name);
    m_valueItems[name] = ctval;
    m_uiItems.append(ctval);
    connect(ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT(collectAllParameters()));
    m_items.append(toFillin);
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

    while (!m_items.isEmpty()) {
        QWidget * die = m_items.takeFirst();
        die->disconnect();
        delete die;
    }

    qDeleteAll(m_uiItems);
    m_uiItems.clear();
    m_items.clear();
    QLayoutItem *item = m_vbox->itemAt(0);
    while (item) {
        m_vbox->removeItem(item);
        delete item;
        item = m_vbox->itemAt(0);
    }
    blockSignals(false);
}
