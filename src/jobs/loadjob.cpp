/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "loadjob.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "mltcontroller/clip.h"
#include "profiles/profilemodel.hpp"
#include "project/dialogs/slideshowclip.h"
#include "xml/xml.hpp"
#include <QMimeDatabase>
#include <QWidget>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

LoadJob::LoadJob(const QString &binId, const QDomElement &xml)
    : AbstractClipJob(LOADJOB, binId)
    , m_xml(xml)
{
}

const QString LoadJob::getDescription() const
{
    return i18n("Loading clip %1", m_clipId);
}

namespace {
ClipType getTypeForService(const QString &id, const QString &path)
{
    if (id.isEmpty()) {
        QString ext = path.section(QLatin1Char('.'), -1);
        if (ext == QLatin1String("mlt") || ext == QLatin1String("kdenlive")) {
            return ClipType::Playlist;
        }
        return ClipType::Unknown;
    }
    if (id == QLatin1String("color") || id == QLatin1String("colour")) {
        return ClipType::Color;
    }
    if (id == QLatin1String("kdenlivetitle")) {
        return ClipType::Text;
    }
    if (id == QLatin1String("qtext")) {
        return ClipType::QText;
    }
    if (id == QLatin1String("xml") || id == QLatin1String("consumer")) {
        return ClipType::Playlist;
    }
    if (id == QLatin1String("webvfx")) {
        return ClipType::WebVfx;
    }
    return ClipType::Unknown;
}

// Read the properties of the xml and pass them to the producer. Note that some properties like resource are ignored
void processProducerProperties(std::shared_ptr<Mlt::Producer> prod, const QDomElement &xml)
{
    // TODO: there is some duplication with clipcontroller > updateproducer that also copies properties
    QString value;
    QStringList internalProperties;
    internalProperties << QStringLiteral("bypassDuplicate") << QStringLiteral("resource") << QStringLiteral("mlt_service") << QStringLiteral("audio_index")
                       << QStringLiteral("video_index") << QStringLiteral("mlt_type");
    QDomNodeList props;

    if (xml.tagName() == QLatin1String("producer")) {
        props = xml.childNodes();
    } else {
        props = xml.firstChildElement(QStringLiteral("producer")).childNodes();
    }
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().tagName() != QStringLiteral("property")) {
            continue;
        }
        QString propertyName = props.at(i).toElement().attribute(QStringLiteral("name"));
        if (!internalProperties.contains(propertyName) && !propertyName.startsWith(QLatin1Char('_'))) {
            value = props.at(i).firstChild().nodeValue();
            if (propertyName.startsWith(QLatin1String("kdenlive-force."))) {
                // this is a special forced property, pass it
                propertyName.remove(0, 15);
            }
            prod->set(propertyName.toUtf8().constData(), value.toUtf8().constData());
        }
    }
}
} // namespace

// static
std::shared_ptr<Mlt::Producer> LoadJob::loadResource(QString &resource, const QString &type)
{
    if (!resource.startsWith(type)) {
        resource.prepend(type);
    }
    return std::make_shared<Mlt::Producer>(pCore->getCurrentProfile()->profile(), nullptr, resource.toUtf8().constData());
}

std::shared_ptr<Mlt::Producer> LoadJob::loadPlaylist(QString &resource)
{
    std::unique_ptr<Mlt::Profile> xmlProfile(new Mlt::Profile());
    xmlProfile->set_explicit(0);
    std::unique_ptr<Mlt::Producer> producer(new Mlt::Producer(*xmlProfile, "xml", resource.toUtf8().constData()));
    if (!producer->is_valid()) {
        return nullptr;
    }
    if (pCore->getCurrentProfile()->isCompatible(xmlProfile.get())) {
        // We can use the "xml" producer since profile is the same (using it with different profiles corrupts the project.
        // Beware that "consumer" currently crashes on audio mixes!
        resource.prepend(QStringLiteral("xml:"));
    } else {
        // This is currently crashing so I guess we'd better reject it for now
        return nullptr;
        // path.prepend(QStringLiteral("consumer:"));
    }
    pCore->getCurrentProfile()->set_explicit(1);
    return std::make_shared<Mlt::Producer>(pCore->getCurrentProfile()->profile(), nullptr, resource.toUtf8().constData());
}

void LoadJob::checkProfile()
{
    // Check if clip profile matches
    QString service = m_producer->get("mlt_service");
    // Check for image producer
    if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
        // This is an image, create profile from image size
        int width = m_producer->get_int("meta.media.width");
        int height = m_producer->get_int("meta.media.height");
        if (width > 100 && height > 100) {
            std::unique_ptr<ProfileParam> projectProfile(new ProfileParam(pCore->getCurrentProfile().get()));
            projectProfile->m_width = width;
            projectProfile->m_height = height;
            projectProfile->m_sample_aspect_num = 1;
            projectProfile->m_sample_aspect_den = 1;
            projectProfile->m_display_aspect_num = width;
            projectProfile->m_display_aspect_den = height;
            projectProfile->m_description.clear();
            pCore->currentDoc()->switchProfile(projectProfile, m_clipId, m_xml);
        } else {
            // Very small image, we probably don't want to use this as profile
        }
    } else if (service.contains(QStringLiteral("avformat"))) {
        std::unique_ptr<Mlt::Profile> blankProfile(new Mlt::Profile());
        blankProfile->set_explicit(0);
        blankProfile->from_producer(*m_producer);
        std::unique_ptr<ProfileParam> clipProfile(new ProfileParam(blankProfile.get()));
        std::unique_ptr<ProfileParam> projectProfile(new ProfileParam(pCore->getCurrentProfile().get()));
        clipProfile->adjustWidth();
        if (clipProfile != projectProfile) {
            // Profiles do not match, propose profile adjustment
            pCore->currentDoc()->switchProfile(projectProfile, m_clipId, m_xml);
        } else if (KdenliveSettings::default_profile().isEmpty()) {
            // Confirm default project format
            KdenliveSettings::setDefault_profile(pCore->getCurrentProfile()->path());
        }
    }
}

void LoadJob::processSlideShow()
{
    int ttl = EffectsList::property(m_xml, QStringLiteral("ttl")).toInt();
    QString anim = EffectsList::property(m_xml, QStringLiteral("animation"));
    if (!anim.isEmpty()) {
        auto *filter = new Mlt::Filter(pCore->getCurrentProfile()->profile(), "affine");
        if ((filter != nullptr) && filter->is_valid()) {
            int cycle = ttl;
            QString geometry = SlideshowClip::animationToGeometry(anim, cycle);
            if (!geometry.isEmpty()) {
                if (anim.contains(QStringLiteral("low-pass"))) {
                    auto *blur = new Mlt::Filter(pCore->getCurrentProfile()->profile(), "boxblur");
                    if ((blur != nullptr) && blur->is_valid()) {
                        m_producer->attach(*blur);
                    }
                }
                filter->set("transition.geometry", geometry.toUtf8().data());
                filter->set("transition.cycle", cycle);
                m_producer->attach(*filter);
            }
        }
    }
    QString fade = EffectsList::property(m_xml, QStringLiteral("fade"));
    if (fade == QLatin1String("1")) {
        // user wants a fade effect to slideshow
        auto *filter = new Mlt::Filter(pCore->getCurrentProfile()->profile(), "luma");
        if ((filter != nullptr) && filter->is_valid()) {
            if (ttl != 0) {
                filter->set("cycle", ttl);
            }
            QString luma_duration = EffectsList::property(m_xml, QStringLiteral("luma_duration"));
            QString luma_file = EffectsList::property(m_xml, QStringLiteral("luma_file"));
            if (!luma_duration.isEmpty()) {
                filter->set("duration", luma_duration.toInt());
            }
            if (!luma_file.isEmpty()) {
                filter->set("luma.resource", luma_file.toUtf8().constData());
                QString softness = EffectsList::property(m_xml, QStringLiteral("softness"));
                if (!softness.isEmpty()) {
                    int soft = softness.toInt();
                    filter->set("luma.softness", (double)soft / 100.0);
                }
            }
            m_producer->attach(*filter);
        }
    }
    QString crop = EffectsList::property(m_xml, QStringLiteral("crop"));
    if (crop == QLatin1String("1")) {
        // user wants to center crop the slides
        auto *filter = new Mlt::Filter(pCore->getCurrentProfile()->profile(), "crop");
        if ((filter != nullptr) && filter->is_valid()) {
            filter->set("center", 1);
            m_producer->attach(*filter);
        }
    }
}
bool LoadJob::startJob()
{
    if (m_done) {
        return true;
    }
    m_resource = Xml::getXmlProperty(m_xml, QStringLiteral("resource"));
    ClipType type = static_cast<ClipType>(m_xml.attribute(QStringLiteral("type")).toInt());
    if (type == ClipType::Unknown) {
        type = getTypeForService(Xml::getXmlProperty(m_xml, QStringLiteral("mlt_service")), m_resource);
    }
    switch (type) {
    case ClipType::Color:
        m_producer = loadResource(m_resource, QStringLiteral("color:"));
        break;
    case ClipType::Text:
    case ClipType::TextTemplate:
        m_producer = loadResource(m_resource, QStringLiteral("kdenlivetitle:"));
        break;
    case ClipType::QText:
        m_producer = loadResource(m_resource, QStringLiteral("qtext:"));
        break;
    case ClipType::Playlist:
        m_producer = loadPlaylist(m_resource);
        break;
    case ClipType::SlideShow:
    default:
        m_producer = std::make_shared<Mlt::Producer>(pCore->getCurrentProfile()->profile(), nullptr, m_resource.toUtf8().constData());
        break;
    }
    if (!m_producer || m_producer->is_blank() || !m_producer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << " / / / / / / / / ERROR / / / / // CANNOT LOAD PRODUCER: " << m_resource;
        m_done = true;
        m_successful = false;
        m_errorMessage.append(i18n("ERROR: Could not load clip %1: producer is invalid", m_resource));
        return false;
    }
    processProducerProperties(m_producer, m_xml);
    QString clipName = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:clipname"));
    if (clipName.isEmpty()) {
        clipName = QFileInfo(Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:originalurl"))).fileName();
    }
    m_producer->set("kdenlive:clipname", clipName.toUtf8().constData());
    QString groupId = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:folderid"));
    if (!groupId.isEmpty()) {
        m_producer->set("kdenlive:folderid", groupId.toUtf8().constData());
    }
    int clipOut = 0, duration = 0;
    if (m_xml.hasAttribute(QStringLiteral("out"))) {
        clipOut = m_xml.attribute(QStringLiteral("out")).toInt();
    }
    // setup length here as otherwise default length (currently 15000 frames in MLT) will be taken even if outpoint is larger
    if (type == ClipType::Color || type == ClipType::Text || type == ClipType::TextTemplate || type == ClipType::QText || type == ClipType::Image ||
        type == ClipType::SlideShow) {
        int length;
        if (m_xml.hasAttribute(QStringLiteral("length"))) {
            length = m_xml.attribute(QStringLiteral("length")).toInt();
            clipOut = qMax(1, length - 1);
        } else {
            length = Xml::getXmlProperty(m_xml, QStringLiteral("length")).toInt();
            clipOut -= m_xml.attribute(QStringLiteral("in")).toInt();
            if (length < clipOut) {
                length = clipOut == 1 ? 1 : clipOut + 1;
            }
        }
        // Pass duration if it was forced
        if (m_xml.hasAttribute(QStringLiteral("duration"))) {
            duration = m_xml.attribute(QStringLiteral("duration")).toInt();
            if (length < duration) {
                length = duration;
                if (clipOut > 0) {
                    clipOut = length - 1;
                }
            }
        }
        if (duration == 0) {
            duration = length;
        }
        m_producer->set("length", length);
        int kdenlive_duration = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:duration")).toInt();
        m_producer->set("kdenlive:duration", kdenlive_duration > 0 ? kdenlive_duration : length);
    }
    if (clipOut > 0) {
        m_producer->set_in_and_out(m_xml.attribute(QStringLiteral("in")).toInt(), clipOut);
    }

    if (m_xml.hasAttribute(QStringLiteral("templatetext"))) {
        m_producer->set("templatetext", m_xml.attribute(QStringLiteral("templatetext")).toUtf8().constData());
    }
    duration = duration > 0 ? duration : m_producer->get_playtime();
    if (type == ClipType::SlideShow) {
        processSlideShow();
    }

    int vindex = -1;
    double fps = -1;
    const QString mltService = m_producer->get("mlt_service");
    if (mltService == QLatin1String("xml") || mltService == QLatin1String("consumer")) {
        // MLT playlist, create producer with blank profile to get real profile info
        QString tmpPath = m_resource;
        if (tmpPath.startsWith(QLatin1String("consumer:"))) {
            tmpPath = "xml:" + tmpPath.section(QLatin1Char(':'), 1);
        }
        Mlt::Profile original_profile;
        std::unique_ptr<Mlt::Producer> tmpProd(new Mlt::Producer(original_profile, nullptr, tmpPath.toUtf8().constData()));
        original_profile.set_explicit(1);
        double originalFps = original_profile.fps();
        fps = originalFps;
        if (originalFps > 0 && !qFuzzyCompare(originalFps, pCore->getCurrentFps())) {
            int originalLength = tmpProd->get_length();
            int fixedLength = (int)(originalLength * pCore->getCurrentFps() / originalFps);
            m_producer->set("length", fixedLength);
            m_producer->set("out", fixedLength - 1);
        }
    } else if (mltService == QLatin1String("avformat")) {
        // check if there are multiple streams
        vindex = m_producer->get_int("video_index");
        // List streams
        int streams = m_producer->get_int("meta.media.nb_streams");
        m_audio_list.clear();
        m_video_list.clear();
        for (int i = 0; i < streams; ++i) {
            QByteArray propertyName = QStringLiteral("meta.media.%1.stream.type").arg(i).toLocal8Bit();
            QString stype = m_producer->get(propertyName.data());
            if (stype == QLatin1String("audio")) {
                m_audio_list.append(i);
            } else if (stype == QLatin1String("video")) {
                m_video_list.append(i);
            }
        }

        if (vindex > -1) {
            char property[200];
            snprintf(property, sizeof(property), "meta.media.%d.stream.frame_rate", vindex);
            fps = m_producer->get_double(property);
        }

        if (fps <= 0) {
            if (m_producer->get_double("meta.media.frame_rate_den") > 0) {
                fps = m_producer->get_double("meta.media.frame_rate_num") / m_producer->get_double("meta.media.frame_rate_den");
            } else {
                fps = m_producer->get_double("source_fps");
            }
        }
    }
    if (fps <= 0 && type == ClipType::Unknown) {
        // something wrong, maybe audio file with embedded image
        QMimeDatabase db;
        QString mime = db.mimeTypeForFile(m_resource).name();
        if (mime.startsWith(QLatin1String("audio"))) {
            m_producer->set("video_index", -1);
            vindex = -1;
        }
    }
    m_done = m_successful = true;
    return true;
}

void LoadJob::processMultiStream()
{
    auto m_binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);

    // We retrieve the folder containing our clip, because we will set the other streams in the same
    auto parent = pCore->projectItemModel()->getRootFolder()->clipId();
    if (auto ptr = m_binClip->parentItem().lock()) {
        parent = std::static_pointer_cast<AbstractProjectItem>(ptr)->clipId();
    } else {
        qDebug() << "Warning, something went wrong while accessing parent of bin clip";
    }
    // This helper lambda request addition of a given stream
    auto addStream = [ this, parentId = std::move(parent) ](int vindex, int aindex, Fun &undo, Fun &redo)
    {
        auto clone = Clip::clone(m_producer);
        clone->set("video_index", vindex);
        clone->set("audio_index", aindex);
        QString id;

        pCore->projectItemModel()->requestAddBinClip(id, clone, parentId, undo, redo);
    };
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    if (KdenliveSettings::automultistreams()) {
        for (int i = 1; i < m_video_list.count(); ++i) {
            int vindex = m_video_list.at(i);
            int aindex = 0;
            if (i <= m_audio_list.count() - 1) {
                aindex = m_audio_list.at(i);
            }
            addStream(vindex, aindex, undo, redo);
        }
        return;
    }

    int width = 60.0 * pCore->getCurrentDar();
    if (width % 2 == 1) {
        width++;
    }

    QScopedPointer<QDialog> dialog(new QDialog(qApp->activeWindow()));
    dialog->setWindowTitle(QStringLiteral("Multi Stream Clip"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QWidget *mainWidget = new QWidget(dialog.data());
    auto *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    dialog->connect(buttonBox, &QDialogButtonBox::accepted, dialog.data(), &QDialog::accept);
    dialog->connect(buttonBox, &QDialogButtonBox::rejected, dialog.data(), &QDialog::reject);
    okButton->setText(i18n("Import selected clips"));

    QLabel *lab1 = new QLabel(i18n("Additional streams for clip\n %1", m_resource), mainWidget);
    mainLayout->addWidget(lab1);
    QList<QGroupBox *> groupList;
    QList<KComboBox *> comboList;
    // We start loading the list at 1, video index 0 should already be loaded
    for (int j = 1; j < m_video_list.count(); ++j) {
        m_producer->set("video_index", m_video_list.at(j));
        // TODO this keyframe should be cached
        QImage thumb = KThumb::getFrame(m_producer.get(), 0, width, 60);
        QGroupBox *streamFrame = new QGroupBox(i18n("Video stream %1", m_video_list.at(j)), mainWidget);
        mainLayout->addWidget(streamFrame);
        streamFrame->setProperty("vindex", m_video_list.at(j));
        groupList << streamFrame;
        streamFrame->setCheckable(true);
        streamFrame->setChecked(true);
        auto *vh = new QVBoxLayout(streamFrame);
        QLabel *iconLabel = new QLabel(mainWidget);
        mainLayout->addWidget(iconLabel);
        iconLabel->setPixmap(QPixmap::fromImage(thumb));
        vh->addWidget(iconLabel);
        if (m_audio_list.count() > 1) {
            auto *cb = new KComboBox(mainWidget);
            mainLayout->addWidget(cb);
            for (int k = 0; k < m_audio_list.count(); ++k) {
                cb->addItem(i18n("Audio stream %1", m_audio_list.at(k)), m_audio_list.at(k));
            }
            comboList << cb;
            cb->setCurrentIndex(qMin(j, m_audio_list.count() - 1));
            vh->addWidget(cb);
        }
        mainLayout->addWidget(streamFrame);
    }
    mainLayout->addWidget(buttonBox);
    if (dialog->exec() == QDialog::Accepted) {
        // import selected streams
        for (int i = 0; i < groupList.count(); ++i) {
            if (groupList.at(i)->isChecked()) {
                int vindex = groupList.at(i)->property("vindex").toInt();
                int ax = qMin(i, comboList.size() - 1);
                int aindex = -1;
                if (ax >= 0) {
                    // only check audio index if we have several audio streams
                    aindex = comboList.at(ax)->itemData(comboList.at(ax)->currentIndex()).toInt();
                }
                addStream(vindex, aindex, undo, redo);
            }
        }
    }
    pCore->pushUndo(undo, redo, i18n("Add additional streams for clip"));
}

bool LoadJob::commitResult(Fun &undo, Fun &redo)
{
    qDebug() << "################### loadjob COMMIT";
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    m_resultConsumed = true;
    auto m_binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    if (!m_successful) {
        pCore->projectItemModel()->requestBinClipDeletion(m_binClip, undo, redo);
        return false;
    }
    if (m_xml.hasAttribute(QStringLiteral("checkProfile")) && m_producer->get_int("video_index") > -1) {
        checkProfile();
    }
    if (m_video_list.size() > 1) {
        processMultiStream();
    }

    // note that the image is moved into lambda, it won't be available from this class anymore
    auto operation = [ clip = m_binClip, prod = std::move(m_producer) ]()
    {
        clip->setProducer(prod, true);
        return true;
    };
    auto reverse = []()
    {
        // This is probably not invertible, leave as is.
        return true;
    };
    bool ok = operation();
    if (ok) {
        if (pCore->projectItemModel()->clipsCount() == 2) {
            // Always select first added clip
            pCore->selectBinClip(m_clipId);
        }
        UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo);
    }
    return ok;
}
