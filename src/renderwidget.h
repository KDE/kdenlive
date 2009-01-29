/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef RENDERWIDGET_H
#define RENDERWIDGET_H

#include <QDialog>
#include <QPushButton>
#include <QPainter>

#include "definitions.h"
#include "ui_renderwidget_ui.h"


// RenderViewDelegate is used to draw the progress bars.
class RenderViewDelegate : public QItemDelegate {
    Q_OBJECT
public:
    RenderViewDelegate(QWidget *parent) : QItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const {
        if (index.column() == 0) {
            QItemDelegate::paint(painter, option, index);
            return;
        } else if (index.column() == 1) {
            const bool hover = option.state & (QStyle::State_Selected);
            QRect r1 = option.rect;
            painter->save();
            if (hover) {
                painter->setPen(option.palette.color(QPalette::HighlightedText));
                QColor backgroundColor = option.palette.color(QPalette::Highlight);
                painter->setBrush(QBrush(backgroundColor));
                painter->fillRect(r1, QBrush(backgroundColor));
            } else painter->setPen(option.palette.color(QPalette::Text));
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int mid = (int)((r1.height() / 2));
            r1.setBottom(r1.y() + mid);
            QRect r2 = option.rect;
            r2.setTop(r2.y() + mid);
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom , index.data().toString());
            font.setBold(false);
            painter->setFont(font);
            painter->setPen(option.palette.color(QPalette::Mid));
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignVCenter , index.data(Qt::UserRole + 3).toString());
            painter->restore();
            return;
        }
        // Set up a QStyleOptionProgressBar to precisely mimic the
        // environment of a progress bar.
        QStyleOptionProgressBar progressBarOption;
        progressBarOption.state = option.state;
        progressBarOption.direction = QApplication::layoutDirection();
        QRect rect = option.rect;
        const bool hover = option.state & (QStyle::State_Selected);
        if (hover) {
            painter->setPen(option.palette.color(QPalette::HighlightedText));
            QColor backgroundColor = option.palette.color(QPalette::Highlight);
            painter->setBrush(QBrush(backgroundColor));
            painter->fillRect(rect, QBrush(backgroundColor));
        }

        int mid = rect.height() / 2;
        rect.setTop(rect.top() + mid / 2);
        rect.setHeight(mid);
        progressBarOption.rect = rect;
        progressBarOption.fontMetrics = QApplication::fontMetrics();
        progressBarOption.minimum = 0;
        progressBarOption.maximum = 100;
        progressBarOption.textAlignment = Qt::AlignCenter;
        progressBarOption.textVisible = true;

        // Set the progress and text values of the style option.
        int progress = index.data(Qt::UserRole).toInt();
        progressBarOption.progress = progress < 0 ? 0 : progress;
        progressBarOption.text = QString().sprintf("%d%%", progressBarOption.progress);

        // Draw the progress bar onto the view.
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOption, painter);
    }
};


class RenderWidget : public QDialog {
    Q_OBJECT

public:
    RenderWidget(QWidget * parent = 0);
    void setGuides(QDomElement guidesxml, double duration);
    void focusFirstVisibleItem();
    void setProfile(MltVideoProfile profile);
    void setRenderJob(const QString &dest, int progress = 0);
    void setRenderStatus(const QString &dest, int status, const QString &error);

private slots:
    void slotUpdateButtons(KUrl url);
    void slotUpdateButtons();
    void slotExport();
    void refreshView();
    void refreshParams();
    void slotSaveProfile();
    void slotEditProfile();
    void slotDeleteProfile();
    void slotUpdateGuideBox();
    void slotCheckStartGuidePosition();
    void slotCheckEndGuidePosition();
    void showInfoPanel();
    void slotAbortCurrentJob();

private:
    Ui::RenderWidget_UI m_view;
    MltVideoProfile m_profile;
    void parseProfiles(QString group = QString(), QString profile = QString());
    void parseFile(QString exportFile, bool editable);
    void updateButtons();
    KUrl filenameWithExtension(KUrl url, QString extension);

signals:
    void doRender(const QString&, const QString&, const QStringList &, const QStringList &, bool, bool, double, double, bool);
    void abortProcess(const QString &url);
    void openDvdWizard(const QString &url, const QString &profile);
};


#endif

