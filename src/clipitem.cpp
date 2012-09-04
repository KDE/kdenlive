/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "clipitem.h"
#include "customtrackview.h"
#include "customtrackscene.h"
#include "renderer.h"
#include "docclipbase.h"
#include "transition.h"
#include "kdenlivesettings.h"
#include "kthumb.h"
#include "profilesdialog.h"
#ifdef USE_QJSON
#include "rotoscoping/rotowidget.h"
#endif

#include <KDebug>
#include <KIcon>

#include <QPainter>
#include <QTimer>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QMimeData>

static int FRAME_SIZE;

ClipItem::ClipItem(DocClipBase *clip, ItemInfo info, double fps, double speed, int strobe, int frame_width, bool generateThumbs) :
        AbstractClipItem(info, QRectF(), fps),
        m_clip(clip),
        m_startFade(0),
        m_endFade(0),
        m_audioOnly(false),
        m_videoOnly(false),
        m_startPix(QPixmap()),
        m_endPix(QPixmap()),
        m_hasThumbs(false),
        m_selectedEffect(-1),
        m_timeLine(0),
        m_startThumbRequested(false),
        m_endThumbRequested(false),
        //m_hover(false),
        m_speed(speed),
        m_strobe(strobe),
        m_framePixelWidth(0),
        m_limitedKeyFrames(false)
{
    setZValue(2);
    m_effectList = EffectsList(true);
    FRAME_SIZE = frame_width;
    setRect(0, 0, (info.endPos - info.startPos).frames(fps) - 0.02, (double) itemHeight());
    setPos(info.startPos.frames(fps), (double)(info.track * KdenliveSettings::trackheight()) + 1 + itemOffset());

    // set speed independant info
    if (m_speed <= 0 && m_speed > -1)
        m_speed = -1.0;
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);

    m_videoPix = KIcon("kdenlive-show-video").pixmap(QSize(16, 16));
    m_audioPix = KIcon("kdenlive-show-audio").pixmap(QSize(16, 16));

    if (m_speed == 1.0)
        m_clipName = m_clip->name();
    else
        m_clipName = m_clip->name() + " - " + QString::number(m_speed * 100, 'f', 0) + '%';

    m_producer = m_clip->getId();
    m_clipType = m_clip->clipType();
    //m_cropStart = info.cropStart;
    m_maxDuration = m_clip->maxDuration();
    setAcceptDrops(true);
    m_audioThumbReady = m_clip->audioThumbCreated();
    //setAcceptsHoverEvents(true);
    connect(this , SIGNAL(prepareAudioThumb(double, int, int, int)) , this, SLOT(slotPrepareAudioThumb(double, int, int, int)));

    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW || m_clipType == PLAYLIST) {
        m_baseColor = QColor(141, 166, 215);
        if (!m_clip->isPlaceHolder()) {
            m_hasThumbs = true;
            m_startThumbTimer.setSingleShot(true);
            connect(&m_startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
            m_endThumbTimer.setSingleShot(true);
            connect(&m_endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));
            connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QImage)), this, SLOT(slotThumbReady(int, QImage)));
            connect(m_clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
            if (generateThumbs) QTimer::singleShot(200, this, SLOT(slotFetchThumbs()));
        }

    } else if (m_clipType == COLOR) {
        QString colour = m_clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        m_baseColor = QColor(colour.left(7));
    } else if (m_clipType == IMAGE || m_clipType == TEXT) {
        m_baseColor = QColor(141, 166, 215);
        if (m_clipType == TEXT) {
            connect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QImage)), this, SLOT(slotThumbReady(int, QImage)));
        }
        //m_startPix = KThumb::getImage(KUrl(clip->getProperty("resource")), (int)(KdenliveSettings::trackheight() * KdenliveSettings::project_display_ratio()), KdenliveSettings::trackheight());
    } else if (m_clipType == AUDIO) {
        m_baseColor = QColor(141, 215, 166);
        connect(m_clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
}


ClipItem::~ClipItem()
{
    blockSignals(true);
    m_endThumbTimer.stop();
    m_startThumbTimer.stop();
    if (scene()) scene()->removeItem(this);
    if (m_clipType == VIDEO || m_clipType == AV || m_clipType == SLIDESHOW || m_clipType == PLAYLIST) {
        //disconnect(m_clip->thumbProducer(), SIGNAL(thumbReady(int, QImage)), this, SLOT(slotThumbReady(int, QImage)));
        //disconnect(m_clip, SIGNAL(gotAudioData()), this, SLOT(slotGotAudioData()));
    }
    delete m_timeLine;
}

ClipItem *ClipItem::clone(ItemInfo info) const
{
    ClipItem *duplicate = new ClipItem(m_clip, info, m_fps, m_speed, m_strobe, FRAME_SIZE);
    if (m_clipType == IMAGE || m_clipType == TEXT) duplicate->slotSetStartThumb(m_startPix);
    else if (m_clipType != COLOR) {
        if (info.cropStart == m_info.cropStart) duplicate->slotSetStartThumb(m_startPix);
        if (info.cropStart + (info.endPos - info.startPos) == m_info.cropStart + m_info.cropDuration) {
            duplicate->slotSetEndThumb(m_endPix);
        }
    }
    //kDebug() << "// CLoning clip: " << (info.cropStart + (info.endPos - info.startPos)).frames(m_fps) << ", CURRENT end: " << (cropStart() + duration()).frames(m_fps);
    duplicate->setEffectList(m_effectList);
    duplicate->setVideoOnly(m_videoOnly);
    duplicate->setAudioOnly(m_audioOnly);
    duplicate->setFades(fadeIn(), fadeOut());
    //duplicate->setSpeed(m_speed);
    return duplicate;
}

void ClipItem::setEffectList(const EffectsList effectList)
{
    m_effectList.clone(effectList);
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (!m_effectList.isEmpty()) {
        for (int i = 0; i < m_effectList.count(); i++) {
	    QDomElement effect = m_effectList.at(i);
            QString effectId = effect.attribute("id");
            // check if it is a fade effect
            QDomNodeList params = effect.elementsByTagName("parameter");
            int fade = 0;
            for (int j = 0; j < params.count(); j++) {
                QDomElement e = params.item(j).toElement();
                if (!e.isNull()) {
                    if (effectId == "fadein") {
                        if (m_effectList.hasEffect(QString(), "fade_from_black") == -1) {
                            if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                            else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                        } else {
                            QDomElement fadein = m_effectList.getEffectByTag(QString(), "fade_from_black");
                            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
                            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
                        }
                    } else if (effectId == "fade_from_black") {
                        if (m_effectList.hasEffect(QString(), "fadein") == -1) {
                            if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                            else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                        } else {
                            QDomElement fadein = m_effectList.getEffectByTag(QString(), "fadein");
                            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
                            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
                        }
                    } else if (effectId == "fadeout") {
                        if (m_effectList.hasEffect(QString(), "fade_to_black") == -1) {
                            if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                            else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                        } else {
                            QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
                            if (fadeout.attribute("name") == "out") fade += fadeout.attribute("value").toInt();
                            else if (fadeout.attribute("name") == "in") fade -= fadeout.attribute("value").toInt();
                        }
                    } else if (effectId == "fade_to_black") {
                        if (m_effectList.hasEffect(QString(), "fadeout") == -1) {
                            if (e.attribute("name") == "out") fade += e.attribute("value").toInt();
                            else if (e.attribute("name") == "in") fade -= e.attribute("value").toInt();
                        } else {
                            QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
                            if (fadeout.attribute("name") == "out") fade += fadeout.attribute("value").toInt();
                            else if (fadeout.attribute("name") == "in") fade -= fadeout.attribute("value").toInt();
                        }
                    }
                }
            }
            if (fade > 0)
                m_startFade = fade;
            else if (fade < 0)
                m_endFade = -fade;
        }
        setSelectedEffect(0);
    }
}

const EffectsList ClipItem::effectList() const
{
    return m_effectList;
}

int ClipItem::selectedEffectIndex() const
{
    return m_selectedEffect;
}

void ClipItem::initEffect(QDomElement effect, int diff, int offset)
{
    // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
    // not be changed

    if (effect.attribute("id") == "freeze" && diff > 0) {
        EffectsList::setParameter(effect, "frame", QString::number(diff));
    }

    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();

        if (e.isNull())
            continue;

        // Check if this effect has a variable parameter
        if (e.attribute("default").contains('%')) {
            double evaluatedValue = ProfilesDialog::getStringEval(projectScene()->profile(), e.attribute("default"));
            e.setAttribute("default", evaluatedValue);
            if (e.hasAttribute("value") && e.attribute("value").startsWith('%')) {
                e.setAttribute("value", evaluatedValue);
            }
        }

        if (effect.attribute("id") == "crop") {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute("name") == "use_profile" && !(m_clip->getProperty("proxy").isEmpty() || m_clip->getProperty("proxy") == "-"))
                e.setAttribute("value", "1");
        }

        if (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe") {
	    if (e.attribute("keyframes").isEmpty()) {
		// Effect has a keyframe type parameter, we need to set the values
		e.setAttribute("keyframes", QString::number(cropStart().frames(m_fps)) + ':' + e.attribute("default"));
	    }
	    else if (offset != 0) {
		// adjust keyframes to this clip
		QString adjusted = adjustKeyframes(e.attribute("keyframes"), offset - cropStart().frames(m_fps));
		e.setAttribute("keyframes", adjusted);
	    }
        }

        if (e.attribute("type") == "geometry" && !e.hasAttribute("fixed")) {
            // Effects with a geometry parameter need to sync in / out with parent clip
	    effect.setAttribute("in", QString::number(cropStart().frames(m_fps)));
	    effect.setAttribute("out", QString::number((cropStart() + cropDuration()).frames(m_fps) - 1));
	    effect.setAttribute("_sync_in_out", "1");
	}
    }
    if (effect.attribute("tag") == "volume" || effect.attribute("tag") == "brightness") {
        if (effect.attribute("id") == "fadeout" || effect.attribute("id") == "fade_to_black") {
            int end = (cropDuration() + cropStart()).frames(m_fps) - 1;
            int start = end;
            if (effect.attribute("id") == "fadeout") {
                if (m_effectList.hasEffect(QString(), "fade_to_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            } else if (effect.attribute("id") == "fade_to_black") {
                if (m_effectList.hasEffect(QString(), "fadeout") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        } else if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fade_from_black") {
            int start = cropStart().frames(m_fps);
            int end = start;
            if (effect.attribute("id") == "fadein") {
                if (m_effectList.hasEffect(QString(), "fade_from_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
		    if (offset != 0) effectDuration -= offset;
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    end += effectDuration;
                } else
                    end += EffectsList::parameter(m_effectList.getEffectByTag(QString(), "fade_from_black"), "out").toInt() - offset;
            } else if (effect.attribute("id") == "fade_from_black") {
                if (m_effectList.hasEffect(QString(), "fadein") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
		    if (offset != 0) effectDuration -= offset;
                    if (effectDuration > cropDuration().frames(m_fps)) {
                        effectDuration = cropDuration().frames(m_fps) / 2;
                    }
                    end += effectDuration;
                } else
                    end += EffectsList::parameter(m_effectList.getEffectByTag(QString(), "fadein"), "out").toInt() - offset;
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        }
    }
}

const QString ClipItem::adjustKeyframes(QString keyframes, int offset)
{
    QStringList result;
    // Simple keyframes
    const QStringList list = keyframes.split(';', QString::SkipEmptyParts);
    foreach(const QString &keyframe, list) {
	int pos = keyframe.section(':', 0, 0).toInt() - offset;
	QString newKey = QString::number(pos) + ":" + keyframe.section(':', 1);
	result.append(newKey);
    }
    return result.join(";");
}

bool ClipItem::checkKeyFrames()
{
    bool clipEffectsModified = false;
    QLocale locale;
    // go through all effects this clip has
    for (int ix = 0; ix < m_effectList.count(); ++ix) {
        QStringList keyframeParams = keyframes(ix);
        QStringList newKeyFrameParams;
        bool effModified = false;

        // go through all params which have keyframes
        foreach(const QString &kfr, keyframeParams) {
            const QStringList keyframes = kfr.split(';', QString::SkipEmptyParts);
            QStringList newKeyFrames;
            bool cutKeyFrame = false;
            bool modified = false;
            int lastPos = -1;
            double lastValue = -1;
            int start = cropStart().frames(m_fps);
            int end = (cropStart() + cropDuration()).frames(m_fps);

            // go through all keyframes for one param
            foreach(const QString &str, keyframes) {
                int pos = str.section(':', 0, 0).toInt();
                double val = locale.toDouble(str.section(':', 1, 1));
                if (pos - start < 0) {
                    // a keyframe is defined before the start of the clip
                    cutKeyFrame = true;
                } else if (cutKeyFrame) {
                    // create new keyframe at clip start, calculate interpolated value
                    if (pos > start) {
                        int diff = pos - lastPos;
                        double ratio = (double)(start - lastPos) / diff;
                        double newValue = lastValue + (val - lastValue) * ratio;
                        newKeyFrames.append(QString::number(start) + ':' + locale.toString(newValue));
                        modified = true;
                    }
                    cutKeyFrame = false;
                }
                if (!cutKeyFrame) {
                    if (pos > end) {
                        // create new keyframe at clip end, calculate interpolated value
                        int diff = pos - lastPos;
                        if (diff != 0) {
                            double ratio = (double)(end - lastPos) / diff;
                            double newValue = lastValue + (val - lastValue) * ratio;
                            newKeyFrames.append(QString::number(end) + ':' + locale.toString(newValue));
                            modified = true;
                        }
                        break;
                    } else {
                        newKeyFrames.append(QString::number(pos) + ':' + locale.toString(val));
                    }
                }
                lastPos = pos;
                lastValue = val;
            }

            newKeyFrameParams.append(newKeyFrames.join(";"));
            if (modified)
                effModified = true;
        }

        if (effModified) {
            // update KeyFrames
            setKeyframes(ix, newKeyFrameParams);
            clipEffectsModified = true;
        }
    }
    return clipEffectsModified;
}

void ClipItem::setKeyframes(const int ix, const QStringList keyframes)
{
    QDomElement effect = m_effectList.at(ix);
    if (effect.attribute("disable") == "1") return;
    QLocale locale;
    QDomNodeList params = effect.elementsByTagName("parameter");
    int keyframeParams = 0;
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe") && e.attribute("intimeline") == "1") {
            e.setAttribute("keyframes", keyframes.at(keyframeParams));
            if (ix == m_selectedEffect && keyframeParams == 0) {
                m_keyframes.clear();
                m_visibleParam = i;
                double max = locale.toDouble(e.attribute("max"));
                double min = locale.toDouble(e.attribute("min"));
                m_keyframeFactor = 100.0 / (max - min);
                m_keyframeOffset = min;
                m_keyframeDefault = locale.toDouble(e.attribute("default"));
                m_selectedKeyframe = 0;
                // parse keyframes
                const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                foreach(const QString &str, keyframes) {
                    int pos = str.section(':', 0, 0).toInt();
                    double val = locale.toDouble(str.section(':', 1, 1));
                    m_keyframes[pos] = val;
                }
                if (m_keyframes.find(m_editedKeyframe) == m_keyframes.end()) m_editedKeyframe = -1;
                if (m_keyframes.find(m_editedKeyframe) == m_keyframes.end()) m_editedKeyframe = -1;
                update();
            }
            ++keyframeParams;
        }
    }
}


void ClipItem::setSelectedEffect(const int ix)
{
    m_selectedEffect = ix;
    QLocale locale;
    QDomElement effect = effectAtIndex(m_selectedEffect);
    if (!effect.isNull() && effect.attribute("disable") != "1") {
        QDomNodeList params = effect.elementsByTagName("parameter");
        for (int i = 0; i < params.count(); i++) {
            QDomElement e = params.item(i).toElement();
            if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe") && e.attribute("intimeline") == "1") {
                m_keyframes.clear();
                m_limitedKeyFrames = e.attribute("type") == "keyframe";
                m_visibleParam = i;
                double max = locale.toDouble(e.attribute("max"));
                double min = locale.toDouble(e.attribute("min"));
                m_keyframeFactor = 100.0 / (max - min);
                m_keyframeOffset = min;
                m_keyframeDefault = locale.toDouble(e.attribute("default"));
                m_selectedKeyframe = 0;

                // parse keyframes
                const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                foreach(const QString &str, keyframes) {
                    int pos = str.section(':', 0, 0).toInt();
                    double val = locale.toDouble(str.section(':', 1, 1));
                    m_keyframes[pos] = val;
                }
                if (m_keyframes.find(m_editedKeyframe) == m_keyframes.end())
                    m_editedKeyframe = -1;
                update();
                return;
            }
        }
    }

    if (!m_keyframes.isEmpty()) {
        m_keyframes.clear();
        update();
    }
}

QStringList ClipItem::keyframes(const int index)
{
    QStringList result;
    QDomElement effect = m_effectList.at(index);
    QDomNodeList params = effect.elementsByTagName("parameter");

    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe"))
            result.append(e.attribute("keyframes"));
    }
    return result;
}

void ClipItem::updateKeyframeEffect()
{
    // regenerate xml parameter from the clip keyframes
    QDomElement effect = getEffectAtIndex(m_selectedEffect);
    if (effect.attribute("disable") == "1") return;
    QDomNodeList params = effect.elementsByTagName("parameter");
    QDomElement e = params.item(m_visibleParam).toElement();

    if (!e.isNull()) {
        QString keyframes;
        if (m_keyframes.count() > 0) {
            QMap<int, int>::const_iterator i = m_keyframes.constBegin();
            while (i != m_keyframes.constEnd()) {
                keyframes.append(QString::number(i.key()) + ':' + QString::number(i.value()) + ';');
                ++i;
            }
        }
        // Effect has a keyframe type parameter, we need to set the values
        e.setAttribute("keyframes", keyframes);
    }
}

QDomElement ClipItem::selectedEffect()
{
    if (m_selectedEffect == -1 || m_effectList.isEmpty()) return QDomElement();
    return effectAtIndex(m_selectedEffect);
}

void ClipItem::resetThumbs(bool clearExistingThumbs)
{
    if (clearExistingThumbs) {
        m_startPix = QPixmap();
        m_endPix = QPixmap();
        m_audioThumbCachePic.clear();
    }
    slotFetchThumbs();
}


void ClipItem::refreshClip(bool checkDuration, bool forceResetThumbs)
{
    if (checkDuration && (m_maxDuration != m_clip->maxDuration())) {
        m_maxDuration = m_clip->maxDuration();
        if (m_clipType != IMAGE && m_clipType != TEXT && m_clipType != COLOR) {
            if (m_maxDuration != GenTime() && m_info.cropStart + m_info.cropDuration > m_maxDuration) {
                // Clip duration changed, make sure to stay in correct range
                if (m_info.cropStart > m_maxDuration) {
                    m_info.cropStart = GenTime();
                    m_info.cropDuration = qMin(m_info.cropDuration, m_maxDuration);
                } else {
                    m_info.cropDuration = m_maxDuration;
                }
                updateRectGeometry();
            }
        }
    }
    if (m_clipType == COLOR) {
        QString colour = m_clip->getProperty("colour");
        colour = colour.replace(0, 2, "#");
        m_baseColor = QColor(colour.left(7));
        update();
    } else resetThumbs(forceResetThumbs);
}

void ClipItem::slotFetchThumbs()
{
    if (scene() == NULL || m_clipType == AUDIO || m_clipType == COLOR) return;
    if (m_clipType == IMAGE) {
        if (m_startPix.isNull()) {
            m_startPix = KThumb::getImage(KUrl(m_clip->getProperty("resource")), (int)(KdenliveSettings::trackheight() * KdenliveSettings::project_display_ratio()), KdenliveSettings::trackheight());
            update();
        }
        return;
    }

    if (m_clipType == TEXT) {
        if (m_startPix.isNull()) slotGetStartThumb();
        return;
    }

    QList <int> frames;
    if (m_startPix.isNull()) {
        m_startThumbRequested = true;
        frames.append((int)m_speedIndependantInfo.cropStart.frames(m_fps));
    }

    if (m_endPix.isNull()) {
        m_endThumbRequested = true;
        frames.append((int)(m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1);
    }

    if (!frames.isEmpty()) m_clip->slotExtractImage(frames);
}

void ClipItem::stopThumbs()
{
    // Clip is about to be deleted, make sure we don't request thumbnails
    disconnect(&m_startThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetStartThumb()));
    disconnect(&m_endThumbTimer, SIGNAL(timeout()), this, SLOT(slotGetEndThumb()));
}

void ClipItem::slotGetStartThumb()
{
    m_startThumbRequested = true;
    m_clip->slotExtractImage(QList<int>() << (int)m_speedIndependantInfo.cropStart.frames(m_fps));
}

void ClipItem::slotGetEndThumb()
{
    m_endThumbRequested = true;
    m_clip->slotExtractImage(QList<int>() << (int)(m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1);
}


void ClipItem::slotSetStartThumb(QImage img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_startPix = pix;
        QRectF r = sceneBoundingRect();
        r.setRight(pix.width() + 2);
        update(r);
    }
}

void ClipItem::slotSetEndThumb(QImage img)
{
    if (!img.isNull() && img.format() == QImage::Format_ARGB32) {
        QPixmap pix = QPixmap::fromImage(img);
        m_endPix = pix;
        QRectF r = sceneBoundingRect();
        r.setLeft(r.right() - pix.width() - 2);
        update(r);
    }
}

void ClipItem::slotThumbReady(int frame, QImage img)
{
    if (scene() == NULL) return;
    QRectF r = boundingRect();
    QPixmap pix = QPixmap::fromImage(img);
    double width = pix.width() / projectScene()->scale().x();
    if (m_startThumbRequested && frame == m_speedIndependantInfo.cropStart.frames(m_fps)) {
        m_startPix = pix;
        m_startThumbRequested = false;
        update(r.left(), r.top(), width, pix.height());
        if (m_clipType == IMAGE || m_clipType == TEXT) {
            update(r.right() - width, r.top(), width, pix.height());
        }
    } else if (m_endThumbRequested && frame == (m_speedIndependantInfo.cropStart + m_speedIndependantInfo.cropDuration).frames(m_fps) - 1) {
        m_endPix = pix;
        m_endThumbRequested = false;
        update(r.right() - width, r.top(), width, pix.height());
    }
}

void ClipItem::slotSetStartThumb(const QPixmap pix)
{
    m_startPix = pix;
}

void ClipItem::slotSetEndThumb(const QPixmap pix)
{
    m_endPix = pix;
}

QPixmap ClipItem::startThumb() const
{
    return m_startPix;
}

QPixmap ClipItem::endThumb() const
{
    return m_endPix;
}

void ClipItem::slotGotAudioData()
{
    m_audioThumbReady = true;
    if (m_clipType == AV && !isAudioOnly()) {
        QRectF r = boundingRect();
        r.setTop(r.top() + r.height() / 2 - 1);
        update(r);
    } else update();
}

int ClipItem::type() const
{
    return AVWIDGET;
}

DocClipBase *ClipItem::baseClip() const
{
    return m_clip;
}

QDomElement ClipItem::xml() const
{
    return itemXml();
}

QDomElement ClipItem::itemXml() const
{
    QDomElement xml = m_clip->toXML();
    if (m_speed != 1.0) xml.setAttribute("speed", m_speed);
    if (m_strobe > 1) xml.setAttribute("strobe", m_strobe);
    if (m_audioOnly) xml.setAttribute("audio_only", 1);
    else if (m_videoOnly) xml.setAttribute("video_only", 1);
    return xml;
}

int ClipItem::clipType() const
{
    return m_clipType;
}

QString ClipItem::clipName() const
{
    return m_clipName;
}

void ClipItem::setClipName(const QString &name)
{
    m_clipName = name;
}

const QString ClipItem::clipProducer() const
{
    return m_producer;
}

void ClipItem::flashClip()
{
    if (m_timeLine == 0) {
        m_timeLine = new QTimeLine(750, this);
        m_timeLine->setUpdateInterval(80);
        m_timeLine->setCurveShape(QTimeLine::EaseInOutCurve);
        m_timeLine->setFrameRange(0, 100);
        connect(m_timeLine, SIGNAL(valueChanged(qreal)), this, SLOT(animate(qreal)));
    }
    //m_timeLine->start();
}

void ClipItem::animate(qreal /*value*/)
{
    QRectF r = boundingRect();
    r.setHeight(20);
    update(r);
}

// virtual
void ClipItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *)
{
    QColor paintColor;
    QPen framePen;
    if (parentItem()) paintColor = QColor(255, 248, 149);
    else paintColor = m_baseColor;
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        paintColor = paintColor.darker();
        framePen.setColor(Qt::red);
        framePen.setWidthF(2.0);
    }
    else {
        framePen.setColor(paintColor.darker());
    }

    const QRectF exposed = option->exposedRect;
    painter->setClipRect(exposed);
    painter->fillRect(exposed, paintColor);
    painter->setWorldMatrixEnabled(false);;
    const QRectF mapped = painter->worldTransform().mapRect(rect());
    // draw thumbnails
    if (KdenliveSettings::videothumbnails() && !isAudioOnly()) {
        QPen pen = painter->pen();
        pen.setColor(QColor(255, 255, 255, 150));
        painter->setPen(pen);
        if ((m_clipType == IMAGE || m_clipType == TEXT) && !m_startPix.isNull()) {
            const QPointF top = mapped.topRight() - QPointF(m_startPix.width() - 1, 0);
            painter->drawPixmap(top, m_startPix);
            QLineF l2(top.x(), mapped.top(), top.x(), mapped.bottom());
            painter->drawLine(l2);
        } else if (!m_endPix.isNull()) {
            const QPointF top = mapped.topRight() - QPointF(m_endPix.width() - 1, 0);
            painter->drawPixmap(top, m_endPix);
            QLineF l2(top.x(), mapped.top(), top.x(), mapped.bottom());
            painter->drawLine(l2);
        }
        if (!m_startPix.isNull()) {
            painter->drawPixmap(mapped.topLeft(), m_startPix);
            QLineF l2(mapped.left() + m_startPix.width(), mapped.top(), mapped.left() + m_startPix.width(), mapped.bottom());
            painter->drawLine(l2);
        }

        // if we are in full zoom, paint thumbnail for every frame
        if (m_clip->thumbProducer() && clipType() != COLOR && clipType() != AUDIO && !m_audioOnly && painter->worldTransform().m11() == FRAME_SIZE) {
            int offset = (m_info.startPos - m_info.cropStart).frames(m_fps);
            int left = qMax((int) m_info.cropStart.frames(m_fps) + 1, (int) mapToScene(exposed.left(), 0).x() - offset);
            int right = qMin((int)(m_info.cropStart + m_info.cropDuration).frames(m_fps) - 1, (int) mapToScene(exposed.right(), 0).x() - offset);
            QPointF startPos = mapped.topLeft();
            int startOffset = m_info.cropStart.frames(m_fps);
            if (clipType() == IMAGE || clipType() == TEXT) {
                for (int i = left; i <= right; i++) {
                    painter->drawPixmap(startPos + QPointF(FRAME_SIZE *(i - startOffset), 0), m_startPix);
                }
            }
            else {
#if KDE_IS_VERSION(4,5,0)
                if (m_clip && m_clip->thumbProducer()) {
                    QString path = m_clip->fileURL().path() + '_';
                    QImage img;
                    QPen pen(Qt::white);
                    pen.setStyle(Qt::DotLine);
                    painter->setPen(pen);
                    QList <int> missing;
                    for (int i = left; i <= right; i++) {
                        img = m_clip->thumbProducer()->findCachedThumb(path + QString::number(i));
                        QPointF xpos = startPos + QPointF(FRAME_SIZE *(i - startOffset), 0);
                        if (img.isNull()) missing << i;
                        else painter->drawImage(xpos, img);
                        painter->drawLine(xpos, xpos + QPointF(0, mapped.height()));
                    }
                    if (!missing.isEmpty()) {
                        m_clip->thumbProducer()->queryIntraThumbs(missing);
                        connect(m_clip->thumbProducer(), SIGNAL(thumbsCached()), this, SLOT(slotGotThumbsCache()));
                    }
                }
#endif
            }
        }
        painter->setPen(Qt::black);
    }

    // draw audio thumbnails
    if (KdenliveSettings::audiothumbnails() && m_speed == 1.0 && !isVideoOnly() && ((m_clipType == AV && (exposed.bottom() > (rect().height() / 2) || isAudioOnly())) || m_clipType == AUDIO) && m_audioThumbReady) {

        double startpixel = exposed.left();
        if (startpixel < 0)
            startpixel = 0;
        double endpixel = exposed.right();
        if (endpixel < 0)
            endpixel = 0;
        //kDebug()<<"///  REPAINTING AUDIO THMBS ZONE: "<<startpixel<<"x"<<endpixel;

        /*QPainterPath path = m_clipType == AV ? roundRectPathLower : resultClipPath;*/
        QRectF mappedRect;
        if (m_clipType == AV && !isAudioOnly()) {
            mappedRect = mapped;
            mappedRect.setTop(mappedRect.bottom() - mapped.height() / 2);
        } else mappedRect = mapped;

        double scale = painter->worldTransform().m11();
        int channels = 0;
        if (isEnabled() && m_clip) channels = m_clip->getProperty("channels").toInt();
        if (scale != m_framePixelWidth)
            m_audioThumbCachePic.clear();
        double cropLeft = m_info.cropStart.frames(m_fps);
        const int clipStart = mappedRect.x();
        const int mappedStartPixel =  painter->worldTransform().map(QPointF(startpixel + cropLeft, 0)).x() - clipStart;
        const int mappedEndPixel =  painter->worldTransform().map(QPointF(endpixel + cropLeft, 0)).x() - clipStart;
        cropLeft = cropLeft * scale;

        if (channels >= 1) {
            emit prepareAudioThumb(scale, mappedStartPixel, mappedEndPixel, channels);
        }

        for (int startCache = mappedStartPixel - (mappedStartPixel) % 100; startCache < mappedEndPixel; startCache += 100) {
            if (m_audioThumbCachePic.contains(startCache) && !m_audioThumbCachePic[startCache].isNull())
                painter->drawPixmap(clipStart + startCache - cropLeft, mappedRect.y(),  m_audioThumbCachePic[startCache]);
        }
    }

    // only paint details if clip is big enough
    if (mapped.width() > 20) {

        // Draw effects names
        if (!m_effectNames.isEmpty() && mapped.width() > 40) {
            QRectF txtBounding = painter->boundingRect(mapped, Qt::AlignLeft | Qt::AlignTop, m_effectNames);
            QColor bgColor;
            if (m_timeLine && m_timeLine->state() == QTimeLine::Running) {
                qreal value = m_timeLine->currentValue();
                txtBounding.setWidth(txtBounding.width() * value);
                bgColor.setRgb(50 + 200 *(1.0 - value), 50, 50, 100 + 50 * value);
            } else bgColor.setRgb(50, 50, 90, 180);

            QPainterPath rounded;
            rounded.moveTo(txtBounding.bottomRight());
            rounded.arcTo(txtBounding.right() - txtBounding.height() - 2, txtBounding.top() - txtBounding.height(), txtBounding.height() * 2, txtBounding.height() * 2, 270, 90);
            rounded.lineTo(txtBounding.topLeft());
            rounded.lineTo(txtBounding.bottomLeft());
            painter->fillPath(rounded, bgColor);
            painter->setPen(Qt::lightGray);
            painter->drawText(txtBounding.adjusted(1, 0, 1, 0), Qt::AlignCenter, m_effectNames);
        }

        const QRectF txtBounding2 = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, ' ' + m_clipName + ' ');
        painter->setBrush(framePen.color());
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(txtBounding2, 3, 3);
        painter->setBrush(QBrush(Qt::NoBrush));

        if (m_videoOnly) {
            painter->drawPixmap(txtBounding2.topLeft() - QPointF(17, -1), m_videoPix);
        } else if (m_audioOnly) {
            painter->drawPixmap(txtBounding2.topLeft() - QPointF(17, -1), m_audioPix);
        }
        painter->setPen(Qt::white);
        painter->drawText(txtBounding2, Qt::AlignCenter, m_clipName);


        // draw markers
        if (isEnabled() && m_clip) {
            QList < CommentedTime > markers = m_clip->commentedSnapMarkers();
            QList < CommentedTime >::Iterator it = markers.begin();
            GenTime pos;
            double framepos;
            QBrush markerBrush(QColor(120, 120, 0, 140));
            QPen pen = painter->pen();
            pen.setColor(QColor(255, 255, 255, 200));
            pen.setStyle(Qt::DotLine);

            for (; it != markers.end(); ++it) {
                pos = GenTime((int)((*it).time().frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
                if (pos > GenTime()) {
                    if (pos > cropDuration()) break;
                    QLineF l(rect().x() + pos.frames(m_fps), rect().y(), rect().x() + pos.frames(m_fps), rect().bottom());
                    QLineF l2 = painter->worldTransform().map(l);
                    painter->setPen(pen);
                    painter->drawLine(l2);
                    if (KdenliveSettings::showmarkers()) {
                        framepos = rect().x() + pos.frames(m_fps);
                        const QRectF r1(framepos + 0.04, 10, rect().width() - framepos - 2, rect().height() - 10);
                        const QRectF r2 = painter->worldTransform().mapRect(r1);
                        const QRectF txtBounding3 = painter->boundingRect(r2, Qt::AlignLeft | Qt::AlignTop, ' ' + (*it).comment() + ' ');
                        painter->setBrush(markerBrush);
                        painter->setPen(Qt::NoPen);
                        painter->drawRoundedRect(txtBounding3, 3, 3);
                        painter->setBrush(QBrush(Qt::NoBrush));
                        painter->setPen(Qt::white);
                        painter->drawText(txtBounding3, Qt::AlignCenter, (*it).comment());
                    }
                    //painter->fillRect(QRect(br.x() + framepos, br.y(), 10, br.height()), QBrush(QColor(0, 0, 0, 150)));
                }
            }
        }

        // draw start / end fades
        QBrush fades;
        if (isSelected()) {
            fades = QBrush(QColor(200, 50, 50, 150));
        } else fades = QBrush(QColor(200, 200, 200, 200));

        if (m_startFade != 0) {
            QPainterPath fadeInPath;
            fadeInPath.moveTo(0, 0);
            fadeInPath.lineTo(0, rect().height());
            fadeInPath.lineTo(m_startFade, 0);
            fadeInPath.closeSubpath();
            QPainterPath f1 = painter->worldTransform().map(fadeInPath);
            painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
            /*if (isSelected()) {
                QLineF l(m_startFade * scale, 0, 0, itemHeight);
                painter->drawLine(l);
            }*/
        }
        if (m_endFade != 0) {
            QPainterPath fadeOutPath;
            fadeOutPath.moveTo(rect().width(), 0);
            fadeOutPath.lineTo(rect().width(), rect().height());
            fadeOutPath.lineTo(rect().width() - m_endFade, 0);
            fadeOutPath.closeSubpath();
            QPainterPath f1 = painter->worldTransform().map(fadeOutPath);
            painter->fillPath(f1/*.intersected(resultClipPath)*/, fades);
            /*if (isSelected()) {
                QLineF l(itemWidth - m_endFade * scale, 0, itemWidth, itemHeight);
                painter->drawLine(l);
            }*/
        }


        painter->setPen(QPen(Qt::lightGray));
        // draw effect or transition keyframes
        drawKeyFrames(painter, m_limitedKeyFrames);
    }
    
    // draw clip border
    // expand clip rect to allow correct painting of clip border
    painter->setClipping(false);
    painter->setPen(framePen);
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        painter->drawRect(mapped.adjusted(0.5, 0.5, -0.5, -0.5));
    }
    else {
        painter->drawRect(mapped.adjusted(0, 0, -0.5, 0));
    }
}


OPERATIONTYPE ClipItem::operationMode(QPointF pos)
{
    if (isItemLocked()) return NONE;
    const double scale = projectScene()->scale().x();
    double maximumOffset = 6 / scale;
    if (isSelected() || (parentItem() && parentItem()->isSelected())) {
        int kf = mouseOverKeyFrames(pos, maximumOffset);
        if (kf != -1) {
            m_editedKeyframe = kf;
            return KEYFRAME;
        }
    }
    QRectF rect = sceneBoundingRect();
    int addtransitionOffset = 10;
    // Don't allow add transition if track height is very small
    if (rect.height() < 30) addtransitionOffset = 0;

    if (qAbs((int)(pos.x() - (rect.x() + m_startFade))) < maximumOffset  && qAbs((int)(pos.y() - rect.y())) < 6) {
        return FADEIN;
    } else if ((pos.x() <= rect.x() + rect.width() / 2) && pos.x() - rect.x() < maximumOffset && (rect.bottom() - pos.y() > addtransitionOffset)) {
        return RESIZESTART;
    } else if (qAbs((int)(pos.x() - (rect.x() + rect.width() - m_endFade))) < maximumOffset && qAbs((int)(pos.y() - rect.y())) < 6) {
        return FADEOUT;
    } else if ((pos.x() >= rect.x() + rect.width() / 2) && (rect.right() - pos.x() < maximumOffset) && (rect.bottom() - pos.y() > addtransitionOffset)) {
        return RESIZEEND;
    } else if ((pos.x() - rect.x() < 16 / scale) && (rect.bottom() - pos.y() <= addtransitionOffset)) {
        return TRANSITIONSTART;
    } else if ((rect.right() - pos.x() < 16 / scale) && (rect.bottom() - pos.y() <= addtransitionOffset)) {
        return TRANSITIONEND;
    }

    return MOVE;
}

int ClipItem::itemHeight()
{
    return KdenliveSettings::trackheight() - 2;
}

void ClipItem::resetFrameWidth(int width)
{
    FRAME_SIZE = width;
    update();
}

QList <GenTime> ClipItem::snapMarkers() const
{
    QList < GenTime > snaps;
    if (!m_clip) return snaps;
    QList < GenTime > markers = m_clip->snapMarkers();
    GenTime pos;

    for (int i = 0; i < markers.size(); i++) {
        pos = GenTime((int)(markers.at(i).frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) break;
            else snaps.append(pos + startPos());
        }
    }
    return snaps;
}

QList <CommentedTime> ClipItem::commentedSnapMarkers() const
{
    QList < CommentedTime > snaps;
    if (!m_clip) return snaps;
    QList < CommentedTime > markers = m_clip->commentedSnapMarkers();
    GenTime pos;

    for (int i = 0; i < markers.size(); i++) {
        pos = GenTime((int)(markers.at(i).time().frames(m_fps) / qAbs(m_speed) + 0.5), m_fps) - cropStart();
        if (pos > GenTime()) {
            if (pos > cropDuration()) break;
            else snaps.append(CommentedTime(pos + startPos(), markers.at(i).comment()));
        }
    }
    return snaps;
}

void ClipItem::slotPrepareAudioThumb(double pixelForOneFrame, int startpixel, int endpixel, int channels)
{
    QRectF re =  sceneBoundingRect();
    if (m_clipType == AV && !isAudioOnly()) re.setTop(re.y() + re.height() / 2);

    //kDebug() << "// PREP AUDIO THMB FRMO : scale:" << pixelForOneFrame<< ", from: " << startpixel << ", to: " << endpixel;
    //if ( (!audioThumbWasDrawn || framePixelWidth!=pixelForOneFrame ) && !baseClip()->audioFrameChache.isEmpty()){

    for (int startCache = startpixel - startpixel % 100; startCache < endpixel; startCache += 100) {
        //kDebug() << "creating " << startCache;
        //if (framePixelWidth!=pixelForOneFrame  ||
        if (m_framePixelWidth == pixelForOneFrame && m_audioThumbCachePic.contains(startCache))
            continue;
        if (m_audioThumbCachePic[startCache].isNull() || m_framePixelWidth != pixelForOneFrame) {
            m_audioThumbCachePic[startCache] = QPixmap(100, (int)(re.height()));
            m_audioThumbCachePic[startCache].fill(QColor(180, 180, 200, 140));
        }
        bool fullAreaDraw = pixelForOneFrame < 10;
        QMap<int, QPainterPath > positiveChannelPaths;
        QMap<int, QPainterPath > negativeChannelPaths;
        QPainter pixpainter(&m_audioThumbCachePic[startCache]);
        QPen audiopen;
        audiopen.setWidth(0);
        pixpainter.setPen(audiopen);
        //pixpainter.setRenderHint(QPainter::Antialiasing,true);
        //pixpainter.drawLine(0,0,100,re.height());
        // Bail out, if caller provided invalid data
        if (channels <= 0) {
            kWarning() << "Unable to draw image with " << channels << "number of channels";
            return;
        }

        int channelHeight = m_audioThumbCachePic[startCache].height() / channels;

        for (int i = 0; i < channels; i++) {

            positiveChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
            negativeChannelPaths[i].moveTo(0, channelHeight*i + channelHeight / 2);
        }

        for (int samples = 0; samples <= 100; samples++) {
            double frame = (double)(samples + startCache - 0) / pixelForOneFrame;
            int sample = (int)((frame - (int)(frame)) * 20);   // AUDIO_FRAME_SIZE
            if (frame < 0 || sample < 0 || sample > 19)
                continue;
            QMap<int, QByteArray> frame_channel_data = baseClip()->m_audioFrameCache[(int)frame];

            for (int channel = 0; channel < channels && frame_channel_data[channel].size() > 0; channel++) {

                int y = channelHeight * channel + channelHeight / 2;
                int delta = (int)(frame_channel_data[channel][sample] - 127 / 2)  * channelHeight / 64;
                if (fullAreaDraw) {
                    positiveChannelPaths[channel].lineTo(samples, 0.1 + y + qAbs(delta));
                    negativeChannelPaths[channel].lineTo(samples, 0.1 + y - qAbs(delta));
                } else {
                    positiveChannelPaths[channel].lineTo(samples, 0.1 + y + delta);
                    negativeChannelPaths[channel].lineTo(samples, 0.1 + y - delta);
                }
            }
            for (int channel = 0; channel < channels ; channel++)
                if (fullAreaDraw && samples == 100) {
                    positiveChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(samples, channelHeight*channel + channelHeight / 2);
                    positiveChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                    negativeChannelPaths[channel].lineTo(0, channelHeight*channel + channelHeight / 2);
                }

        }
        pixpainter.setPen(QPen(QColor(0, 0, 0)));
        pixpainter.setBrush(QBrush(QColor(60, 60, 60)));

        for (int i = 0; i < channels; i++) {
            if (fullAreaDraw) {
                //pixpainter.fillPath(positiveChannelPaths[i].united(negativeChannelPaths[i]),QBrush(Qt::SolidPattern));//or singleif looks better
                pixpainter.drawPath(positiveChannelPaths[i].united(negativeChannelPaths[i]));//or singleif looks better
            } else
                pixpainter.drawPath(positiveChannelPaths[i]);
        }
    }
    //audioThumbWasDrawn=true;
    m_framePixelWidth = pixelForOneFrame;

    //}
}

int ClipItem::fadeIn() const
{
    return m_startFade;
}

int ClipItem::fadeOut() const
{
    return m_endFade;
}


void ClipItem::setFadeIn(int pos)
{
    if (pos == m_startFade) return;
    int oldIn = m_startFade;
    m_startFade = qBound(0, pos, (int)cropDuration().frames(m_fps));
    QRectF rect = boundingRect();
    update(rect.x(), rect.y(), qMax(oldIn, m_startFade), rect.height());
}

void ClipItem::setFadeOut(int pos)
{
    if (pos == m_endFade) return;
    int oldOut = m_endFade;
    m_endFade = qBound(0, pos, (int)cropDuration().frames(m_fps));
    QRectF rect = boundingRect();
    update(rect.x() + rect.width() - qMax(oldOut, m_endFade), rect.y(), qMax(oldOut, m_endFade), rect.height());

}

void ClipItem::setFades(int in, int out)
{
    m_startFade = in;
    m_endFade = out;
}

/*
//virtual
void ClipItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    //if (e->pos().x() < 20) m_hover = true;
    return;
    if (isItemLocked()) return;
    m_hover = true;
    QRectF r = boundingRect();
    double width = 35 / projectScene()->scale().x();
    double height = r.height() / 2;
    //WARNING: seems like it generates a full repaint of the clip, maybe not so good...
    update(r.x(), r.y() + height, width, height);
    update(r.right() - width, r.y() + height, width, height);
}

//virtual
void ClipItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (isItemLocked()) return;
    m_hover = false;
    QRectF r = boundingRect();
    double width = 35 / projectScene()->scale().x();
    double height = r.height() / 2;
    //WARNING: seems like it generates a full repaint of the clip, maybe not so good...
    update(r.x(), r.y() + height, width, height);
    update(r.right() - width, r.y() + height, width, height);
}
*/

void ClipItem::resizeStart(int posx, bool /*size*/)
{
    bool sizeLimit = false;
    if (clipType() != IMAGE && clipType() != COLOR && clipType() != TEXT) {
        const int min = (startPos() - cropStart()).frames(m_fps);
        if (posx < min) posx = min;
        sizeLimit = true;
    }

    if (posx == startPos().frames(m_fps)) return;
    const int previous = cropStart().frames(m_fps);
    AbstractClipItem::resizeStart(posx, sizeLimit);

    // set speed independant info
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);

    if ((int) cropStart().frames(m_fps) != previous) {
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            m_startThumbTimer.start(150);
        }
    }
}

void ClipItem::resizeEnd(int posx)
{
    const int max = (startPos() - cropStart() + maxDuration()).frames(m_fps);
    if (posx > max && maxDuration() != GenTime()) posx = max;
    if (posx == endPos().frames(m_fps)) return;
    //kDebug() << "// NEW POS: " << posx << ", OLD END: " << endPos().frames(m_fps);
    const int previous = cropDuration().frames(m_fps);
    AbstractClipItem::resizeEnd(posx);

    // set speed independant info
    m_speedIndependantInfo = m_info;
    m_speedIndependantInfo.cropStart = GenTime((int)(m_info.cropStart.frames(m_fps) * qAbs(m_speed)), m_fps);
    m_speedIndependantInfo.cropDuration = GenTime((int)(m_info.cropDuration.frames(m_fps) * qAbs(m_speed)), m_fps);

    if ((int) cropDuration().frames(m_fps) != previous) {
        if (m_hasThumbs && KdenliveSettings::videothumbnails()) {
            m_endThumbTimer.start(150);
        }
    }
}

//virtual
QVariant ClipItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) setZValue(10);
        else setZValue(2);
    }
    if (change == ItemPositionChange && scene()) {
        // calculate new position.
        //if (parentItem()) return pos();
        QPointF newPos = value.toPointF();
        //kDebug() << "/// MOVING CLIP ITEM.------------\n++++++++++";
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);
	// Warning: newPos gives a position relative to the click event, so hack to get absolute pos
	int yOffset = property("y_absolute").toInt() + newPos.y();
        int newTrack = yOffset / KdenliveSettings::trackheight();
        newTrack = qMin(newTrack, projectScene()->tracksCount() - 1);
        newTrack = qMax(newTrack, 0);
        newPos.setY((int)(newTrack  * KdenliveSettings::trackheight() + 1));
        // Only one clip is moving
        QRectF sceneShape = rect();
        sceneShape.translate(newPos);
        QList<QGraphicsItem*> items;
        if (projectScene()->editMode() == NORMALEDIT)
            items = scene()->items(sceneShape, Qt::IntersectsItemShape);
        items.removeAll(this);
        bool forwardMove = newPos.x() > pos().x();
        int offset = 0;
        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
                if (!items.at(i)->isEnabled()) continue;
                if (items.at(i)->type() == type()) {
                    // Collision!
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) {
                        return pos();
                    }
                    if (forwardMove) {
                        offset = qMax(offset, (int)(newPos.x() - (static_cast < AbstractClipItem* >(items.at(i))->startPos() - cropDuration()).frames(m_fps)));
                    } else {
                        offset = qMax(offset, (int)((static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps)) - newPos.x()));
                    }

                    if (offset > 0) {
                        if (forwardMove) {
                            sceneShape.translate(QPointF(-offset, 0));
                            newPos.setX(newPos.x() - offset);
                        } else {
                            sceneShape.translate(QPointF(offset, 0));
                            newPos.setX(newPos.x() + offset);
                        }
                        QList<QGraphicsItem*> subitems = scene()->items(sceneShape, Qt::IntersectsItemShape);
                        subitems.removeAll(this);
                        for (int j = 0; j < subitems.count(); j++) {
                            if (!subitems.at(j)->isEnabled()) continue;
                            if (subitems.at(j)->type() == type()) {
                                // move was not successful, revert to previous pos
                                m_info.startPos = GenTime((int) pos().x(), m_fps);
                                return pos();
                            }
                        }
                    }

                    m_info.track = newTrack;
                    m_info.startPos = GenTime((int) newPos.x(), m_fps);

                    return newPos;
                }
            }
        }
        m_info.track = newTrack;
        m_info.startPos = GenTime((int) newPos.x(), m_fps);
        //kDebug()<<"// ITEM NEW POS: "<<newPos.x()<<", mapped: "<<mapToScene(newPos.x(), 0).x();
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

// virtual
/*void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event) {
}*/

int ClipItem::effectsCounter()
{
    return effectsCount() + 1;
}

int ClipItem::effectsCount()
{
    return m_effectList.count();
}

int ClipItem::hasEffect(const QString &tag, const QString &id) const
{
    return m_effectList.hasEffect(tag, id);
}

QStringList ClipItem::effectNames()
{
    return m_effectList.effectNames();
}

QDomElement ClipItem::effect(int ix) const
{
    if (ix >= m_effectList.count() || ix < 0) return QDomElement();
    return m_effectList.at(ix).cloneNode().toElement();
}

QDomElement ClipItem::effectAtIndex(int ix) const
{
    if (ix > m_effectList.count() || ix <= 0) return QDomElement();
    return m_effectList.itemFromIndex(ix).cloneNode().toElement();
}

QDomElement ClipItem::getEffectAtIndex(int ix) const
{
    if (ix > m_effectList.count() || ix <= 0) return QDomElement();
    return m_effectList.itemFromIndex(ix);
}

void ClipItem::updateEffect(QDomElement effect)
{
    //kDebug() << "CHange EFFECT AT: " << ix << ", CURR: " << m_effectList.at(ix).attribute("tag") << ", NEW: " << effect.attribute("tag");
    m_effectList.updateEffect(effect);
    m_effectNames = m_effectList.effectNames().join(" / ");
    QString id = effect.attribute("id");
    if (id == "fadein" || id == "fadeout" || id == "fade_from_black" || id == "fade_to_black")
        update();
    else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
}

void ClipItem::enableEffects(QList <int> indexes, bool disable)
{
    m_effectList.enableEffects(indexes, disable);
}

bool ClipItem::moveEffect(QDomElement effect, int ix)
{
    if (ix <= 0 || ix > (m_effectList.count()) || effect.isNull()) {
        kDebug() << "Invalid effect index: " << ix;
        return false;
    }
    m_effectList.removeAt(effect.attribute("kdenlive_ix").toInt());
    effect.setAttribute("kdenlive_ix", ix);
    m_effectList.insert(effect);
    m_effectNames = m_effectList.effectNames().join(" / ");
    QString id = effect.attribute("id");
    if (id == "fadein" || id == "fadeout" || id == "fade_from_black" || id == "fade_to_black")
        update();
    else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
    return true;
}

EffectsParameterList ClipItem::addEffect(QDomElement effect, bool /*animate*/)
{
    bool needRepaint = false;
    QLocale locale;
    int ix;
    QDomElement insertedEffect;
    if (!effect.hasAttribute("kdenlive_ix")) {
	// effect dropped from effect list
        ix = effectsCounter();
    } else ix = effect.attribute("kdenlive_ix").toInt();
    if (!m_effectList.isEmpty() && ix <= m_effectList.count()) {
        needRepaint = true;
        insertedEffect = m_effectList.insert(effect);
    } else insertedEffect = m_effectList.append(effect);
    
    // Update index to the real one
    effect.setAttribute("kdenlive_ix", insertedEffect.attribute("kdenlive_ix"));
    int effectIn;
    int effectOut;

    if (effect.attribute("tag") == "affine") {
	// special case: the affine effect needs in / out points
	effectIn = effect.attribute("in").toInt();
	effectOut = effect.attribute("out").toInt();
    }
    else {
	effectIn = EffectsList::parameter(effect, "in").toInt();
	effectOut = EffectsList::parameter(effect, "out").toInt();
    }
    
    EffectsParameterList parameters;
    parameters.addParam("tag", insertedEffect.attribute("tag"));
    parameters.addParam("kdenlive_ix", insertedEffect.attribute("kdenlive_ix"));
    if (insertedEffect.hasAttribute("src")) parameters.addParam("src", insertedEffect.attribute("src"));
    if (insertedEffect.hasAttribute("disable")) parameters.addParam("disable", insertedEffect.attribute("disable"));

    QString effectId = insertedEffect.attribute("id");
    if (effectId.isEmpty()) effectId = insertedEffect.attribute("tag");
    parameters.addParam("id", effectId);

    QDomNodeList params = insertedEffect.elementsByTagName("parameter");
    int fade = 0;
    bool needInOutSync = false;

    // check if it is a fade effect
    if (effectId == "fadein") {
	needRepaint = true;
        if (m_effectList.hasEffect(QString(), "fade_from_black") == -1) {
	    fade = effectOut - effectIn;
        }/* else {
	    QDomElement fadein = m_effectList.getEffectByTag(QString(), "fade_from_black");
            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
        }*/
    } else if (effectId == "fade_from_black") {
	kDebug()<<"// FOUND FTB:"<<effectOut<<" - "<<effectIn;
	needRepaint = true;
        if (m_effectList.hasEffect(QString(), "fadein") == -1) {
	    fade = effectOut - effectIn;
        }/* else {
	    QDomElement fadein = m_effectList.getEffectByTag(QString(), "fadein");
            if (fadein.attribute("name") == "out") fade += fadein.attribute("value").toInt();
            else if (fadein.attribute("name") == "in") fade -= fadein.attribute("value").toInt();
        }*/
     } else if (effectId == "fadeout") {
	needRepaint = true;
        if (m_effectList.hasEffect(QString(), "fade_to_black") == -1) {
	    fade = effectIn - effectOut;
        } /*else {
	    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fade_to_black");
            if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
            else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
        }*/
    } else if (effectId == "fade_to_black") {
	needRepaint = true;
        if (m_effectList.hasEffect(QString(), "fadeout") == -1) {
	    fade = effectIn - effectOut;
        }/* else {
	    QDomElement fadeout = m_effectList.getEffectByTag(QString(), "fadeout");
            if (fadeout.attribute("name") == "out") fade -= fadeout.attribute("value").toInt();
            else if (fadeout.attribute("name") == "in") fade += fadeout.attribute("value").toInt();
        }*/
    }

    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute("type") == "geometry" && !e.hasAttribute("fixed")) {
                // Effects with a geometry parameter need to sync in / out with parent clip
                needInOutSync = true;
            }
            if (e.attribute("type") == "simplekeyframe") {
                QStringList values = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                double factor = locale.toDouble(e.attribute("factor", "1"));
                double offset = e.attribute("offset", "0").toDouble();
                if (factor != 1 || offset != 0) {
                    for (int j = 0; j < values.count(); j++) {
                        QString pos = values.at(j).section(':', 0, 0);
                        double val = (locale.toDouble(values.at(j).section(':', 1, 1)) - offset) / factor;
                        values[j] = pos + '=' + locale.toString(val);
                    }
                }
                parameters.addParam(e.attribute("name"), values.join(";"));
                /*parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", );*/
            } else if (e.attribute("type") == "keyframe") {
                parameters.addParam("keyframes", e.attribute("keyframes"));
                parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", e.attribute("factor", "1"));
                parameters.addParam("offset", e.attribute("offset", "0"));
                parameters.addParam("starttag", e.attribute("starttag", "start"));
                parameters.addParam("endtag", e.attribute("endtag", "end"));
            } else if (e.attribute("factor", "1") == "1" && e.attribute("offset", "0") == "0") {
                parameters.addParam(e.attribute("name"), e.attribute("value"));

            } else {
                double fact;
                if (e.attribute("factor").contains('%')) {
                    fact = ProfilesDialog::getStringEval(projectScene()->profile(), e.attribute("factor"));
                } else {
                    fact = locale.toDouble(e.attribute("factor", "1"));
                }
                double offset = e.attribute("offset", "0").toDouble();
                parameters.addParam(e.attribute("name"), locale.toString((locale.toDouble(e.attribute("value")) - offset) / fact));
            }
        }
    }
    if (needInOutSync) {
        parameters.addParam("in", QString::number(cropStart().frames(m_fps)));
        parameters.addParam("out", QString::number((cropStart() + cropDuration()).frames(m_fps) - 1));
        parameters.addParam("_sync_in_out", "1");
    }
    m_effectNames = m_effectList.effectNames().join(" / ");
    if (fade > 0) m_startFade = fade;
    else if (fade < 0) m_endFade = -fade;

    if (m_selectedEffect == -1) {
        setSelectedEffect(0);
    } else if (m_selectedEffect == ix - 1) setSelectedEffect(m_selectedEffect);
    if (needRepaint) update(boundingRect());
    /*if (animate) {
        flashClip();
    } */
    else { /*if (!needRepaint) */
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
    return parameters;
}

void ClipItem::deleteEffect(QString index)
{
    bool needRepaint = false;
    int ix = index.toInt();

    QDomElement effect = m_effectList.itemFromIndex(ix);
    QString effectId = effect.attribute("id");
    if ((effectId == "fadein" && hasEffect(QString(), "fade_from_black") == -1) ||
	(effectId == "fade_from_black" && hasEffect(QString(), "fadein") == -1)) {
        m_startFade = 0;
        needRepaint = true;
    } else if ((effectId == "fadeout" && hasEffect(QString(), "fade_to_black") == -1) ||
	(effectId == "fade_to_black" && hasEffect(QString(), "fadeout") == -1)) {
        m_endFade = 0;
        needRepaint = true;
    } else if (EffectsList::hasKeyFrames(effect)) needRepaint = true;
    m_effectList.removeAt(ix);
    m_effectNames = m_effectList.effectNames().join(" / ");

    if (m_effectList.isEmpty() || m_selectedEffect == ix) {
        // Current effect was removed
        if (ix > m_effectList.count()) {
            setSelectedEffect(m_effectList.count());
        } else setSelectedEffect(ix);
    }
    if (needRepaint) update(boundingRect());
    else {
        QRectF r = boundingRect();
        r.setHeight(20);
        update(r);
    }
    //if (!m_effectList.isEmpty()) flashClip();
}

double ClipItem::speed() const
{
    return m_speed;
}

int ClipItem::strobe() const
{
    return m_strobe;
}

void ClipItem::setSpeed(const double speed, const int strobe)
{
    m_speed = speed;
    if (m_speed <= 0 && m_speed > -1)
        m_speed = -1.0;
    m_strobe = strobe;
    if (m_speed == 1.0) m_clipName = m_clip->name();
    else m_clipName = m_clip->name() + " - " + QString::number(speed * 100, 'f', 0) + '%';
    m_info.cropStart = GenTime((int)(m_speedIndependantInfo.cropStart.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
    m_info.cropDuration = GenTime((int)(m_speedIndependantInfo.cropDuration.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
    //update();
}

GenTime ClipItem::maxDuration() const
{
    return GenTime((int)(m_maxDuration.frames(m_fps) / qAbs(m_speed) + 0.5), m_fps);
}

GenTime ClipItem::speedIndependantCropStart() const
{
    return m_speedIndependantInfo.cropStart;
}

GenTime ClipItem::speedIndependantCropDuration() const
{
    return m_speedIndependantInfo.cropDuration;
}


const ItemInfo ClipItem::speedIndependantInfo() const
{
    return m_speedIndependantInfo;
}

int ClipItem::nextFreeEffectGroupIndex() const
{
    int freeGroupIndex = 0;
    for (int i = 0; i < m_effectList.count(); i++) {
        QDomElement effect = m_effectList.at(i);
	EffectInfo effectInfo;
	effectInfo.fromString(effect.attribute("kdenlive_info"));
	if (effectInfo.groupIndex >= freeGroupIndex) {
	    freeGroupIndex = effectInfo.groupIndex + 1;
	}
    }
    return freeGroupIndex;
}

//virtual
void ClipItem::dropEvent(QGraphicsSceneDragDropEvent * event)
{
    if (event->proposedAction() == Qt::CopyAction && scene() && !scene()->views().isEmpty()) {
	const QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
	event->acceptProposedAction();
	QDomDocument doc;
	doc.setContent(effects, true);
	QDomElement e = doc.documentElement();
	if (e.tagName() == "effectgroup") {
	    // dropped an effect group
	    QDomNodeList effectlist = e.elementsByTagName("effect");
	    int freeGroupIndex = nextFreeEffectGroupIndex();
	    EffectInfo effectInfo;
	    for (int i = 0; i < effectlist.count(); i++) {
		QDomElement effect = effectlist.at(i).toElement();
		effectInfo.fromString(effect.attribute("kdenlive_info"));
		effectInfo.groupIndex = freeGroupIndex;
		effect.setAttribute("kdenlive_info", effectInfo.toString());
		effect.removeAttribute("kdenlive_ix");
	    }
	} else {
	    // single effect dropped
	    e.removeAttribute("kdenlive_ix");
	}
        CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
        if (view) view->slotAddEffect(e, m_info.startPos, track());
    }
    else return;
}

//virtual
void ClipItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (isItemLocked()) event->setAccepted(false);
    else if (event->mimeData()->hasFormat("kdenlive/effectslist")) {
	event->acceptProposedAction();
    } else event->setAccepted(false);
}

void ClipItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
}

void ClipItem::addTransition(Transition* t)
{
    m_transitionsList.append(t);
    //CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    QDomDocument doc;
    QDomElement e = doc.documentElement();
    //if (view) view->slotAddTransition(this, t->toXML() , t->startPos(), track());
}

void ClipItem::setVideoOnly(bool force)
{
    m_videoOnly = force;
}

void ClipItem::setAudioOnly(bool force)
{
    m_audioOnly = force;
    if (m_audioOnly) m_baseColor = QColor(141, 215, 166);
    else {
        if (m_clipType == COLOR) {
            QString colour = m_clip->getProperty("colour");
            colour = colour.replace(0, 2, "#");
            m_baseColor = QColor(colour.left(7));
        } else if (m_clipType == AUDIO) m_baseColor = QColor(141, 215, 166);
        else m_baseColor = QColor(141, 166, 215);
    }
    m_audioThumbCachePic.clear();
}

bool ClipItem::isAudioOnly() const
{
    return m_audioOnly;
}

bool ClipItem::isVideoOnly() const
{
    return m_videoOnly;
}

void ClipItem::insertKeyframe(QDomElement effect, int pos, int val)
{
    if (effect.attribute("disable") == "1") return;
    QLocale locale;
    effect.setAttribute("active_keyframe", pos);
    m_editedKeyframe = pos;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe")) {
            QString kfr = e.attribute("keyframes");
            const QStringList keyframes = kfr.split(';', QString::SkipEmptyParts);
            QStringList newkfr;
            bool added = false;
            foreach(const QString &str, keyframes) {
                int kpos = str.section(':', 0, 0).toInt();
                double newval = locale.toDouble(str.section(':', 1, 1));
                if (kpos < pos) {
                    newkfr.append(str);
                } else if (!added) {
                    if (i == m_visibleParam)
                        newkfr.append(QString::number(pos) + ':' + QString::number(val));
                    else
                        newkfr.append(QString::number(pos) + ':' + locale.toString(newval));
                    if (kpos > pos) newkfr.append(str);
                    added = true;
                } else newkfr.append(str);
            }
            if (!added) {
                if (i == m_visibleParam)
                    newkfr.append(QString::number(pos) + ':' + QString::number(val));
                else
                    newkfr.append(QString::number(pos) + ':' + e.attribute("default"));
            }
            e.setAttribute("keyframes", newkfr.join(";"));
        }
    }
}

void ClipItem::movedKeyframe(QDomElement effect, int oldpos, int newpos, double value)
{
    if (effect.attribute("disable") == "1") return;
    QLocale locale;
    effect.setAttribute("active_keyframe", newpos);
    QDomNodeList params = effect.elementsByTagName("parameter");
    int start = cropStart().frames(m_fps);
    int end = (cropStart() + cropDuration()).frames(m_fps) - 1;
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe")) {
            QString kfr = e.attribute("keyframes");
            const QStringList keyframes = kfr.split(';', QString::SkipEmptyParts);
            QStringList newkfr;
            foreach(const QString &str, keyframes) {
                if (str.section(':', 0, 0).toInt() != oldpos) {
                    newkfr.append(str);
                } else if (newpos != -1) {
                    newpos = qMax(newpos, start);
                    newpos = qMin(newpos, end);
                    if (i == m_visibleParam)
                        newkfr.append(QString::number(newpos) + ':' + locale.toString(value));
                    else
                        newkfr.append(QString::number(newpos) + ':' + str.section(':', 1, 1));
                }
            }
            e.setAttribute("keyframes", newkfr.join(";"));
        }
    }

    updateKeyframes(effect);
    update();
}

void ClipItem::updateKeyframes(QDomElement effect)
{
    m_keyframes.clear();
    QLocale locale;
    // parse keyframes
    QDomNodeList params = effect.elementsByTagName("parameter");
    QDomElement e = params.item(m_visibleParam).toElement();
    if (e.attribute("intimeline") != "1") {
        setSelectedEffect(m_selectedEffect);
        return;
    }
    m_limitedKeyFrames = e.attribute("type") == "keyframe";
    const QStringList keyframes = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
    foreach(const QString &str, keyframes) {
        int pos = str.section(':', 0, 0).toInt();
        double val = locale.toDouble(str.section(':', 1, 1));
        m_keyframes[pos] = val;
    }
    if (!m_keyframes.contains(m_selectedKeyframe)) m_selectedKeyframe = -1;
}

Mlt::Producer *ClipItem::getProducer(int track, bool trackSpecific)
{
    if (isAudioOnly())
        return m_clip->audioProducer(track);
    else if (isVideoOnly())
        return m_clip->videoProducer();
    else
        return m_clip->getProducer(trackSpecific ? track : -1);
}

QMap<int, QDomElement> ClipItem::adjustEffectsToDuration(int width, int height, ItemInfo oldInfo)
{
    QMap<int, QDomElement> effects;
    for (int i = 0; i < m_effectList.count(); i++) {
        QDomElement effect = m_effectList.at(i);

        if (effect.attribute("id").startsWith("fade")) {
            QString id = effect.attribute("id");
            int in = EffectsList::parameter(effect, "in").toInt();
            int out = EffectsList::parameter(effect, "out").toInt();
            int clipEnd = (cropStart() + cropDuration()).frames(m_fps) - 1;
            if (id == "fade_from_black" || id == "fadein") {
                if (in != cropStart().frames(m_fps)) {
                    effects[i] = effect.cloneNode().toElement();
                    int duration = out - in;
                    in = cropStart().frames(m_fps);
                    out = in + duration;
                    EffectsList::setParameter(effect, "in", QString::number(in));
                    EffectsList::setParameter(effect, "out", QString::number(out));
                }
                if (out > clipEnd) {
                    if (!effects.contains(i))
                        effects[i] = effect.cloneNode().toElement();
                    EffectsList::setParameter(effect, "out", QString::number(clipEnd));
                }
                if (effects.contains(i))
                    setFadeIn(out - in);
            } else {
                if (out != clipEnd) {
                    effects[i] = effect.cloneNode().toElement();
                    int diff = out - clipEnd;
                    in = qMax(in - diff, (int) cropStart().frames(m_fps));
                    out -= diff;
                    EffectsList::setParameter(effect, "in", QString::number(in));
                    EffectsList::setParameter(effect, "out", QString::number(out));
                }
                if (in < cropStart().frames(m_fps)) {
                    if (!effects.contains(i))
                        effects[i] = effect.cloneNode().toElement();
                    EffectsList::setParameter(effect, "in", QString::number(cropStart().frames(m_fps)));
                }
                if (effects.contains(i))
                    setFadeOut(out - in);
            }
            continue;
        } else if (effect.attribute("id") == "freeze" && cropStart() != oldInfo.cropStart) {
            effects[i] = effect.cloneNode().toElement();
            int diff = (oldInfo.cropStart - cropStart()).frames(m_fps);
            int frame = EffectsList::parameter(effect, "frame").toInt();
            EffectsList::setParameter(effect, "frame", QString::number(frame - diff));
            continue;
        } else if (effect.attribute("id") == "pan_zoom") {
	    effect.setAttribute("in", cropStart().frames(m_fps));
	    effect.setAttribute("out", (cropStart() + cropDuration()).frames(m_fps) - 1);
	}

        QDomNodeList params = effect.elementsByTagName("parameter");
        for (int j = 0; j < params.count(); j++) {
            QDomElement param = params.item(j).toElement();

            QString type = param.attribute("type");
            if (type == "geometry" && !param.hasAttribute("fixed")) {
                if (!effects.contains(i))
                    effects[i] = effect.cloneNode().toElement();
                updateGeometryKeyframes(effect, j, width, height, oldInfo);
            } else if (type == "simplekeyframe" || type == "keyframe") {
                if (!effects.contains(i))
                    effects[i] = effect.cloneNode().toElement();
                updateNormalKeyframes(param, oldInfo);
#ifdef USE_QJSON
            } else if (type == "roto-spline") {
                if (!effects.contains(i))
                    effects[i] = effect.cloneNode().toElement();
                QString value = param.attribute("value");
                if (adjustRotoDuration(&value, cropStart().frames(m_fps), (cropStart() + cropDuration()).frames(m_fps) - 1))
                    param.setAttribute("value", value);
#endif    
            }
        }
    }
    return effects;
}

bool ClipItem::updateNormalKeyframes(QDomElement parameter, ItemInfo oldInfo)
{
    int in = cropStart().frames(m_fps);
    int out = (cropStart() + cropDuration()).frames(m_fps) - 1;
    int oldin = oldInfo.cropStart.frames(m_fps);
    QLocale locale;
    bool keyFrameUpdated = false;

    const QStringList data = parameter.attribute("keyframes").split(';', QString::SkipEmptyParts);
    QMap <int, double> keyframes;
    foreach (QString keyframe, data) {
	int keyframepos = keyframe.section(':', 0, 0).toInt();
	// if keyframe was at clip start, update it
	if (keyframepos == oldin) {
	    keyframepos = in;
	    keyFrameUpdated = true;
	}
        keyframes[keyframepos] = locale.toDouble(keyframe.section(':', 1, 1));
    }


    QMap<int, double>::iterator i = keyframes.end();
    int lastPos = -1;
    double lastValue = 0;
    qreal relPos;

    /*
     * Take care of resize from start
     */
    bool startFound = false;
    while (i-- != keyframes.begin()) {
        if (i.key() < in && !startFound) {
            startFound = true;
            if (lastPos < 0) {
                keyframes[in] = i.value();
            } else {
                relPos = (in - i.key()) / (qreal)(lastPos - i.key() + 1);
                keyframes[in] = i.value() + (lastValue - i.value()) * relPos;
            }
        }
        lastPos = i.key();
        lastValue = i.value();
        if (startFound)
            i = keyframes.erase(i);
    }

    /*
     * Take care of resize from end
     */
    i = keyframes.begin();
    lastPos = -1;
    bool endFound = false;
    while (i != keyframes.end()) {
        if (i.key() > out && !endFound) {
            endFound = true;
            if (lastPos < 0) {
                keyframes[out] = i.value();
            } else {
                relPos = (out - lastPos) / (qreal)(i.key() - lastPos + 1);
                keyframes[out] = lastValue + (i.value() - lastValue) * relPos;
            }
         }
        lastPos = i.key();
        lastValue = i.value();
        if (endFound)
            i = keyframes.erase(i);
        else
            ++i;
    }

    if (startFound || endFound || keyFrameUpdated) {
        QString newkfr;
        QMap<int, double>::const_iterator k = keyframes.constBegin();
        while (k != keyframes.constEnd()) {
            newkfr.append(QString::number(k.key()) + ':' + QString::number(qRound(k.value())) + ';');
            ++k;
        }
        parameter.setAttribute("keyframes", newkfr);
        return true;
    }

    return false;
}

void ClipItem::updateGeometryKeyframes(QDomElement effect, int paramIndex, int width, int height, ItemInfo oldInfo)
{
    QDomElement param = effect.elementsByTagName("parameter").item(paramIndex).toElement();
    int offset = oldInfo.cropStart.frames(m_fps);
    QString data = param.attribute("value");
    if (offset > 0) {
        QStringList kfrs = data.split(';');
        data.clear();
        foreach (const QString &keyframe, kfrs) {
            if (keyframe.contains('=')) {
                int pos = keyframe.section('=', 0, 0).toInt();
                pos += offset;
                data.append(QString::number(pos) + '=' + keyframe.section('=', 1) + ";");
            }
            else data.append(keyframe + ';');
        }
    }
    Mlt::Geometry geometry(data.toUtf8().data(), oldInfo.cropDuration.frames(m_fps), width, height);
    param.setAttribute("value", geometry.serialise(cropStart().frames(m_fps), (cropStart() + cropDuration()).frames(m_fps) - 1));
}

void ClipItem::slotGotThumbsCache()
{
    disconnect(m_clip->thumbProducer(), SIGNAL(thumbsCached()), this, SLOT(slotGotThumbsCache()));
    update();
}

#include "clipitem.moc"

