/****************************************************************************
** Form interface generated from reading ui file './kmmeditpanel.ui'
**
** Created: Tue Apr 9 14:13:30 2002
**      by:  The User Interface Compiler (uic)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/
#ifndef KMMEDITPANEL_H
#define KMMEDITPANEL_H

#include <qvariant.h>
#include <qwidget.h>
class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QSlider;
class QToolButton;

class KMMEditPanel : public QWidget
{ 
    Q_OBJECT

public:
    KMMEditPanel( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~KMMEditPanel();

    QSlider* positionSlider;
    QToolButton* startButton;
    QToolButton* rewindButton;
    QToolButton* stopButton;
    QToolButton* playButton;
    QToolButton* forwardButton;
    QToolButton* endButton;

protected:
    QGridLayout* KMMEditPanelLayout;
    QVBoxLayout* Layout2;
    QHBoxLayout* Layout1;
};

#endif // KMMEDITPANEL_H
