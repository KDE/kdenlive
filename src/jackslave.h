/***************************************************************************
 *   Copyright (C) 2012 by Ed Rogalsky (ed.rogalsky@gmail.com)             *
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

#ifndef JACKSLAVE_H_
#define JACKSLAVE_H_

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <mlt++/Mlt.h>
#include <QObject>
#include <qstring.h>
#include <KDebug>


class JackSlave : public QObject
{
    Q_OBJECT
public:
    static JackSlave& singleton(Mlt::Profile * profile = 0);
	virtual ~JackSlave();
	bool isValid();
	Mlt::Filter * filter();
	void open(const QString &name, int channels, int buffersize);
	void close();

	/**
	 * Convert jack audio frame position to kdenlive video position.
	 */
	int toVideoPosition(const jack_position_t &position);

	/**
	 * Checks if jackd is started.
	 */
	bool probe();

	/**
	 * Starts jack transport playback.
	 */
	void startPlayback();

	/**
	 * Stops jack transport playback.
	 */
	void stopPlayback();

	/**
	 * Seeks jack transport playback.
	 * \param time The time to seek to.
	 */
	void seekPlayback(int time);

	/**
	 * Retrieves the jack transport playback time.
	 * \return The current time position.
	 */
	int getPlaybackPosition();

	/**
	 * Returns whether jack transport plays back.
	 * \return Whether jack transport plays back.
	 */
	bool doesPlayback();
	/**
	 * Next Jack Transport state (-1 if not expected to change).
	 */
	jack_transport_state_t m_nextState;

	/**
	 * Current jack transport status.
	 */
	jack_transport_state_t m_state;

	/**
	 * Syncronisation state.
	 */
	int m_sync;

	void locate(int position);

protected:
	JackSlave(Mlt::Profile * profile);

private:
	/**
	 * The output ports of jack.
	 */
	jack_port_t** m_ports;

	/**
	 * The jack client.
	 */
	jack_client_t* m_client;

	/**
	 * The jack ring buffer.
	 */
	jack_ringbuffer_t** m_ringbuffers;

	/**
	 * Whether the device is valid.
	 */
	bool m_valid;

	/**
	 * Whether the device is shutdown by jack.
	 */
	bool m_shutdown;

	/**
	 * The transport synchronization thread.
	 */
	pthread_t m_transportThread;

	/**
	 * Mutex for mixing.
	 */
	pthread_mutex_t m_transportLock;

	/**
	 * Condition for mixing.
	 */
	pthread_cond_t m_transportCondition;

	/**
	 * Transport thread function.
	 * \param device The this pointer.
	 * \return NULL.
	 */
	static void* runTransportThread(void* device);

	/**
	 * Updates the transport state.
	 */
	void updateTransportState();

	/**
	 * Invalidates the jack device.
	 * \param data The jack device that gets invalidet by jack.
	 */
	static void jack_shutdown(void *data);

	/**
	 * Mixes the next bytes into the buffer.
	 * \param length The length in samples to be filled.
	 * \param data A pointer to the jack device.
	 * \return 0 what shows success.
	 */
	static int jack_process(jack_nframes_t length, void *data);

	static int jack_sync(jack_transport_state_t state, jack_position_t* pos, void* data);


	/** ----------------------------- **/
	Mlt::Filter *m_mltFilterJack;
    Mlt::Profile *m_mltProfile;

    static void onJackStartedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position);
    static void onJackStoppedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position);

signals:
	void playbackStarted(int position);
	void playbackStopped(int position);
};

#define JACKSLAVE JackSlave::singleton()

#endif /* JACKSLAVE_H_ */
