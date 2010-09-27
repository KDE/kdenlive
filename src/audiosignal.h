#ifndef AUDIOSIGNAL_H
#define AUDIOSIGNAL_H

#include <QByteArray>
#include <QList>
#include <QColor>
class QLabel;

#include  <QWidget>
class AudioSignal : public QWidget 
{
	Q_OBJECT
	public:
		AudioSignal (QWidget *parent=0);
	private:
		QLabel* label;
		QByteArray channels;
		QList<QColor> col;
	protected:
		void paintEvent(QPaintEvent* );
	public slots:
		void showAudio(QByteArray);


};

#endif
