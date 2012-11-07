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
	void open();
	void open(const QString &name, int channels, int buffersize);
	void close();

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
