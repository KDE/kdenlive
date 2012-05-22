/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef DOUBLEPROPERTIESVIEW_H
#define DOUBLEPROPERTIESVIEW_H

#include <QWidget>

class DragValue;


class DoublePropertiesView : public QWidget
{
    Q_OBJECT

public:
    DoublePropertiesView(const QString &name, double value, double min, double max, const QString &comment, int id, const QString suffix = QString(), int decimals = 0, QWidget* parent = 0);
    ~DoublePropertiesView() {};

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(double value);

private slots:
    void valueChanged(double value, bool final);

private:
    DragValue *m_dragValue;

signals:
    void valueChanged(double);
};

/*/**
 * @class DoubleParameterWidget
 * @brief Widget to choose a double parameter (for a effect) with the help of a slider and a spinbox.
 * @author Till Theato
 *
 * The widget does only handle integers, so the parameter has to be converted into the proper double range afterwards.
 * /

class DoubleParameterWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
    * @param name Name of the parameter
    * @param value Value of the parameter
    * @param min Minimum value
    * @param max maximum value
    * @param defaultValue Value used when using reset functionality
    * @param comment A comment explaining the parameter. Will be shown for the tooltip.
    * @param suffix (optional) Suffix to display in spinbox
    * @param parent (optional) Parent Widget * /
    DoubleParameterWidget(const QString &name, double value, double min, double max, double defaultValue, const QString &comment, int id, const QString suffix = QString(), int decimals = 0, QWidget* parent = 0);
    ~DoubleParameterWidget();

    /** @brief Gets the parameter's value. * /
    double getValue();
    /** @brief Set the inTimeline property to paint widget with other colors. * /
    void setInTimelineProperty(bool intimeline);
    /** @brief Returns minimum size for QSpinBox, used to set all spinboxes to the same width. * /
    int spinSize();
    void setSpinSize(int width);
    

public slots:
    /** @brief Sets the value to @param value.
    void setValue(double value);

    /** @brief Sets value to m_default. 
    void slotReset();

private slots:
    /** @brief Shows/Hides the comment label.
    void slotShowComment(bool show);

    void slotSetValue(double value, bool final);

private:
    DragValue *m_dragVal;
    QLabel *m_commentLabel;
    
signals:
    void valueChanged(double);
    /** @brief User wants to see this parameter in timeline.
    void setInTimeline(int);
};
*/
#endif
