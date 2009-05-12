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

#include "dvdwizardmenu.h"

#include <KDebug>


DvdWizardMenu::DvdWizardMenu(const QString &profile, QWidget *parent) :
        QWizardPage(parent)
{
    m_view.setupUi(this);
    m_view.play_text->setText(i18n("Play"));
    m_scene = new DvdScene(this);
    m_view.menu_preview->setScene(m_scene);

    connect(m_view.create_menu, SIGNAL(toggled(bool)), m_view.menu_box, SLOT(setEnabled(bool)));
    connect(m_view.create_menu, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));

    m_view.add_button->setIcon(KIcon("document-new"));
    m_view.delete_button->setIcon(KIcon("trash-empty"));

    // TODO: re-enable add button options
    /*m_view.add_button->setVisible(false);
    m_view.delete_button->setVisible(false);*/

    m_view.menu_profile->addItems(QStringList() << i18n("PAL") << i18n("NTSC"));

    if (profile == "dv_ntsc" || profile == "dv_ntsc_wide") {
        m_view.menu_profile->setCurrentIndex(1);
        m_isPal = false;
    } else m_isPal = true;

    // Create color background
    m_color = new QGraphicsRectItem(0, 0, m_width, m_height);
    m_color->setBrush(m_view.background_color->color());
    m_color->setZValue(2);
    m_scene->addItem(m_color);


    // create background image
    m_background = new QGraphicsPixmapItem();
    m_background->setZValue(3);
    //m_scene->addItem(m_background);

    // create safe zone rect
    int safeW = m_width / 20;
    int safeH = m_height / 20;
    m_safeRect = new QGraphicsRectItem(safeW, safeH, m_width - 2 * safeW, m_height - 2 * safeH);
    QPen pen(Qt::red);
    pen.setStyle(Qt::DashLine);
    pen.setWidth(3);
    m_safeRect->setPen(pen);
    m_safeRect->setZValue(5);
    m_scene->addItem(m_safeRect);

    changeProfile(m_view.menu_profile->currentIndex());
    checkBackgroundType(0);

    connect(m_view.menu_profile, SIGNAL(activated(int)), this, SLOT(changeProfile(int)));

    // create menu button
    DvdButton *button = new DvdButton(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    button->setZValue(4);
    button->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    QRectF r = button->sceneBoundingRect();
    m_scene->addItem(button);
    button->setPos((m_width - r.width()) / 2, (m_height - r.height()) / 2);
    button->setSelected(true);


    //m_view.menu_preview->resizefitInView(0, 0, m_width, m_height);

    connect(m_view.play_text, SIGNAL(textChanged(const QString &)), this, SLOT(buildButton()));
    connect(m_view.text_color, SIGNAL(changed(const QColor &)), this, SLOT(updateColor()));
    connect(m_view.font_size, SIGNAL(valueChanged(int)), this, SLOT(buildButton()));
    connect(m_view.font_family, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(buildButton()));
    connect(m_view.background_image, SIGNAL(textChanged(const QString &)), this, SLOT(buildImage()));
    connect(m_view.background_color, SIGNAL(changed(const QColor &)), this, SLOT(buildColor()));

    connect(m_view.background_list, SIGNAL(activated(int)), this, SLOT(checkBackgroundType(int)));

    connect(m_view.target_list, SIGNAL(activated(int)), this, SLOT(setButtonTarget(int)));

    connect(m_view.add_button, SIGNAL(pressed()), this, SLOT(addButton()));
    connect(m_view.delete_button, SIGNAL(pressed()), this, SLOT(deleteButton()));
    connect(m_scene, SIGNAL(selectionChanged()), this, SLOT(buttonChanged()));
    connect(m_scene, SIGNAL(changed(const QList<QRectF> &)), this, SIGNAL(completeChanged()));

    m_view.menu_box->setEnabled(false);

}

DvdWizardMenu::~DvdWizardMenu()
{
    delete m_color;
    delete m_safeRect;
    delete m_background;
    delete m_scene;
}

// virtual

bool DvdWizardMenu::isComplete() const
{
    if (!m_view.create_menu->isChecked()) return true;
    QList <int> targets;
    QList<QGraphicsItem *> list = m_scene->items();
    int buttonCount = 0;
    // check that the menu buttons don't collide
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            buttonCount++;
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            QList<QGraphicsItem *> collisions = button->collidingItems();
            if (!collisions.isEmpty()) {
                for (int j = 0; j < collisions.count(); j++) {
                    if (collisions.at(j)->type() == button->type()) return false;
                }
            }
            targets.append(button->target());
        }
    }
    if (buttonCount == 0) {
        // We need at least one button
        return false;
    }

    if (!m_view.background_image->isHidden()) {
        // Make sure user selected a valid image / video file
        if (!QFile::exists(m_view.background_image->url().path())) return false;
    }

    // check that we have a "Play all" entry
    if (targets.contains(0)) return true;
    // ... or that each video file has a button
    for (int i = m_view.target_list->count() - 1; i > 0; i--) {
        if (!targets.contains(i)) return false;
    }
    return true;
}

void DvdWizardMenu::setButtonTarget(int ix)
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            button->setTarget(ix, m_view.target_list->itemData(ix).toString());
            break;
        }
    }
    emit completeChanged();
}

void DvdWizardMenu::buttonChanged()
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    bool foundButton = false;
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            m_view.play_text->blockSignals(true);
            m_view.font_size->blockSignals(true);
            m_view.font_family->blockSignals(true);
            m_view.target_list->blockSignals(true);
            foundButton = true;
            m_view.tabWidget->widget(0)->setEnabled(true);
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            m_view.target_list->setCurrentIndex(button->target());
            m_view.play_text->setText(button->toPlainText());
            QFont font = button->font();
            m_view.font_size->setValue(font.pixelSize());
            m_view.font_family->setCurrentFont(font);
            m_view.play_text->blockSignals(false);
            m_view.font_size->blockSignals(false);
            m_view.font_family->blockSignals(false);
            m_view.target_list->blockSignals(false);
            break;
        }
    }
    if (!foundButton) m_view.tabWidget->widget(0)->setEnabled(false);
}

void DvdWizardMenu::addButton()
{
    m_scene->clearSelection();
    DvdButton *button = new DvdButton(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    button->setZValue(4);
    button->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    QRectF r = button->sceneBoundingRect();
    m_scene->addItem(button);
    button->setPos((m_width - r.width()) / 2, (m_height - r.height()) / 2);
    button->setSelected(true);
}

void DvdWizardMenu::deleteButton()
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            delete list.at(i);
            break;
        }
    }
}

void DvdWizardMenu::changeProfile(int ix)
{
    switch (ix) {
    case 1:
        m_width = 720;
        m_height = 480;
        m_isPal = false;
        updatePreview();
        break;
    default:
        m_width = 720;
        m_height = 576;
        m_isPal = true;
        updatePreview();
        break;
    }
}

void DvdWizardMenu::updatePreview()
{
    m_scene->setProfile(m_width, m_height);
    QMatrix matrix;
    matrix.scale(0.5, 0.5);
    m_view.menu_preview->setMatrix(matrix);
    m_view.menu_preview->setMinimumSize(m_width / 2 + 4, m_height / 2 + 8);

    m_color->setRect(0, 0, m_width, m_height);

    int safeW = m_width / 20;
    int safeH = m_height / 20;
    m_safeRect->setRect(safeW, safeH, m_width - 2 * safeW, m_height - 2 * safeH);
}

void DvdWizardMenu::setTargets(QStringList list, QStringList targetlist)
{
    m_view.target_list->clear();
    m_view.target_list->addItem(i18n("Play All"), "title 1");
    for (int i = 0; i < list.count(); i++) {
        m_view.target_list->addItem(list.at(i), targetlist.at(i));
    }
}

void DvdWizardMenu::checkBackgroundType(int ix)
{
    if (ix == 0) {
        m_view.background_color->setVisible(true);
        m_view.background_image->setVisible(false);
        m_scene->removeItem(m_background);
    } else {
        m_view.background_color->setVisible(false);
        m_view.background_image->setVisible(true);
        if (ix == 1) {
            m_view.background_image->setFilter("*");
            m_scene->addItem(m_background);
        } else {
            m_scene->removeItem(m_background);
            m_view.background_image->setFilter("video/mpeg");
        }
    }
}

void DvdWizardMenu::buildColor()
{
    m_color->setBrush(m_view.background_color->color());
}

void DvdWizardMenu::buildImage()
{
    emit completeChanged();
    if (m_view.background_image->url().isEmpty()) {
        m_scene->removeItem(m_background);
        return;
    }
    QPixmap pix;
    if (!pix.load(m_view.background_image->url().path())) {
        m_scene->removeItem(m_background);
        return;
    }
    pix = pix.scaled(m_width, m_height);
    m_background->setPixmap(pix);
    if (m_view.background_list->currentIndex() == 1) m_scene->addItem(m_background);
}

void DvdWizardMenu::checkBackground()
{
    if (m_view.background_list->currentIndex() != 1) {
        m_scene->removeItem(m_background);
    } else {
        m_scene->addItem(m_background);
    }
}

void DvdWizardMenu::buildButton()
{
    DvdButton *button = NULL;
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            button = static_cast < DvdButton* >(list.at(i));
            break;
        }
    }
    if (button == NULL) return;
    button->setPlainText(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
}

void DvdWizardMenu::updateColor()
{
    updateColor(m_view.text_color->color());
    m_view.menu_preview->viewport()->update();
}

void DvdWizardMenu::updateColor(QColor c)
{
    DvdButton *button = NULL;
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            button = static_cast < DvdButton* >(list.at(i));
            button->setDefaultTextColor(c);
        }
    }
}


void DvdWizardMenu::createButtonImages(const QString &img1, const QString &img2, const QString &img3)
{
    if (m_view.create_menu->isChecked()) {
        m_scene->clearSelection();
        QImage img(m_width, m_height, QImage::Format_ARGB32);
        QPainter p(&img);
        m_scene->removeItem(m_safeRect);
        m_scene->removeItem(m_color);
        m_scene->removeItem(m_background);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        QImage saved;
        if (m_view.menu_profile->currentIndex() < 2)
            saved = img.scaled(720, 576);
        else saved = img.scaled(720, 480);
        saved.setNumColors(4);
        saved.save(img1);

        updateColor(m_view.selected_color->color());
        p.begin(&img);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        if (m_view.menu_profile->currentIndex() < 2)
            saved = img.scaled(720, 576);
        else saved = img.scaled(720, 480);
        saved.setNumColors(4);
        saved.save(img2);


        updateColor(m_view.highlighted_color->color());
        p.begin(&img);
        m_scene->render(&p, QRectF(0, 0, m_width, m_height));
        p.end();
        if (m_view.menu_profile->currentIndex() < 2)
            saved = img.scaled(720, 576);
        else saved = img.scaled(720, 480);
        saved.setNumColors(4);
        saved.save(img3);

        updateColor();

        m_scene->addItem(m_safeRect);
        m_scene->addItem(m_color);
        if (m_view.background_list->currentIndex() == 1) m_scene->addItem(m_background);
    }
}


void DvdWizardMenu::createBackgroundImage(const QString &img1)
{
    QImage img;
    if (m_view.background_list->currentIndex() == 0) {
        // color background
        if (m_isPal)
            img = QImage(768, 576, QImage::Format_ARGB32);
        else
            img = QImage(720, 540, QImage::Format_ARGB32);
        img.fill(m_view.background_color->color().rgb());
    } else if (m_view.background_list->currentIndex() == 1) {
        img.load(m_view.background_image->url().path());
        if (m_isPal) {
            if (img.width() != 768 || img.height() != 576)
                img = img.scaled(768, 576, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        } else {
            if (img.width() != 720 || img.height() != 540)
                img = img.scaled(720, 540, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    } else return;
    img.save(img1);
}

bool DvdWizardMenu::createMenu() const
{
    return m_view.create_menu->isChecked();
}

bool DvdWizardMenu::menuMovie() const
{
    return m_view.background_list->currentIndex() == 2;
}

QString DvdWizardMenu::menuMoviePath() const
{
    return m_view.background_image->url().path();
}


QMap <QString, QRect> DvdWizardMenu::buttonsInfo()
{
    QMap <QString, QRect> info;
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            QRect r = list.at(i)->sceneBoundingRect().toRect();
            // Make sure y1 is not odd (requested by spumux)
            if (r.height() % 2 == 1) r.setHeight(r.height() + 1);
            if (r.y() % 2 == 1) r.setY(r.y() - 1);
            info.insertMulti(button->command(), r);
        }
    }
    return info;
}

bool DvdWizardMenu::isPalMenu() const
{
    return m_isPal;
}
