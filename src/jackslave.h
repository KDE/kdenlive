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

#include <kdebug.h>
#include <mlt++/Mlt.h>
#include <QImage>


class JackSlave : public QObject
{
    Q_OBJECT
public:
    static JackSlave& singleton(Mlt::Profile * profile = 0);
	virtual ~JackSlave();
	bool isActive();
	Mlt::Filter * filter();
	void open();
	void close();
	void startPlayback();
	void stopPlayback();
	void locate(int position);

protected:
	JackSlave(Mlt::Profile * profile);

private:
	bool m_isActive;
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
