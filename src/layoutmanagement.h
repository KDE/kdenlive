/*
SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2020 Julius Künzel <jk.kdedev@smartlab.uber.space>
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef LAYOUTMANAGEMENT_H
#define LAYOUTMANAGEMENT_H

#include <QObject>

class KSelectAction;
class QAction;
class QButtonGroup;
class QAbstractButton;
class QHBoxLayout;

class LayoutManagement : public QObject
{
    Q_OBJECT

public:
    explicit LayoutManagement(QObject *parent);
    /** @brief Load a layout by its name. */
    bool loadLayout(const QString &layoutId, bool selectButton);

private slots:
    /** @brief Saves the widget layout. */
    void slotSaveLayout();
    /** @brief Loads a layout from its button. */
    void activateLayout(QAbstractButton *button);
    /** @brief Loads a saved widget layout. */
    void slotLoadLayout(QAction *action);
    /** @brief Manage layout. */
    void slotManageLayouts();

private:
    /** @brief Saves the given layout asking the user for a name.
     * @param layout
     * @param suggestedName name that is filled in to the save layout dialog
     * @return names of the saved layout. First is the visible name, second the internal name (they are different if the layout is a default one)
    */
    std::pair<QString, QString> saveLayout(QString layout, QString suggestedName);
    /** @brief Populates the "load layout" menu. */
    void initializeLayouts();
    QWidget *m_container;
    QButtonGroup *m_containerGrp;
    QHBoxLayout *m_containerLayout;
    KSelectAction *m_loadLayout;
    QList <QAction *> m_layoutActions;

signals:
    /** @brief Layout changed, ensure title bars are correctly displayed. */
    void updateTitleBars();
};

#endif
