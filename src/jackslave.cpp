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

#include "jackslave.h"

JackSlave::JackSlave(Mlt::Profile * profile) :
	m_isActive(false),
	m_valid(false)
{
	m_mltProfile = profile;
}

JackSlave& JackSlave::singleton(Mlt::Profile * profile)
{
    static JackSlave* instance = 0;

    if (!instance && profile != 0) {
    	instance = new JackSlave(profile);
    }

    return *instance;
}

JackSlave::~JackSlave()
{
	// TODO Auto-generated destructor stub


}

bool JackSlave::isActive()
{
	return m_isActive;
}

Mlt::Filter * JackSlave::filter()
{
	return m_mltFilterJack;
}

void JackSlave::open()
{
	if (m_isActive == false /* && m_mltFilterJack == 0 */) {
		// create jackrack filter using the factory
		m_mltFilterJack = new Mlt::Filter(*m_mltProfile, "jackrack", NULL);

		if(m_mltFilterJack != NULL && m_mltFilterJack->is_valid()) {
			// set filter properties
			m_mltFilterJack->set("out_1", "system:playback_1");
			m_mltFilterJack->set("out_2", "system:playback_2");

			// register the jack listeners
			m_mltFilterJack->listen("jack-stopped", this, (mlt_listener) JackSlave::onJackStoppedProxy);
			m_mltFilterJack->listen("jack-started", this, (mlt_listener) JackSlave::onJackStartedProxy);
//			m_mltFilterJack->listen("jack-starting", this, (mlt_listener) Render::_on_jack_starting);
//			m_mltFilterJack->listen("jack-last-pos-req", this, (mlt_listener) Render::_on_jack_last_pos_req);

			m_isActive = true;
		}
	}
}

void JackSlave::open(QString name, int channels, int buffersize)
{
	kDebug() << "open client";
	if (m_valid == true)
		return;

	jack_options_t options = JackNullOption;
	jack_status_t status;

	// open client
//	m_client = jack_client_open(name.toAscii().data(), options, &status);
	m_client = jack_client_open("kdenlive", options, &status);

	if(m_client == NULL)
//		AUD_THROW(AUD_ERROR_JACK, clientopen_error);
		return;

	// set callbacks
//	jack_set_process_callback(m_client, AUD_JackDevice::jack_mix, this);
//	jack_on_shutdown(m_client, AUD_JackDevice::jack_shutdown, this);
//	jack_set_sync_callback(m_client, AUD_JackDevice::jack_sync, this);

	// register our output channels which are called ports in jack
	m_ports = new jack_port_t*[channels];

//	try
	{
		char portname[64];
		for(int i = 0; i < channels; i++)
		{
			sprintf(portname, "out_%d", i+1);
			m_ports[i] = jack_port_register(m_client, portname,
											JACK_DEFAULT_AUDIO_TYPE,
											JackPortIsOutput, 0);
			if(m_ports[i] == NULL) {
//				AUD_THROW(AUD_ERROR_JACK, port_error);
				jack_client_close(m_client);
				delete[] m_ports;
				return;
			}
		}
	}
//	catch(AUD_Exception&)
//	{
//		jack_client_close(m_client);
//		delete[] m_ports;
//		throw;
//	}

//	m_specs.rate = (AUD_SampleRate)jack_get_sample_rate(m_client);
//
//	buffersize *= sizeof(sample_t);
//	m_ringbuffers = new jack_ringbuffer_t*[specs.channels];
//	for(unsigned int i = 0; i < specs.channels; i++)
//		m_ringbuffers[i] = jack_ringbuffer_create(buffersize);
//	buffersize *= specs.channels;
//	m_deinterleavebuf.resize(buffersize);
//	m_buffer.resize(buffersize);
//
//	create();

	m_valid = true;
//	m_sync = 0;
//	m_syncFunc = NULL;
	m_nextState = m_state = jack_transport_query(m_client, NULL);

//	pthread_mutex_init(&m_mixingLock, NULL);
//	pthread_cond_init(&m_mixingCondition, NULL);

	// activate the client
	if(jack_activate(m_client))
	{
		jack_client_close(m_client);
		delete[] m_ports;
//		for(unsigned int i = 0; i < specs.channels; i++)
//			jack_ringbuffer_free(m_ringbuffers[i]);
//		delete[] m_ringbuffers;
//		pthread_mutex_destroy(&m_mixingLock);
//		pthread_cond_destroy(&m_mixingCondition);
//		destroy();
//
//		AUD_THROW(AUD_ERROR_JACK, activate_error);
		m_valid = false;
		return;
	}

//	const char** ports = jack_get_ports(m_client, NULL, NULL,
//										JackPortIsPhysical | JackPortIsInput);
//	if(ports != NULL)
//	{
//		for(int i = 0; i < m_specs.channels && ports[i]; i++)
//			jack_connect(m_client, jack_port_name(m_ports[i]), ports[i]);
//
//		free(ports);
//	}

//	pthread_attr_t attr;
//	pthread_attr_init(&attr);
//	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
//
//	pthread_create(&m_mixingThread, &attr, runMixingThread, this);
//
//	pthread_attr_destroy(&attr);
}

void JackSlave::close()
{
//	if(m_mltFilterJack && isActive()) {
//		delete m_mltFilterJack;
//		m_mltFilterJack = 0;
//		m_isActive = false;
//	}
//
	if(m_valid) {
		jack_client_close(m_client);
		delete[] m_ports;
		m_valid = false;
	}
//	pthread_mutex_lock(&m_mixingLock);
//	pthread_cond_signal(&m_mixingCondition);
//	pthread_mutex_unlock(&m_mixingLock);
//	pthread_join(m_mixingThread, NULL);
//
//	pthread_cond_destroy(&m_mixingCondition);
//	pthread_mutex_destroy(&m_mixingLock);
//	for(unsigned int i = 0; i < m_specs.channels; i++)
//		jack_ringbuffer_free(m_ringbuffers[i]);
//	delete[] m_ringbuffers;
}

void JackSlave::onJackStartedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position)
{
	/* get slave ref */
	JackSlave *slave = (JackSlave*)consumer;

	/* fire playback started event */
	emit slave->playbackStarted(*position);
}

void JackSlave::onJackStoppedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position)
{
	/* get slave ref */
	JackSlave *slave = (JackSlave*)consumer;

	/* fire playback started event */
	emit slave->playbackStopped(*position);
}

void JackSlave::startPlayback()
{
	if (m_valid) {
		/* start jack transport */
		jack_transport_start(m_client);
		m_nextState = JackTransportRolling;
	}
}

void JackSlave::stopPlayback()
{
	if (m_valid) {
		/* stop jack transport */
		jack_transport_stop(m_client);
		m_nextState = JackTransportStopped;
	}
}

void JackSlave::seekPlayback(int time)
{
    if (m_valid) {
    	if(time >= 0) {
    		jack_nframes_t jack_frame = jack_get_sample_rate(m_client);
    		jack_frame *= time / m_mltProfile->fps();
    		jack_transport_locate(m_client, jack_frame);
    	}
    }
}

int JackSlave::getPlaybackPosition()
{
	jack_position_t position;
	jack_transport_query(m_client, &position);
//	mlt_position position = m_mltProfile->fps() * jack_pos->frame / jack_pos->frame_rate + 0.5;
	return ((m_mltProfile->fps() * position.frame) / position.frame_rate + 0.5);
}

#include "jackslave.moc"
