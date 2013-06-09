/*
 * Copyright (c) 2011 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MLTCONTROLLER_H
#define MLTCONTROLLER_H

#include <QImage>
#include <QMutex>
#include <QMap>
#include <mlt++/Mlt.h>
#include "kdenlivecore_export.h"
//#include "transportcontrol.h"

// forward declarations
class QWidget;
class MltController;
class ProducerWrapper;

enum DISPLAYMODE {MLTOPENGL, MLTGLSL, MLTSCENE, MLTSDL};

namespace Mlt {

class QFrame : public QObject
{
    Q_OBJECT
public:
    QFrame(QObject *parent = 0);
    QFrame(const Frame& frame);
    QFrame(const QFrame& qframe);
    ~QFrame();
    Frame* frame() const;
    QImage image();
private:
    Frame* m_frame;
};

}
class KDENLIVECORE_EXPORT TransportControl : public QObject
{
    Q_OBJECT
public:
    TransportControl(MltController *controller);
public slots:
    void play(double speed = 1.0);
    void pause();
    void stop();
    void seek(int position);
    int position() const;
    void rewind();
    void fastForward();
    void previous(int currentPosition);
    void next(int currentPosition);
    void setIn(int);
    void setOut(int);
private:
    MltController *m_controller;
};

class MltController
{
protected:
    MltController(Mlt::Profile *profile);
    virtual int reconfigure(bool isMulti) = 0;

public:
    static MltController& singleton(QWidget* parent = 0);
    virtual ~MltController();

    virtual QWidget* displayWidget() = 0;
    virtual QWidget* videoWidget() = 0;
    virtual int open(ProducerWrapper *, bool isMulti = false, bool isLive = false);
    virtual void closeConsumer();
    virtual void activate();
    virtual int reOpen(bool isMulti = false);
    //virtual int open(const char* url);
    virtual void close();
    virtual void pause();
    virtual void reStart();
    virtual void seek(int position);
    virtual QImage image(Mlt::Frame *frame, int width, int height);
    virtual void refreshConsumer();
    
    bool isActive() const;

    void play(double speed = 1.0);
    void switchPlay();
    void stop();
    bool enableJack(bool enable = true);
    void setVolume(double volume);
    double volume() const;
    void onWindowResize();
    QString saveXML(const QString& filename, Mlt::Service* service = 0);
    int consumerChanged();
    int setProfile(Mlt::Profile *profile);
    QString resource() const;
    bool isSeekable();
    bool isPlaylist() const;
    void rewind();
    void fastForward();
    void nextFrame(int factor = 1);
    void previousFrame(int factor = 1);
    void previous(int currentPosition);
    void next(int currentPosition);
    void setIn(int);
    void setOut(int);
    int position() const;
    QImage thumb(ProducerWrapper *producer, int position);
    virtual void renderImage(const QString &, ProducerWrapper *, QList<int> positions, int width = -1, int height = -1) = 0;
    ProducerWrapper *producer();
    ProducerWrapper *originalProducer();
    DISPLAYMODE displayType() const;

    Mlt::Repository* repository() const {
        return m_repo;
    }
    Mlt::Profile& profile() const {
        return *m_profile;
    }
    ProducerWrapper* producer() const {
        return m_producer;
    }
    Mlt::Consumer* consumer() const {
        return m_consumer;
    }
    const QString& URL() const {
        return m_url;
    }
    TransportControl* transportControl() const {
        return m_transportControl;
    }
    int displayPosition;

protected:
    Mlt::Repository* m_repo;
    ProducerWrapper* m_producer;
    Mlt::FilteredConsumer* m_consumer;
    DISPLAYMODE m_type;
    double m_display_ratio;
    /**
     * @brief Useful in configurations where only one monitor can be active at a time (glsl / sdl).
     */
    bool m_isActive;
    bool m_isLive;
    QPoint m_zone;
    QMap <int,QString> m_markers;
    QMutex m_mutex;

private:
    Mlt::Profile* m_profile;
    Mlt::Filter* m_volumeFilter;
    Mlt::Filter* m_jackFilter;
    QString m_url;
    double m_volume;
    TransportControl *m_transportControl;

    static void on_jack_started(mlt_properties owner, void* object, mlt_position *position);
    void onJackStarted(int position);
    static void on_jack_stopped(mlt_properties owner, void* object, mlt_position *position);
    void onJackStopped(int position);
};

 // namespace

//#define MLT Mlt::Controller::singleton()

#endif // MLTCONTROLLER_H
