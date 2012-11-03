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
	m_isActive(false)
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

void JackSlave::close()
{
	if(m_mltFilterJack && isActive()) {
		delete m_mltFilterJack;
		m_mltFilterJack = 0;
		m_isActive = false;
	}
}

void JackSlave::onJackStartedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position)
{
	/* get slave ref */
	JackSlave *slave = (JackSlave*)consumer;

	/* fire playback started event */
	slave->playbackStarted(*position);
}

void JackSlave::onJackStoppedProxy(mlt_properties owner, mlt_consumer consumer, mlt_position *position)
{
	/* get slave ref */
	JackSlave *slave = (JackSlave*)consumer;

	/* fire playback started event */
	slave->playbackStopped(*position);
}

void JackSlave::startPlayback()
{
	if (isActive()) {
		/* file transport start event */
		m_mltFilterJack->fire_event("jack-start");
	}
}

void JackSlave::stopPlayback()
{
	if (isActive()) {
		/* fire transport stop event */
		m_mltFilterJack->fire_event("jack-stop");
	}
}

void JackSlave::locate(int position)
{
    if (isActive()) {
		mlt_properties jack_properties = (mlt_properties)m_mltFilterJack->get_properties();
		mlt_events_fire(jack_properties, "jack-seek", &position, NULL);
	}
}


