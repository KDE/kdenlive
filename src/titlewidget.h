#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include "ui_titlewidget_ui.h"
#include <QDialog>

class TitleWidget : public QDialog , public Ui::TitleWidget_UI{
	Q_OBJECT
public:
	TitleWidget(QDialog *parent=0);
public slots:
	void slotNewText();
	void slotNewRect();
	void slotChangeBackground();
	void selectionChanged();
	void textChanged();
};


#endif
