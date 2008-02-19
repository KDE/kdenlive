#ifndef EFFECTSTACKVIEW_H
#define EFFECTSTACKVIEW_H

#include <KIcon>

#include "ui_effectstack_ui.h"
#include "clipitem.h"
class EffectStackView : public QWidget
{
	Q_OBJECT
		
	public:
		EffectStackView( QWidget *parent=0);
	
private:
	int activeRow;
	QStringList effects;
	Ui::EffectStack_UI ui;
	ClipItem* clipref;
	void setupListView(const QStringList& );
public slots:
	void slotClipItemSelected(ClipItem*);
	void slotItemSelectionChanged();
	void slotItemUp();
	void slotItemDown();
	void slotItemDel();
	
};

#endif
