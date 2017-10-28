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
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "widgets/doublewidget.h"
#include "widgets/positionwidget.h"

#include "kdenlivesettings.h"
#include <KMessageBox>
#include <QFontDatabase>
#include <klocalizedstring.h>
#include <mlt++/Mlt.h>

ClipStabilize::ClipStabilize(const std::vector<QString> &binIds, const QString &filterName, int out, QWidget *parent)
    : QDialog(parent)
    , m_filtername(filterName)
    , m_binIds(binIds)
    , vbox(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_binIds.size()));
    auto_add->setChecked(KdenliveSettings::add_new_clip());

    // QString stylesheet = EffectStackView2::getStyleSheet();
    // setStyleSheet(stylesheet);

    Q_ASSERT(binIds.size() > 0);
    auto firstBinClip = pCore->projectItemModel()->getClipByBinID(m_binIds.front());
    auto firstUrl = firstBinClip->url();
    if (m_binIds.size() == 1) {
        QString newFile = firstUrl;
        newFile.append(QStringLiteral(".mlt"));
        dest_url->setMode(KFile::File);
        dest_url->setUrl(QUrl(newFile));
    } else {
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory | KFile::ExistingOnly);
        dest_url->setUrl(QUrl(firstUrl).adjusted(QUrl::RemoveFilename));
    }

    if (m_filtername == QLatin1String("vidstab") || m_filtername == QLatin1String("videostab2")) {
        m_fixedParams[QStringLiteral("algo")] = QStringLiteral("1");
        m_fixedParams[QStringLiteral("relative")] = QStringLiteral("1");
        fillParameters(
            QStringList() << QStringLiteral("accuracy,type,int,value,8,min,1,max,10,tooltip,Accuracy of Shakiness detection")
                          << QStringLiteral("shakiness,type,int,value,4,min,1,max,10,tooltip,How shaky is the Video")
                          << QStringLiteral("stepsize,type,int,value,6,min,0,max,100,tooltip,Stepsize of Detection process minimum around")
                          << QStringLiteral("mincontrast,type,double,value,0.3,min,0,max,1,factor,1,decimals,2,tooltip,Below this Contrast Field is discarded")
                          << QStringLiteral("smoothing,type,int,value,10,min,0,max,100,tooltip,number of frames for lowpass filtering")
                          << QStringLiteral("maxshift,type,int,value,-1,min,-1,max,1000,tooltip,max number of pixels to shift")
                          << QStringLiteral("maxangle,type,double,value,-1,min,-1,max,3.14,decimals,2,tooltip,max angle to rotate (in rad)")
                          << QStringLiteral("crop,type,bool,value,0,min,0,max,1,tooltip,0 = keep border  1 = black background")
                          << QStringLiteral("zoom,type,int,value,0,min,-500,max,500,tooltip,additional zoom during transform")
                          << QStringLiteral("optzoom,type,bool,value,1,min,0,max,1,tooltip,use optimal zoom (calulated from transforms)")
                          << QStringLiteral("sharpen,type,double,value,0.8,min,0,max,1,decimals,1,tooltip,sharpen transformed image")
                          << QStringLiteral("tripod,type,position,value,0,min,0,max,100000,tooltip,reference frame"));

    } else if (m_filtername == QLatin1String("videostab")) {
        fillParameters(QStringList(QStringLiteral("shutterangle,type,int,value,0,min,0,max,180,tooltip,Angle that Images could be maximum rotated")));
    }

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &ClipStabilize::slotValidate);

    vbox = new QVBoxLayout(optionsbox);
    QHashIterator<QString, QHash<QString, QString>> hi(m_ui_params);
    m_tc.setFormat(KdenliveSettings::project_fps());
    while (hi.hasNext()) {
        hi.next();
        QHash<QString, QString> val = hi.value();
        if (val[QStringLiteral("type")] == QLatin1String("int") || val[QStringLiteral("type")] == QLatin1String("double")) {
            DoubleWidget *dbl = new DoubleWidget(hi.key() /*name*/, val[QStringLiteral("value")].toDouble(), val[QStringLiteral("min")].toDouble(),
                                                 val[QStringLiteral("max")].toDouble(), val[QStringLiteral("value")].toDouble(), /*default*/
                                                 QString(),                                                                      /*comment*/
                                                 0 /*id*/, QString(),                                                            /*suffix*/
                                                 val[QStringLiteral("decimals")] != QString() ? val[QStringLiteral("decimals")].toInt() : 0, this);
            dbl->setObjectName(hi.key());
            dbl->setToolTip(val[QStringLiteral("tooltip")]);
            connect(dbl, &DoubleWidget::valueChanged, this, &ClipStabilize::slotUpdateParams);
            vbox->addWidget(dbl);
        } else if (val[QStringLiteral("type")] == QLatin1String("bool")) {
            auto *ch = new QCheckBox(hi.key(), this);
            ch->setCheckState(val[QStringLiteral("value")] == QLatin1String("0") ? Qt::Unchecked : Qt::Checked);
            ch->setObjectName(hi.key());
            connect(ch, &QCheckBox::stateChanged, this, &ClipStabilize::slotUpdateParams);
            ch->setToolTip(val[QStringLiteral("tooltip")]);
            vbox->addWidget(ch);
        } else if (val[QStringLiteral("type")] == QLatin1String("position")) {
            PositionWidget *posedit = new PositionWidget(hi.key(), 0, 0, out, m_tc, QString(), this);
            posedit->setToolTip(val[QStringLiteral("tooltip")]);
            posedit->setObjectName(hi.key());
            vbox->addWidget(posedit);
            connect(posedit, &PositionWidget::valueChanged, this, &ClipStabilize::slotUpdateParams);
        }
    }
    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    /*if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }*/
    KdenliveSettings::setAdd_new_clip(auto_add->isChecked());
}

std::unordered_map<QString, QString> ClipStabilize::filterParams() const
{
    std::unordered_map<QString, QString> params;

    for (const auto &it : m_fixedParams) {
        params[it.first] = it.second;
    }

    QHashIterator<QString, QHash<QString, QString>> it(m_ui_params);
    while (it.hasNext()) {
        it.next();
        params[it.key()] = it.value().value(QStringLiteral("value"));
    }
    return params;
}

QString ClipStabilize::filterName() const
{
    return m_filtername;
}

QString ClipStabilize::destination() const
{
    QString path = dest_url->url().toLocalFile();
    if (m_binIds.size() > 1 && !path.endsWith(QDir::separator())) {
        path.append(QDir::separator());
    }
    return path;
}

QString ClipStabilize::desc() const
{
    return i18n("Stabilize clip");
}

void ClipStabilize::slotUpdateParams()
{
    for (int i = 0; i < vbox->count(); ++i) {
        QWidget *w = vbox->itemAt(i)->widget();
        QString name = w->objectName();
        if (!name.isEmpty() && m_ui_params.contains(name)) {
            if (m_ui_params[name][QStringLiteral("type")] == QLatin1String("int") || m_ui_params[name][QStringLiteral("type")] == QLatin1String("double")) {
                DoubleWidget *dbl = static_cast<DoubleWidget *>(w);
                m_ui_params[name][QStringLiteral("value")] = QString::number((double)(dbl->getValue()));
            } else if (m_ui_params[name][QStringLiteral("type")] == QLatin1String("bool")) {
                QCheckBox *ch = (QCheckBox *)w;
                m_ui_params[name][QStringLiteral("value")] = ch->checkState() == Qt::Checked ? QStringLiteral("1") : QStringLiteral("0");
            } else if (m_ui_params[name][QStringLiteral("type")] == QLatin1String("position")) {
                PositionWidget *pos = (PositionWidget *)w;
                m_ui_params[name][QStringLiteral("value")] = QString::number(pos->getPosition());
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
    while (!lst.isEmpty()) {
        QString vallist = lst.takeFirst();
        QStringList cont = vallist.split(QLatin1Char(','));
        QString name = cont.takeFirst();
        while (!cont.isEmpty()) {
            QString valname = cont.takeFirst();
            QString val;
            if (!cont.isEmpty()) {
                val = cont.takeFirst();
            }
            m_ui_params[name][valname] = val;
        }
    }
}

void ClipStabilize::slotValidate()
{
    if (m_binIds.size() == 1) {
        if (QFile::exists(dest_url->url().toLocalFile())) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", dest_url->url().toLocalFile())) ==
                KMessageBox::No) {
                return;
            }
        }
    } else {
        QDir folder(dest_url->url().toLocalFile());
        QStringList existingFiles;
        for (const QString &binId : m_binIds) {
            auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
            auto url = binClip->url();
            if (folder.exists(url + QStringLiteral(".mlt"))) {
                existingFiles.append(folder.absoluteFilePath(url + QStringLiteral(".mlt")));
            }
        }
        if (!existingFiles.isEmpty()) {
            if (KMessageBox::warningContinueCancelList(this, i18n("The stabilize job will overwrite the following files:"), existingFiles) ==
                KMessageBox::Cancel) {
                return;
            }
        }
    }
    accept();
}
