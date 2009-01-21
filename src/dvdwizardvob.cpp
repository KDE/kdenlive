/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include <KUrlRequester>
#include <KDebug>

#include "dvdwizardvob.h"

DvdWizardVob::DvdWizardVob(QWidget *parent): QWizardPage(parent) {
    m_view.setupUi(this);
    m_view.intro_vob->setEnabled(false);
    m_view.vob_1->setFilter("video/mpeg");
    m_view.intro_vob->setFilter("video/mpeg");
    connect(m_view.use_intro, SIGNAL(toggled(bool)), m_view.intro_vob, SLOT(setEnabled(bool)));
    connect(m_view.vob_1, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));

}

DvdWizardVob::~DvdWizardVob() {
    QList<KUrlRequester *> allUrls = m_view.vob_list->findChildren<KUrlRequester *>();
    qDeleteAll(allUrls);
}

// virtual

bool DvdWizardVob::isComplete() const {
    if (m_view.vob_1->url().path().isEmpty()) return false;
    if (QFile::exists(m_view.vob_1->url().path())) return true;
    return false;
}

void DvdWizardVob::setUrl(const QString &url) {
    m_view.vob_1->setPath(url);
}

QStringList DvdWizardVob::selectedUrls() const {
    QStringList result;
    QList<KUrlRequester *> allUrls = m_view.vob_list->findChildren<KUrlRequester *>();
    for (int i = 0; i < allUrls.count(); i++) {
        if (!allUrls.at(i)->url().path().isEmpty()) {
            result.append(allUrls.at(i)->url().path());
        }
    }
    return result;
}

QString DvdWizardVob::introMovie() const {
    if (!m_view.use_intro->isChecked()) return QString();
    return m_view.intro_vob->url().path();
}

void DvdWizardVob::slotCheckVobList(const QString &text) {
    emit completeChanged();
    QList<KUrlRequester *> allUrls = m_view.vob_list->findChildren<KUrlRequester *>();
    int count = allUrls.count();
    kDebug() << "// FOUND " << count << " URLS";
    if (count == 1) {
        if (text.isEmpty()) return;
        // insert second urlrequester
        KUrlRequester *vob = new KUrlRequester(this);
        vob->setFilter("video/mpeg");
        m_view.vob_list->layout()->addWidget(vob);
        connect(vob, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));
    } else if (text.isEmpty()) {
        if (allUrls.at(count - 1)->url().path().isEmpty() && allUrls.at(count - 2)->url().path().isEmpty()) {
            // The last 2 urlrequesters are empty, remove last one
            KUrlRequester *vob = allUrls.takeLast();
            delete vob;
        }
    } else {
        if (allUrls.at(count - 1)->url().path().isEmpty()) return;
        KUrlRequester *vob = new KUrlRequester(this);
        vob->setFilter("video/mpeg");
        m_view.vob_list->layout()->addWidget(vob);
        connect(vob, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckVobList(const QString &)));
    }
}

