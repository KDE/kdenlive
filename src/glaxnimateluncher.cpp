/*
    SPDX-FileCopyrightText: 2022 Meltytech, LLC
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "glaxnimateluncher.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/kdenlivesettingsdialog.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "timeline2/view/timelinewidget.h"

#include <KConfigDialog>
#include <KMessageBox>
#include <KUrlRequesterDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>

bool GlaxnimateLuncher::checkInstalled()
{
    if (!KdenliveSettings::glaxnimatePath().isEmpty() && QFile(KdenliveSettings::glaxnimatePath()).exists()) {
        return true;
    }
    QUrl url = KUrlRequesterDialog::getUrl(QUrl(), nullptr, i18n("Enter path to the Glaxnimate application"));
    if (url.isEmpty() || !QFile(url.toLocalFile()).exists()) {
        KMessageBox::error(QApplication::activeWindow(), i18n("You need enter a valid path to be able to edit Lottie animations."));
        return false;
    }
    KdenliveSettings::setGlaxnimatePath(url.toLocalFile());
    KdenliveSettingsDialog *d = static_cast<KdenliveSettingsDialog *>(KConfigDialog::exists(QStringLiteral("settings")));
    if (d) {
        d->updateExternalApps();
    }
    return true;
}

GlaxnimateLuncher &GlaxnimateLuncher::instance()
{
    static GlaxnimateLuncher instance;
    return instance;
}

void GlaxnimateLuncher::reset()
{
    if (m_stream && m_socket && m_stream && QLocalSocket::ConnectedState == m_socket->state()) {
        *m_stream << QString("clear");
        m_socket->flush();
    }
    m_parent.reset();
}

void GlaxnimateLuncher::openFile(const QString &filename)
{
    QString error = pCore->openExternalApp(KdenliveSettings::glaxnimatePath(), {filename});
    if (!error.isEmpty()) {
        KMessageBox::detailedError(QApplication::activeWindow(), i18n("Failed to lunch Glaxnimate application"), error);
        return;
    }
}

void GlaxnimateLuncher::openClip(int clipId)
{
    if (!checkInstalled()) {
        return;
    }
    m_parent.reset(new ParentResources);

    m_parent->m_binClip = pCore->projectItemModel()->getClipByBinID(pCore->window()->getCurrentTimeline()->model()->getClipBinId(clipId));
    if (m_parent->m_binClip->clipType() != ClipType::Animation) {
        pCore->displayMessage(i18n("Item is not an animation clip"), ErrorMessage, 500);
        return;
    }

    QString filename = m_parent->m_binClip->clipUrl();

    if (m_server && m_socket && m_stream && QLocalSocket::ConnectedState == m_socket->state()) {
        auto s = QString("open ").append(filename);
        qDebug() << s;
        *m_stream << s;
        m_socket->flush();
        m_parent->m_frameNum = -1;
        return;
    }
    m_parent->m_clipId = clipId;
    m_server.reset(new QLocalServer);
    connect(m_server.get(), &QLocalServer::newConnection, this, &GlaxnimateLuncher::onConnect);
    QString name = QString("kdenlive-%1").arg(QCoreApplication::applicationPid());
    QStringList args = {"--ipc", name, filename};
    /*QProcess childProcess;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("LC_ALL");
    childProcess.setProcessEnvironment(env);*/
    QString error;
    if (!m_server->listen(name)) {
        qDebug() << "failed to start the IPC server:" << m_server->errorString();
        m_server.reset();
        args.clear();
        args << filename;
        qDebug() << "Run without --ipc";
        error = pCore->openExternalApp(KdenliveSettings::glaxnimatePath(), args);
        if (error.isEmpty()) {
            m_sharedMemory.reset(new QSharedMemory(name));
            return;
        }
    } else {
        if (pCore->openExternalApp(KdenliveSettings::glaxnimatePath(), args).isEmpty()) {
            m_sharedMemory.reset(new QSharedMemory(name));
            return;
        } else {
            // This glaxnimate executable may not support --ipc
            // XXX startDetached is not failing in this case, need something better
            qDebug() << "Failed to start glaxnimate with the --ipc, trying without now";
            m_server.reset();
            args.clear();
            args << filename;
            qDebug() << "Run without --ipc";
            error = pCore->openExternalApp(KdenliveSettings::glaxnimatePath(), args);
        }
    }
    if (!error.isEmpty()) {
        KMessageBox::detailedError(QApplication::activeWindow(), i18n("Failed to lunch Glaxnimate application"), error);
        return;
    }
}

void GlaxnimateLuncher::onConnect()
{
    m_socket = m_server->nextPendingConnection();
    connect(m_socket, &QLocalSocket::readyRead, this, &GlaxnimateLuncher::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &GlaxnimateLuncher::onSocketError);
    m_stream.reset(new QDataStream(m_socket));
    m_stream->setVersion(QDataStream::Qt_5_15);
    *m_stream << QString("hello");
    m_socket->flush();
    m_server->close();
    m_isProtocolValid = false;
}

void GlaxnimateLuncher::onReadyRead()
{
    if (!m_isProtocolValid) {
        QString message;
        *m_stream >> message;
        qDebug() << message;
        if (message.startsWith("version ") && message != "version 1") {
            *m_stream << QString("bye");
            m_socket->flush();
            m_server->close();
        } else {
            m_isProtocolValid = true;
        }
    } else {
        qreal time = -1.0;
        for (int i = 0; i < 1000 && !m_stream->atEnd(); i++) {
            *m_stream >> time;
        }

        // Only if the frame number is different
        int frameNum = pCore->window()->getCurrentTimeline()->model()->getClipPosition(m_parent->m_clipId) + time -
                       pCore->window()->getCurrentTimeline()->model()->getClipIn(m_parent->m_clipId);
        if (frameNum != m_parent->m_frameNum) {
            qDebug() << "glaxnimate time =" << time << "=> Kdenlive frameNum =" << frameNum;

            // Get the image from MLT
            pCore->window()->getCurrentTimeline()->model()->producer().get()->seek(frameNum);
            QList<int> clips = m_parent->m_binClip->timelineInstances();
            // Temporarily hide this title clip in timeline so that it does not appear when requesting background frame
            pCore->temporaryUnplug(clips, true);
            std::unique_ptr<Mlt::Frame> frame(pCore->window()->getCurrentTimeline()->model()->producer().get()->get_frame());
            QImage temp = KThumb::getFrame(frame.get(), pCore->getCurrentFrameSize().width(), pCore->getCurrentFrameSize().height());
            pCore->temporaryUnplug(clips, false);
            if (copyToShared(temp)) {
                m_parent->m_frameNum = frameNum;
            }
        }
    }
}

void GlaxnimateLuncher::onSocketError(QLocalSocket::LocalSocketError socketError)
{
    switch (socketError) {
    case QLocalSocket::PeerClosedError:
        qDebug() << "Glaxnimate closed the connection";
        m_stream.reset();
        m_sharedMemory.reset();
        break;
    default:
        qDebug() << "Glaxnimate IPC error:" << m_socket->errorString();
    }
}

bool GlaxnimateLuncher::copyToShared(const QImage &image)
{
    if (!m_sharedMemory) {
        return false;
    }
    qint32 sizeInBytes = image.sizeInBytes() + 4 * sizeof(qint32);
    if (sizeInBytes > m_sharedMemory->size()) {
        if (m_sharedMemory->isAttached()) {
            m_sharedMemory->lock();
            m_sharedMemory->detach();
            m_sharedMemory->unlock();
        }
        // over-allocate to avoid recreating
        if (!m_sharedMemory->create(sizeInBytes)) {
            qDebug() << m_sharedMemory->errorString();
            return false;
        }
    }
    if (m_sharedMemory->isAttached()) {
        m_sharedMemory->lock();

        uchar *to = (uchar *)m_sharedMemory->data();
        // Write the width of the image and move the pointer forward
        qint32 width = image.width();
        ::memcpy(to, &width, sizeof(width));
        to += sizeof(width);

        // Write the height of the image and move the pointer forward
        qint32 height = image.height();
        ::memcpy(to, &height, sizeof(height));
        to += sizeof(height);

        // Write the image format of the image and move the pointer forward
        qint32 imageFormat = image.format();
        ::memcpy(to, &imageFormat, sizeof(imageFormat));
        to += sizeof(imageFormat);

        // Write the bytes per line of the image and move the pointer forward
        qint32 bytesPerLine = image.bytesPerLine();
        ::memcpy(to, &bytesPerLine, sizeof(bytesPerLine));
        to += sizeof(bytesPerLine);

        // Write the raw data of the image and move the pointer forward
        ::memcpy(to, image.constBits(), image.sizeInBytes());

        m_sharedMemory->unlock();
        if (m_stream && m_socket) {
            *m_stream << QString("redraw");
            m_socket->flush();
        }
        return true;
    }
    return false;
}
