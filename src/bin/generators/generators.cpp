/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "generators.h"
#include "assets/abstractassetsrepository.hpp"
#include "doc/kthumb.h"
#include "timecodedisplay.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QDomDocument>
#include <QFileDialog>
#include <QLabel>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "core.h"
#include "klocalizedstring.h"
#include "kxmlgui_version.h"
#include "profiles/profilemodel.hpp"
#include <KMessageBox>
#include <KRecentDirs>
#include <memory>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>

Generators::Generators(const QString &path, QWidget *parent)
    : QDialog(parent)
    , m_producer(nullptr)
    , m_timePos(nullptr)
    , m_container(nullptr)
    , m_preview(nullptr)
{
    QFile file(path);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("generator")) {
        QString generatorTag = base.attribute(QStringLiteral("tag"));
        setWindowTitle(i18n(base.firstChildElement(QStringLiteral("name")).text().toUtf8().data()));
        auto *lay = new QVBoxLayout(this);
        m_preview = new QLabel;
        m_preview->setMinimumSize(1, 1);
        lay->addWidget(m_preview);
        m_producer = new Mlt::Producer(pCore->getCurrentProfile()->profile(), generatorTag.toUtf8().constData());
        m_pixmap = QPixmap::fromImage(KThumb::getFrame(m_producer, 0, pCore->getCurrentProfile()->width(), pCore->getCurrentProfile()->height()));
        m_preview->setPixmap(m_pixmap.scaledToWidth(m_preview->width()));
        auto *hlay = new QHBoxLayout;
        hlay->addWidget(new QLabel(i18n("Duration")));
        m_timePos = new TimecodeDisplay(pCore->timecode(), this);
        if (base.hasAttribute(QStringLiteral("updateonduration"))) {
            connect(m_timePos, &TimecodeDisplay::timeCodeEditingFinished, this, &Generators::updateDuration);
        }
        hlay->addWidget(m_timePos);
        lay->addLayout(hlay);
        QWidget *frameWidget = new QWidget;
        lay->addWidget(frameWidget);

        m_view = new AssetParameterView(frameWidget);
        lay->addWidget(m_view);
        QString tag = base.attribute(QStringLiteral("tag"), QString());

        auto prop = std::make_unique<Mlt::Properties>(m_producer->get_properties());
        m_assetModel.reset(new AssetParameterModel(std::move(prop), base, tag, {ObjectType::NoItem, -1})); // NOLINT
        m_view->setModel(m_assetModel, QSize(1920, 1080), false);
        connect(m_assetModel.get(), &AssetParameterModel::modelChanged, this, [this]() { updateProducer(); });

        lay->addStretch(10);
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(buttonBox);
        m_timePos->setValue(KdenliveSettings::color_duration());
    }
}

void Generators::updateProducer()
{
    int w = m_pixmap.width();
    int h = m_pixmap.height();
    m_pixmap = QPixmap::fromImage(KThumb::getFrame(m_producer, 0, w, h));
    m_preview->setPixmap(m_pixmap.scaledToWidth(m_preview->width()));
}

void Generators::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    m_preview->setPixmap(m_pixmap.scaledToWidth(m_preview->width()));
}

Generators::~Generators()
{
    delete m_timePos;
}

// static
void Generators::getGenerators(const QStringList &producers, QMenu *menu)
{
    const QStringList generatorFolders =
        QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("generators"), QStandardPaths::LocateDirectory);
    const QStringList filters = QStringList() << QStringLiteral("*.xml");
    QStringList parsedGenerators;
    for (const QString &folder : generatorFolders) {
        QDir directory(folder);
        const QStringList filesnames = directory.entryList(filters, QDir::Files);
        for (const QString &fname : filesnames) {
            if (parsedGenerators.contains(fname)) {
                continue;
            }
            parsedGenerators << fname;
            QPair<QString, QString> result = parseGenerator(directory.absoluteFilePath(fname), producers);
            if (!result.first.isEmpty()) {
                QAction *action = menu->addAction(i18n(result.first.toUtf8().data()));
                action->setData(result.second);
            }
        }
    }
}

// static
QPair<QString, QString> Generators::parseGenerator(const QString &path, const QStringList &producers)
{
    QPair<QString, QString> result;
    QDomDocument doc;
    QFile file(path);
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("generator")) {
        QString generatorTag = base.attribute(QStringLiteral("tag"));
        if (producers.contains(generatorTag)) {
            result.first = base.firstChildElement(QStringLiteral("name")).text();
            result.second = path;
        }
    }
    return result;
}

void Generators::updateDuration(int duration)
{
    m_producer->set("length", duration);
    m_producer->set_in_and_out(0, duration - 1);
    updateProducer();
}

QUrl Generators::getSavedClip(QString clipFolder)
{
    if (clipFolder.isEmpty()) {
        clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
    }
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QFileDialog fd(this);
    fd.setDirectory(clipFolder);
    fd.setNameFilter(i18n("MLT playlist (*.mlt)"));
    fd.setAcceptMode(QFileDialog::AcceptSave);
    fd.setFileMode(QFileDialog::AnyFile);
    fd.setDefaultSuffix(QStringLiteral("mlt"));
    if (fd.exec() != QDialog::Accepted || fd.selectedUrls().isEmpty()) {
        return QUrl();
    }
    QUrl url = fd.selectedUrls().constFirst();

    if (url.isValid()) {
        Mlt::Tractor trac(*m_producer->profile());
        m_producer->set("length", m_timePos->getValue());
        m_producer->set_in_and_out(0, m_timePos->getValue() - 1);
        trac.set_track(*m_producer, 0);
        Mlt::Consumer c(*m_producer->profile(), "xml", url.toLocalFile().toUtf8().constData());
        c.connect(trac);
        c.run();
        return url;
    }
    return QUrl();
}
