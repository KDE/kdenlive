/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef DVDWIZARDMENU_H
#define DVDWIZARDMENU_H

#include <QWizardPage>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>

#include <KDebug>

#include "ui_dvdwizardmenu_ui.h"

class DvdScene : public QGraphicsScene {

public:
    DvdScene(QObject * parent = 0): QGraphicsScene(parent) {}
    void setProfile(int width, int height) {
        m_width = width;
        m_height = height;
        setSceneRect(0, 0, m_width, m_height);
    }
    int sceneWidth() const {
        return m_width;
    }
    int sceneHeight() const {
        return m_height;
    }
private:
    int m_width;
    int m_height;
};

class DvdButton : public QGraphicsTextItem {

public:
    DvdButton(const QString & text): QGraphicsTextItem(text), m_target(0) {}
    enum { Type = UserType + 1 };
    void setTarget(int t) {
        m_target = t;
    }
    int target() const {
        return m_target;
    }
    int type() const {
        // Enable the use of qgraphicsitem_cast with this item.
        return Type;
    }

private:
    int m_target;


protected:

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) {
        if (change == ItemPositionChange && scene()) {

            /*   QList<QGraphicsItem *> list = collidingItems();
               if (!list.isEmpty()) {
                   for (int i = 0; i < list.count(); i++) {
                if (list.at(i)->type() == Type) return pos();
                   }
               }
            */
            DvdScene *sc = static_cast < DvdScene * >(scene());
            QRectF rect = QRectF(0, 0, sc->width(), sc->height());
            QPointF newPos = value.toPointF();
            if (!rect.contains(newPos)) {
                // Keep the item inside the scene rect.
                newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
                newPos.setY(qMin(rect.bottom(), qMax(newPos.y(), rect.top())));
                return newPos;
            }
        }
        return QGraphicsItem::itemChange(change, value);
    }

};


class DvdWizardMenu : public QWizardPage {
    Q_OBJECT

public:
    DvdWizardMenu(const QString &profile, QWidget * parent = 0);
    virtual ~DvdWizardMenu();
    virtual bool isComplete() const;
    bool createMenu() const;
    void createBackgroundImage(const QString &img1);
    void createButtonImages(const QString &img1, const QString &img2, const QString &img3);
    void setTargets(QStringList list);
    QMap <int, QRect> buttonsInfo();
    bool menuMovie() const;
    QString menuMoviePath() const;
    bool isPalMenu() const;

private:
    Ui::DvdWizardMenu_UI m_view;
    bool m_isPal;
    DvdScene *m_scene;
    DvdButton *m_button;
    QGraphicsPixmapItem *m_background;
    QGraphicsRectItem *m_color;
    QGraphicsRectItem *m_safeRect;
    int m_width;
    int m_height;
    QStringList m_targets;

private slots:
    void buildButton();
    void buildColor();
    void buildImage();
    void checkBackground();
    void checkBackgroundType(int ix);
    void changeProfile(int ix);
    void updatePreview();
    void buttonChanged();
    void addButton();
    void setButtonTarget(int ix);
    void deleteButton();
    void updateColor();
    void updateColor(QColor c);
};

#endif

