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
	m_valid(false),
	m_shutdown(false),
	m_transportEnabled(false),
	m_loopState(0)
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

void* JackSlave::runTransportThread(void* device)
{
	((JackSlave*)device)->updateTransportState();
	return NULL;
}

void JackSlave::updateTransportState()
{
	pthread_mutex_lock(&m_transportLock);
	while (m_valid && !m_shutdown)	{
		jack_position_t jackpos;
		jack_transport_state_t state = JackTransportStopped;
		int position = 0;

		/* don't call client if already invalidated */
		if (!m_shutdown) {
			state = jack_transport_query(m_client, &jackpos);
			/* convert jack (audio) to kdenlive video position  */
			position = toVideoPosition(jackpos);
		}

		if(state != m_state) {
			m_nextState = m_state = state;

			switch (state) {
				case JackTransportRolling:
					/* fire playback started event */
					emit playbackStarted(position);
					break;
				case JackTransportStopped:
					/* fire playback stopped event */
					emit playbackStopped(position);
					break;
				default:
					break;
			}
		}

		pthread_cond_wait(&m_transportCondition, &m_transportLock);
	}
	pthread_mutex_unlock(&m_transportLock);
}

int JackSlave::toVideoPosition(const jack_position_t &position)
{
	return (int)((m_mltProfile->fps() * (double)position.frame) / (double)position.frame_rate /*+ (double)0.5*/);
}

jack_nframes_t JackSlave::toAudioPosition(const int &position, const jack_nframes_t &framerate)
{
	return (jack_nframes_t)(((double) framerate * (double) position) / (double) m_mltProfile->fps());
}

bool JackSlave::isValid()
{
	return m_valid;
}

void JackSlave::open(const QString &name, int channels, int buffersize)
{
	kDebug() << "open client";
	if (m_valid == true)
		return;

	jack_options_t options = JackNullOption;
	jack_status_t status;

	// open client
	m_client = jack_client_open(name.toUtf8().constData(), options, &status);

	if(m_client == NULL)
//		AUD_THROW(AUD_ERROR_JACK, clientopen_error);
		return;

	// set callbacks
	jack_set_process_callback(m_client, JackSlave::jack_process, this);
	jack_on_shutdown(m_client, JackSlave::jack_shutdown, this);
	jack_set_sync_callback(m_client, JackSlave::jack_sync, this);

	/* store the number of channels */
	m_channels = channels;
	/* register our output channels which are called ports in jack */
	m_ports = new jack_port_t*[m_channels];

//	try
	{
		char portname[64];
		for(int i = 0; i < m_channels; i++)
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

	m_ringbuffers = new jack_ringbuffer_t*[m_channels];
	for(int i = 0; i < m_channels; i++)
		m_ringbuffers[i] = jack_ringbuffer_create(buffersize * sizeof(float));

	resetLooping();
	connect(this, SIGNAL(currentPositionChanged(int)),
			this, SLOT(processLooping()));

	m_valid = true;
	m_sync = 0;
	/* store current transport state */
	m_nextState = m_state = jack_transport_query(m_client, NULL);
	/* store jacks audio sample rate */
	m_frameRate = jack_get_sample_rate(m_client);

	pthread_mutex_init(&m_transportLock, NULL);
	pthread_cond_init(&m_transportCondition, NULL);

	// activate the client
	if(jack_activate(m_client))
	{
		jack_client_close(m_client);
		delete[] m_ports;
		for(int i = 0; i < m_channels; i++)
			jack_ringbuffer_free(m_ringbuffers[i]);
		delete[] m_ringbuffers;
		pthread_mutex_destroy(&m_transportLock);
		pthread_cond_destroy(&m_transportCondition);

//		AUD_THROW(AUD_ERROR_JACK, activate_error);
		m_valid = false;
		return;
	}

	const char** ports = jack_get_ports(m_client, NULL, NULL,
										JackPortIsPhysical | JackPortIsInput);
	if(ports != NULL)
	{
		for(int i = 0; i < /*m_specs.*/m_channels && ports[i]; i++)
			jack_connect(m_client, jack_port_name(m_ports[i]), ports[i]);

		free(ports);
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&m_transportThread, &attr, runTransportThread, this);
	pthread_attr_destroy(&attr);
}

void JackSlave::close()
{
	if(m_valid) {
		m_valid = false;
		/* TODO: find a better solution - don't call close 2 times (two monitor problem)*/
		pthread_mutex_lock(&m_transportLock);
		pthread_cond_signal(&m_transportCondition);
		pthread_mutex_unlock(&m_transportLock);
		pthread_join(m_transportThread, NULL);
		pthread_cond_destroy(&m_transportCondition);
		pthread_mutex_destroy(&m_transportLock);

		/* close the client if not is done already by jackd */
		if (!m_shutdown)
			jack_client_close(m_client);
		delete[] m_ports;

		/* free and delete ringbuffers */
		for(int i = 0; i < m_channels; i++)
			jack_ringbuffer_free(m_ringbuffers[i]);
		delete[] m_ringbuffers;

		disconnect(this, SIGNAL(currentPositionChanged(int)),
				this, SLOT(processLooping()));
	}
}

bool JackSlave::probe()
{
	jack_client_t *client;
	jack_status_t status;
	jack_options_t options = JackNoStartServer;

	/* try to connect to jackd */
	client = jack_client_open("kdenliveprobe", options, &status, NULL );

	/* close client if connection successful */
	if (status == 0) {
		jack_client_close(client);
	}

	/* return status */
	return (status == 0);
}

void JackSlave::jack_shutdown(void *data)
{
	JackSlave* device = (JackSlave*)data;
	device->m_shutdown = true;
}

int JackSlave::jack_sync(jack_transport_state_t state, jack_position_t* pos, void* data)
{
	JackSlave* device = (JackSlave*)data;
	int result = 0;

	/* no job if transport is disabled */
	if (!device->m_transportEnabled)
		return 1;

	if(state == JackTransportStopped) {
		if (!device->m_sync) {
			/*start seeking */
			device->m_sync = 1;
			emit device->playbackSync(device->toVideoPosition(*pos));
		} else if (device->m_sync == 1) {
			/* sync position */
			if (device->m_currentPosition == device->toVideoPosition(*pos)) {
				device->m_sync = 2;
			} else {
				device->m_sync = 0;
			}
		}
	} else if (state == JackTransportStarting) {
		if (!device->m_sync) {
			/*start seeking */
			device->m_sync = 1;
			emit device->playbackSync(device->toVideoPosition(*pos));
		} else if (device->m_sync == 1) {
			/* sync position */
			if (device->m_currentPosition == device->toVideoPosition(*pos)) {
				device->m_sync = 2;
			} else {
				device->m_sync = 0;
			}
		}
	} else if (state == JackTransportRolling) {
		/* give up sync and just start playing */
		device->m_sync = 0;
		result = 1;
	}

	if (device->m_sync > 1) {
		/* FIXME: optimize sync */
#if 0
		for(int i = 0; i < device->m_channels; i++)
			jack_ringbuffer_reset(device->m_ringbuffers[i]);
#endif
		device->m_sync = 0;
		result = 1;
	}

	return result;
}

int JackSlave::jack_process(jack_nframes_t frames, void *data)
{
	JackSlave* device = (JackSlave*)data;
	int channels = device->m_channels;

	/* TODO: review later */
#if 0
	if (device->m_sync) {
		size_t bufsize = (frames * sizeof(float));
		/* play silence while syncing */
		for(int i = 0; i < channels; i++)
			memset(jack_port_get_buffer(device->m_ports[i], frames), 0, bufsize);

		/* FIXME: optimize sync */
//		if (device->m_sync > 1) {
//			for(int i = 0; i < device->m_channels; i++)
//				jack_ringbuffer_reset(device->m_ringbuffers[i]);
//			device->m_sync = 3;
//		}

	} else
#endif
	{
		/* convert nr of frames to nr of bytes */
		size_t jacksize = (frames * sizeof(float));
		/* copy audio data into jack buffers */
		for (int i = 0; i < channels; i++) {
			size_t ringsize;
			char * buffer;

			ringsize = jack_ringbuffer_read_space(device->m_ringbuffers[i]);
			buffer = (char*)jack_port_get_buffer(device->m_ports[i], frames);
			jack_ringbuffer_read(device->m_ringbuffers[i], buffer,
					ringsize < jacksize ? ringsize : jacksize );
			/* fill the rest with zeros to prevent noise */
			if(ringsize < jacksize)
				memset(buffer + ringsize, 0, jacksize - ringsize);
		}
	}

	if(device->m_transportEnabled) {
		if(pthread_mutex_trylock(&(device->m_transportLock)) == 0)	{
			if (device->m_state != jack_transport_query(device->m_client, NULL))
				pthread_cond_signal(&(device->m_transportCondition));
			pthread_mutex_unlock(&(device->m_transportLock));
		}
	}
	return 0;
}

void JackSlave::updateBuffers(Mlt::Frame& frame)
{
	/* no job while syncing */
	if (m_sync) {
		return;
	}

	if (!frame.is_valid() || frame.get_int("test_audio") != 0) {
        return;
    }

    mlt_audio_format format = mlt_audio_float;
    int freq = (int)m_frameRate;
    int samples = 0;

    /* get the audio buffers */
    float *buffer = (float*)frame.get_audio(format, freq, m_channels, samples);

    if (!buffer) {
        return;
    }

	/* process audio */
	size_t size = samples * sizeof(float);
	/* write into output ringbuffer */
	for (int i = 0; i < m_channels; i++)
	{
		if (jack_ringbuffer_write_space(m_ringbuffers[i]) >= size)
			jack_ringbuffer_write(m_ringbuffers[i], (char*)(buffer + i * samples), size);
	}
}

void JackSlave::processLooping()
{
	if (m_loopState == 1) {
		m_loopState = 2;
	} else if (m_loopState == 2) {
		if (m_currentPosition == m_loopOut ) {
			if (m_loopInfinite) {
				seekPlayback(m_loopIn, false);
			} else {
				stopPlayback();
			}
		}
	} else {
		m_loopState = 0;
		return;
	}

}

void JackSlave::startPlayback(bool resetLoop)
{
	if (m_valid) {
		/* reset looping state */
		if (resetLoop)
			resetLooping();
		/* start jack transport */
		jack_transport_start(m_client);
		m_nextState = JackTransportRolling;
	}
}

void JackSlave::stopPlayback(bool resetLoop)
{
	if (m_valid) {
		/* reset looping state */
		if (resetLoop)
			resetLooping();
		/* stop jack transport */
		jack_transport_stop(m_client);
		m_nextState = JackTransportStopped;
	}
}

void JackSlave::seekPlayback(int time, bool resetLoop)
{
    if (m_valid) {
    	if(time >= 0) {
    		/* reset looping state */
    		if (resetLoop)
    			resetLooping();
    		/* locate jack transport */
    		jack_transport_locate(m_client, toAudioPosition(time, m_frameRate));
    	}
    }
}

void JackSlave::loopPlayback(int in, int out, bool infinite)
{
	m_loopIn = in;
	m_loopOut = out;
	m_loopInfinite = infinite;

	if (!doesPlayback())
		startPlayback(false);
	seekPlayback(m_loopIn, false);
	m_loopState = 1;
}

void JackSlave::resetLooping()
{
	m_loopIn = 0;
	m_loopOut = 0;
	m_loopInfinite = false;
	m_loopState = 0;
}

int JackSlave::getPlaybackPosition()
{
	jack_position_t position;
	jack_transport_query(m_client, &position);
	return toVideoPosition(position);
}

void JackSlave::setCurrentPosition(int position)
{
	m_currentPosition = position;
	emit currentPositionChanged(position);
}

void JackSlave::setTransportEnabled(bool state)
{
	m_transportEnabled = state;
}

bool JackSlave::doesPlayback()
{
	return (m_nextState != JackTransportStopped);
}

#include "jackslave.moc"
