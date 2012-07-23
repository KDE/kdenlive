/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2011 by Marco Gittler (marco@gitma.de)                  *
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


#include "clipstabilize.h"
#include "doubleparameterwidget.h"

#include <KDebug>
#include <mlt++/Mlt.h>
#include "kdenlivesettings.h"
#include <KGlobalSettings>
#include <KMessageBox>
#include <KColorScheme>
#include <QtConcurrentRun>
#include <QTimer>
#include <QSlider>
#include <KFileDialog>

ClipStabilize::ClipStabilize(const QString &dest, int count, const QString &filterName,QWidget * parent) :
        QDialog(parent), 
        m_filtername(filterName),
        m_count(count),
        vbox(NULL)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", count));

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

    if (m_count == 1) {
        QString newFile = dest;
        newFile.append(".mlt");
        KUrl dest(newFile);
        dest_url->setMode(KFile::File);
        dest_url->setUrl(KUrl(newFile));
        dest_url->fileDialog()->setOperationMode(KFileDialog::Saving);
    } else {
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory);
        dest_url->setUrl(KUrl(dest));
        dest_url->fileDialog()->setOperationMode(KFileDialog::Saving);
    }

    if (m_filtername=="videostab"){
        QStringList ls;
        ls << "shutterangle,type,int,value,0,min,0,max,100,tooltip,Angle that Images could be maximum rotated";
        fillParameters(ls);
    }else if (m_filtername=="videostab2"){
        QStringList ls;
        ls << "accuracy,type,int,value,4,min,1,max,10,tooltip,Accuracy of Shakiness detection";
        ls << "shakiness,type,int,value,4,min,1,max,10,tooltip,How shaky is the Video";
        ls << "stepsize,type,int,value,6,min,0,max,100,tooltip,Stepsize of Detection process minimum around";
        ls << "algo,type,bool,value,1,min,0,max,1,tooltip,0 = Bruteforce 1 = small measurement fields";
        ls << "mincontrast,type,double,value,0.3,min,0,max,1,factor,1,decimals,2,tooltip,Below this Contrast Field is discarded";
        ls << "show,type,int,value,0,min,0,max,2,tooltip,0 = draw nothing. 1 or 2 show fields and transforms";
        ls << "smoothing,type,int,value,10,min,0,max,100,tooltip,number of frames for lowpass filtering";
        ls << "maxshift,type,int,value,-1,min,-1,max,1000,tooltip,max number of pixels to shift";
        ls << "maxangle,type,int,value,-1,min,-1,max,1000,tooltip,max anglen to rotate (in rad)";
        ls << "crop,type,bool,value,0,min,0,max,1,tooltip,0 = keep border  1 = black background";
        ls << "invert,type,bool,value,0,min,0,max,1,tooltip,invert transform";
        ls << "realtive,type,bool,value,1,min,0,max,1,tooltip,0 = absolute transform  1= relative";
        ls << "zoom,type,int,value,0,min,-500,max,500,tooltip,additional zoom during transform";
        ls << "optzoom,type,bool,value,1,min,0,max,1,tooltip,use optimal zoom (calulated from transforms)";
        ls << "sharpen,type,double,value,0.8,min,0,max,1,decimals,1,tooltip,sharpen transformed image";
        fillParameters(ls);

    }

    //connect(buttonBox,SIGNAL(rejected()), this, SLOT(slotAbortStabilize()));

    vbox=new QVBoxLayout(optionsbox);
    QHashIterator<QString,QHash<QString,QString> > hi(m_ui_params);
    while(hi.hasNext()){
        hi.next();
        QHash<QString,QString> val=hi.value();
        if (val["type"]=="int" || val["type"]=="double"){
            DoubleParameterWidget *dbl=new DoubleParameterWidget(hi.key(), val["value"].toDouble(),
                    val["min"].toDouble(),val["max"].toDouble(),val["value"].toDouble(),
                    "",0/*id*/,""/*suffix*/,val["decimals"]!=""?val["decimals"].toInt():0,this);
            dbl->setObjectName(hi.key());
            dbl->setToolTip(val["tooltip"]);
            connect(dbl,SIGNAL(valueChanged(double)),this,SLOT(slotUpdateParams()));
            vbox->addWidget(dbl);
        }else if (val["type"]=="bool"){
            QCheckBox *ch=new QCheckBox(hi.key(),this);
            ch->setCheckState(val["value"] == "0" ? Qt::Unchecked : Qt::Checked);
            ch->setObjectName(hi.key());
            connect(ch, SIGNAL(stateChanged(int)) , this,SLOT(slotUpdateParams()));
            ch->setToolTip(val["tooltip"]);
            vbox->addWidget(ch);
        
        }
    }
    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    /*if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }*/
}

QStringList ClipStabilize::params()
{
    //we must return a stringlist with:
    // producerparams << filtername << filterparams << consumer << consumerparams
    QStringList params;
    // producer params
    params << QString();
    // filter
    params << m_filtername;
    QStringList filterparamsList;
    QHashIterator <QString,QHash<QString,QString> > it(m_ui_params);
    while (it.hasNext()){
        it.next();
        filterparamsList << it.key() + '=' + it.value().value("value");
    }
    params << filterparamsList.join(" ");
    
    // consumer
    params << "xml";
    // consumer params
    QString title = i18n("Stabilised");
    params << QString("all=1 title=\"%1\"").arg(title);
    return params;
}

QString ClipStabilize::destination() const
{
    if (m_count == 1)
        return dest_url->url().path();
    else
        return dest_url->url().directory(KUrl::AppendTrailingSlash);
}

QString ClipStabilize::desc() const
{
    return i18n("Stabilize clip");
}

void ClipStabilize::slotStartStabilize()
{
    /*
    if (m_consumer && !m_consumer->is_stopped()) {
        return;
    }
    m_duration = 0;
    QStringList parameters;
    QString destination;
    //QString params = ffmpeg_params->toPlainText().simplified();
    if (urls_list->count() > 0) {
        source_url->setUrl(m_urls.takeFirst());
        destination = dest_url->url().path(KUrl::AddTrailingSlash)+ source_url->url().fileName()+".mlt";
        QList<QListWidgetItem *> matching = urls_list->findItems(source_url->url().path(), Qt::MatchExactly);
        if (matching.count() > 0) {
            matching.at(0)->setFlags(Qt::ItemIsSelectable);
            urls_list->setCurrentItem(matching.at(0));
        }
    } else {
        destination = dest_url->url().path();
    }
    QString s_url = source_url->url().path();

    if (QFile::exists(destination)) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", destination )) == KMessageBox::No) return;
    }
    buttonBox->button(QDialogButtonBox::Abort)->setText(i18n("Abort"));

    if (m_profile){
        m_playlist= new Mlt::Playlist;
        Mlt::Filter filter(*m_profile,filtername.toUtf8().data());
        QHashIterator <QString,QHash<QString,QString> > it(m_ui_params);
        while (it.hasNext()){
            it.next();
            filter.set(
                    it.key().toAscii().data(),
                    QString::number(it.value()["value"].toDouble()).toAscii().data());
        }
        Mlt::Producer p(*m_profile,s_url.toUtf8().data());
        if (p.is_valid()) {
            m_playlist->append(p);
            m_playlist->attach(filter);
            m_consumer= new Mlt::Consumer(*m_profile,"xml",destination.toUtf8().data());
            m_consumer->set("all",1);
            m_consumer->set("real_time",-2);
            m_consumer->connect(*m_playlist);
            m_stabilizeRun = QtConcurrent::run(this, &ClipStabilize::slotRunStabilize);
            m_timer->start(500);
            button_start->setEnabled(false);
        }
    }
*/
}



void ClipStabilize::slotUpdateParams()
{
    for (int i=0;i<vbox->count();i++){
        QWidget* w=vbox->itemAt(i)->widget();
        QString name=w->objectName();
        if (!name.isEmpty() && m_ui_params.contains(name)){
            if (m_ui_params[name]["type"]=="int" || m_ui_params[name]["type"]=="double"){
                DoubleParameterWidget *dbl=(DoubleParameterWidget*)w;
                m_ui_params[name]["value"]=QString::number((double)(dbl->getValue()));
            }else if (m_ui_params[name]["type"]=="bool"){
                QCheckBox *ch=(QCheckBox*)w;
                m_ui_params[name]["value"]= ch->checkState() == Qt::Checked ? "1" : "0" ;
            }
        }
    }
}

bool ClipStabilize::autoAddClip() const
{
    return auto_add->isChecked();
}

void ClipStabilize::fillParameters(QStringList lst)
{

    m_ui_params.clear();
    while (!lst.isEmpty()){
        QString vallist=lst.takeFirst();
        QStringList cont=vallist.split(',');
        QString name=cont.takeFirst();
        while (!cont.isEmpty()){
            QString valname=cont.takeFirst();
            QString val;
            if (!cont.isEmpty()){
                val=cont.takeFirst();
            }
            m_ui_params[name][valname]=val;
        }
    }

}


#include "clipstabilize.moc"


