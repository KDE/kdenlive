/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINESEARCH_H
#define TIMELINESEARCH_H

#include <QTimer>
#include <QTime>

class QAction;
class QKeyEvent;

/**
 * @class TimelineSearch
 * @brief Provides functionality to search the timeline items (clips, guides) by name.
 *
 * Currently a part of the code remains in CustomTrackView.
 * Should be made a plugin when refactoring is done.
 */

class TimelineSearch : public QObject
{
    Q_OBJECT

public:
    explicit TimelineSearch(QObject *parent = nullptr);

private slots:
    void slotInitSearch();
    void slotEndSearch();
    void slotFindNext();

protected:
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

private:
    void search();
    bool keyPressEvent(QKeyEvent *key);

    QAction *m_findAction;
    QAction *m_findNextAction;

    QString m_searchTerm;
    QTimer m_searchTimer;
};

#endif
