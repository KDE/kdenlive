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

#include <KDebug>
#include <KLocale>
#include "effectstackedit.h"
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>

#include "ui_constval_ui.h"
#include "ui_listval_ui.h"
#include "ui_boolval_ui.h"
#include "ui_colorval_ui.h"
#include "ui_wipeval_ui.h"
#include "complexparameter.h"

#include "geometryval.h"

QMap<QString, QImage> EffectStackEdit::iconCache;

EffectStackEdit::EffectStackEdit(QFrame* frame, QWidget *parent): QObject(parent) {
    QScrollArea *area;
    QVBoxLayout *vbox1 = new QVBoxLayout(frame);
    QVBoxLayout *vbox2 = new QVBoxLayout(frame);
    vbox = new QVBoxLayout(frame);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    vbox2->setContentsMargins(0, 0, 0, 0);
    vbox2->setSpacing(0);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    frame->setLayout(vbox1);
    QFont widgetFont = frame->font();
    widgetFont.setPointSize(widgetFont.pointSize() - 2);
    frame->setFont(widgetFont);

    area = new QScrollArea(frame);
    QWidget *wid = new QWidget(area);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    wid->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    //area->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::MinimumExpanding));

    vbox1->addWidget(area);
    wid->setLayout(vbox2);
    vbox2->addLayout(vbox);
    vbox2->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    area->setWidget(wid);
    area->setWidgetResizable(true);
    wid->show();

}

EffectStackEdit::~EffectStackEdit() {
    iconCache.clear();
}

void EffectStackEdit::updateProjectFormat(MltVideoProfile profile) {
    m_profile = profile;
}

void EffectStackEdit::transferParamDesc(const QDomElement& d, int , int) {
    kDebug() << "in";
    params = d;

    clearAllItems();
    if (params.isNull()) return;

    QDomDocument doc;
    doc.appendChild(doc.importNode(params, true));
    kDebug() << "IMPORTED TRANS: " << doc.toString();
    QDomNodeList namenode = params.elementsByTagName("parameter");
    QDomElement e = params.toElement();
    const int minFrame = e.attribute("start").toInt();
    const int maxFrame = e.attribute("end").toInt();


    for (int i = 0;i < namenode.count() ;i++) {
        kDebug() << "in form";
        QDomElement pa = namenode.item(i).toElement();
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attribute("type");
        QString paramName = na.toElement().text();
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
            createSliderItem(paramName, value.toInt(), pa.attribute("min").toInt(), pa.attribute("max").toInt());
            delete toFillin;
            toFillin = NULL;
        } else if (type == "list") {
            Ui::Listval_UI *lsval = new Ui::Listval_UI;
            lsval->setupUi(toFillin);
            QStringList listitems = pa.attribute("paramlist").split(",");
            QStringList listitemsdisplay = pa.attribute("paramlistdisplay").split(",");
            if (listitemsdisplay.count() != listitems.count()) listitemsdisplay = listitems;
            //lsval->list->addItems(listitems);
            for (int i = 0;i < listitems.count();i++) {
                lsval->list->addItem(listitemsdisplay.at(i), listitems.at(i));
            }
            lsval->list->setCurrentIndex(listitems.indexOf(value));
            for (int i = 0;i < lsval->list->count();i++) {
                QString entry = lsval->list->itemData(i).toString();
                if (!entry.isEmpty() && (entry.endsWith(".png") || entry.endsWith(".pgm"))) {
                    if (!EffectStackEdit::iconCache.contains(entry)) {
                        QImage pix(entry);
                        EffectStackEdit::iconCache[entry] = pix.scaled(30, 30);
                    }
                    lsval->list->setIconSize(QSize(30, 30));
                    lsval->list->setItemIcon(i, QPixmap::fromImage(iconCache[entry]));
                }
            }
            connect(lsval->list, SIGNAL(currentIndexChanged(int)) , this, SLOT(collectAllParameters()));
            lsval->title->setTitle(na.toElement().text());
            valueItems[paramName] = lsval;
            uiItems.append(lsval);
        } else if (type == "bool") {
            Ui::Boolval_UI *bval = new Ui::Boolval_UI;
            bval->setupUi(toFillin);
            bval->checkBox->setCheckState(value == "0" ? Qt::Unchecked : Qt::Checked);

            connect(bval->checkBox, SIGNAL(stateChanged(int)) , this, SLOT(collectAllParameters()));
            bval->checkBox->setText(na.toElement().text());
            valueItems[paramName] = bval;
            uiItems.append(bval);
        } else if (type == "complex") {
            /*QStringList names=nodeAtts.namedItem("name").nodeValue().split(";");
            QStringList max=nodeAtts.namedItem("max").nodeValue().split(";");
            QStringList min=nodeAtts.namedItem("min").nodeValue().split(";");
            QStringList val=value.split(";");
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
            connect(pl, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
            pl->setupParam(d, pa.attribute("name"), 0, 100);
            vbox->addWidget(pl);
            valueItems[paramName+"complex"] = pl;
            items.append(pl);
        } else if (type == "geometry") {
            Geometryval *geo = new Geometryval(m_profile);
            connect(geo, SIGNAL(parameterChanged()), this, SLOT(collectAllParameters()));
            geo->setupParam(pa, minFrame, maxFrame);
            vbox->addWidget(geo);
            valueItems[paramName+"geometry"] = geo;
            items.append(geo);
        } else if (type == "color") {
            Ui::Colorval_UI *cval = new Ui::Colorval_UI;
            cval->setupUi(toFillin);
            bool ok;
            cval->kcolorbutton->setColor(value.toUInt(&ok, 16));
            kDebug() << value.toUInt(&ok, 16);

            connect(cval->kcolorbutton, SIGNAL(clicked()) , this, SLOT(collectAllParameters()));
            cval->label->setText(na.toElement().text());
            valueItems[paramName] = cval;
            uiItems.append(cval);
        } else if (type == "wipe") {
            Ui::Wipeval_UI *wpval = new Ui::Wipeval_UI;
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
            valueItems[paramName] = wpval;
            uiItems.append(wpval);
        } else {
            delete toFillin;
            toFillin = NULL;
        }

        if (toFillin) {
            items.append(toFillin);
            vbox->addWidget(toFillin);
        }
    }
}

wipeInfo EffectStackEdit::getWipeInfo(QString value) {
    wipeInfo info;
    QString start = value.section(";", 0, 0);
    QString end = value.section(";", 1, 1).section("=", 1, 1);
    if (start.startsWith("-100%,0")) info.start = LEFT;
    else if (start.startsWith("100%,0")) info.start = RIGHT;
    else if (start.startsWith("0%,100%")) info.start = DOWN;
    else if (start.startsWith("0%,-100%")) info.start = UP;
    else if (start.startsWith("0%,0%")) info.start = CENTER;
    if (start.count(':') == 2) info.startTransparency = start.section(':', -1).toInt();
    else info.startTransparency = 100;

    if (end.startsWith("-100%,0")) info.end = LEFT;
    else if (end.startsWith("100%,0")) info.end = RIGHT;
    else if (end.startsWith("0%,100%")) info.end = DOWN;
    else if (end.startsWith("0%,-100%")) info.end = UP;
    else if (end.startsWith("0%,0%")) info.end = CENTER;
    if (end.count(':') == 2) info.endTransparency = end.section(':', -1).toInt();
    else info.endTransparency = 100;
    return info;
}

QString EffectStackEdit::getWipeString(wipeInfo info) {

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
    start.append(":" + QString::number(info.startTransparency));

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
    end.append(":" + QString::number(info.endTransparency));
    return QString(start + ";-1=" + end);
}

void EffectStackEdit::collectAllParameters() {
    QDomElement oldparam = params.cloneNode().toElement();
    QDomNodeList namenode = params.elementsByTagName("parameter");

    for (int i = 0;i < namenode.count() ;i++) {
        QDomNode pa = namenode.item(i);
        QDomNode na = pa.firstChildElement("name");
        QString type = pa.attributes().namedItem("type").nodeValue();
        QString setValue;
        if (type == "double" || type == "constant") {
            QSlider* slider = ((Ui::Constval_UI*)valueItems[na.toElement().text()])->horizontalSlider;
            setValue = QString::number(slider->value());
        } else if (type == "list") {
            KComboBox *box = ((Ui::Listval_UI*)valueItems[na.toElement().text()])->list;
            setValue = box->itemData(box->currentIndex()).toString();
        } else if (type == "bool") {
            QCheckBox *box = ((Ui::Boolval_UI*)valueItems[na.toElement().text()])->checkBox;
            setValue = box->checkState() == Qt::Checked ? "1" : "0" ;
        } else if (type == "color") {
            KColorButton *color = ((Ui::Colorval_UI*)valueItems[na.toElement().text()])->kcolorbutton;
            setValue.sprintf("0x%08x", color->color().rgba());
        } else if (type == "complex") {
            ComplexParameter *complex = ((ComplexParameter*)valueItems[na.toElement().text()+"complex"]);
            namenode.item(i) = complex->getParamDesc();
        } else if (type == "geometry") {
            Geometryval *geom = ((Geometryval*)valueItems[na.toElement().text()+"geometry"]);
            namenode.item(i) = geom->getParamDesc();
        } else if (type == "wipe") {
            Ui::Wipeval_UI *wp = (Ui::Wipeval_UI*)valueItems[na.toElement().text()];
            wipeInfo info;
            if (wp->start_left->isChecked()) info.start = LEFT;
            else if (wp->start_right->isChecked()) info.start = RIGHT;
            else if (wp->start_up->isChecked()) info.start = UP;
            else if (wp->start_down->isChecked()) info.start = DOWN;
            else if (wp->start_center->isChecked()) info.start = CENTER;
            info.startTransparency = wp->start_transp->value();
            if (wp->end_left->isChecked()) info.end = LEFT;
            else if (wp->end_right->isChecked()) info.end = RIGHT;
            else if (wp->end_up->isChecked()) info.end = UP;
            else if (wp->end_down->isChecked()) info.end = DOWN;
            else if (wp->end_center->isChecked()) info.end = CENTER;
            info.endTransparency = wp->end_transp->value();
            setValue = getWipeString(info);
        }

        if (!setValue.isNull()) {
            pa.attributes().namedItem("value").setNodeValue(setValue);
        }
    }
    emit parameterChanged(oldparam, params);
}

void EffectStackEdit::createSliderItem(const QString& name, int val , int min, int max) {
    QWidget* toFillin = new QWidget;
    Ui::Constval_UI *ctval = new Ui::Constval_UI;
    ctval->setupUi(toFillin);

    ctval->horizontalSlider->setMinimum(min);
    ctval->horizontalSlider->setMaximum(max);
    ctval->spinBox->setMinimum(min);
    ctval->spinBox->setMaximum(max);
    ctval->horizontalSlider->setPageStep((int)(max - min) / 10);
    ctval->horizontalSlider->setValue(val);
    ctval->label->setText(name);
    valueItems[name] = ctval;
    uiItems.append(ctval);
    connect(ctval->horizontalSlider, SIGNAL(valueChanged(int)) , this, SLOT(collectAllParameters()));
    items.append(toFillin);
    vbox->addWidget(toFillin);
}

void EffectStackEdit::slotSliderMoved(int) {
    collectAllParameters();
}

void EffectStackEdit::clearAllItems() {
    qDeleteAll(items);
    foreach(void *p, uiItems) {
        delete p;
    }
    uiItems.clear();
    items.clear();
    valueItems.clear();
}
