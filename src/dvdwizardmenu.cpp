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
#include "kdenlivesettings.h"

#include <KDebug>
#include <KColorScheme>

#if KDE_IS_VERSION(4,6,0)
#include <QGraphicsDropShadowEffect>
#endif


#include "kthumb.h"

DvdWizardMenu::DvdWizardMenu(const QString &profile, QWidget *parent) :
        QWizardPage(parent),
        m_color(NULL),
        m_safeRect(NULL),
        m_finalSize(720, 576),
        m_movieLength(-1)
{
    m_view.setupUi(this);
    m_view.play_text->setText(i18n("Play"));
    m_scene = new DvdScene(this);
    m_view.menu_preview->setScene(m_scene);
    m_view.menu_preview->setMouseTracking(true);
    connect(m_view.create_menu, SIGNAL(toggled(bool)), m_view.menu_box, SLOT(setEnabled(bool)));
    connect(m_view.create_menu, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    
#if KDE_IS_VERSION(4,7,0)
    m_menuMessage = new KMessageWidget;
    QGridLayout *s =  static_cast <QGridLayout*> (layout());
    s->addWidget(m_menuMessage, 7, 0, 1, -1);
    m_menuMessage->hide();
    m_view.error_message->hide();
#endif

    m_view.add_button->setIcon(KIcon("document-new"));
    m_view.delete_button->setIcon(KIcon("trash-empty"));
    m_view.zoom_button->setIcon(KIcon("zoom-in"));
    m_view.unzoom_button->setIcon(KIcon("zoom-out"));

    m_view.add_button->setToolTip(i18n("Add new button"));
    m_view.delete_button->setToolTip(i18n("Delete current button"));

    if (profile == "dv_ntsc" || profile == "dv_ntsc_wide") {
        changeProfile(false);
    } else changeProfile(true);


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
    checkBackgroundType(0);

    // create menu button
    DvdButton *button = new DvdButton(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    //font.setStyleStrategy(QFont::NoAntialias);
#if KDE_IS_VERSION(4,6,0)
    if (m_view.use_shadow->isChecked()) {
	QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
	shadow->setBlurRadius(7);
	shadow->setOffset(4, 4);
	button->setGraphicsEffect(shadow);
    }
    connect(m_view.use_shadow, SIGNAL(stateChanged(int)), this, SLOT(slotEnableShadows(int)));
#elif KDE_IS_VERSION(4,6,0)
    m_view.use_shadow->setHidden(true);
#endif
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    button->setZValue(4);
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
    connect(m_view.back_to_menu, SIGNAL(toggled(bool)), this, SLOT(setBackToMenu(bool)));

    connect(m_view.add_button, SIGNAL(pressed()), this, SLOT(addButton()));
    connect(m_view.delete_button, SIGNAL(pressed()), this, SLOT(deleteButton()));
    connect(m_view.zoom_button, SIGNAL(pressed()), this, SLOT(slotZoom()));
    connect(m_view.unzoom_button, SIGNAL(pressed()), this, SLOT(slotUnZoom()));
    connect(m_scene, SIGNAL(selectionChanged()), this, SLOT(buttonChanged()));
    connect(m_scene, SIGNAL(changed(const QList<QRectF> &)), this, SIGNAL(completeChanged()));

    // red background for error message
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    QPalette p = m_view.error_message->palette();
    p.setColor(QPalette::Background, scheme.background(KColorScheme::NegativeBackground).color());
    m_view.error_message->setAutoFillBackground(true);
    m_view.error_message->setPalette(p);

    m_view.menu_box->setEnabled(false);

}

DvdWizardMenu::~DvdWizardMenu()
{
    delete m_color;
    delete m_safeRect;
    delete m_background;
    delete m_scene;
}

void DvdWizardMenu::slotEnableShadows(int enable)
{
#if KDE_IS_VERSION(4,6,0)
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            if (enable) {
		QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
		shadow->setBlurRadius(7);
		shadow->setOffset(4, 4);
		list.at(i)->setGraphicsEffect(shadow);
	    }
            else list.at(i)->setGraphicsEffect(NULL);
        }
    }
#endif
}

// virtual
bool DvdWizardMenu::isComplete() const
{
    m_view.error_message->setHidden(true);
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
                    if (collisions.at(j)->type() == button->type()) {
#if KDE_IS_VERSION(4,7,0)
                        m_menuMessage->setText(i18n("Buttons overlapping"));
                        m_menuMessage->setMessageType(KMessageWidget::Warning);
                        m_menuMessage->animatedShow();
#else
                        m_view.error_message->setText(i18n("Buttons overlapping"));
                        m_view.error_message->setHidden(false);
#endif
                        return false;
                    }
                }
            }
            targets.append(button->target());
        }
    }
    if (buttonCount == 0) {
        //We need at least one button
#if KDE_IS_VERSION(4,7,0)
        m_menuMessage->setText(i18n("No button in menu"));
        m_menuMessage->setMessageType(KMessageWidget::Warning);
        m_menuMessage->animatedShow();
#else
        m_view.error_message->setText(i18n("No button in menu"));
        m_view.error_message->setHidden(false);
#endif
        return false;
    }

    if (!m_view.background_image->isHidden()) {
        // Make sure user selected a valid image / video file
        if (!QFile::exists(m_view.background_image->url().path())) {
#if KDE_IS_VERSION(4,7,0)
            m_menuMessage->setText(i18n("Missing background image"));
            m_menuMessage->setMessageType(KMessageWidget::Warning);
            m_menuMessage->animatedShow();
#else
            m_view.error_message->setText(i18n("Missing background image"));
            m_view.error_message->setHidden(false);
#endif
            return false;
        }
    }
    
#if KDE_IS_VERSION(4,7,0)
    m_menuMessage->animatedHide();
#endif

    // check that we have a "Play all" entry
    if (targets.contains(0)) return true;
    // ... or that each video file has a button
    for (int i = m_view.target_list->count() - 1; i > 0; i--) {
        // If there is a vob file entry and it has no button assigned, don't allow to go further
        if (m_view.target_list->itemIcon(i).isNull() == false && !targets.contains(i)) {
#if KDE_IS_VERSION(4,7,0)
            m_menuMessage->setText(i18n("No menu entry for %1", m_view.target_list->itemText(i)));
            m_menuMessage->setMessageType(KMessageWidget::Warning);
            m_menuMessage->animatedShow();
#else
            m_view.error_message->setText(i18n("No menu entry for %1", m_view.target_list->itemText(i)));
            m_view.error_message->setHidden(false);
#endif
            return false;
        }
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

void DvdWizardMenu::setBackToMenu(bool backToMenu)
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            button->setBackMenu(backToMenu);
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
            m_view.back_to_menu->blockSignals(true);
            foundButton = true;
            m_view.tabWidget->widget(0)->setEnabled(true);
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            m_view.target_list->setCurrentIndex(button->target());
            m_view.play_text->setText(button->toPlainText());
            m_view.back_to_menu->setChecked(button->backMenu());
            QFont font = button->font();
            m_view.font_size->setValue(font.pixelSize());
            m_view.font_family->setCurrentFont(font);
            m_view.play_text->blockSignals(false);
            m_view.font_size->blockSignals(false);
            m_view.font_family->blockSignals(false);
            m_view.target_list->blockSignals(false);
            m_view.back_to_menu->blockSignals(false);
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
#if KDE_IS_VERSION(4,6,0)
    if (m_view.use_shadow->isChecked()) {
	QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
	shadow->setBlurRadius(7);
	shadow->setOffset(4, 4);
	button->setGraphicsEffect(shadow);
    }
#endif
    //font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    button->setZValue(4);
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

void DvdWizardMenu::changeProfile(bool isPal)
{
    m_isPal = isPal;
    if (isPal == false) {
	m_finalSize = QSize(720, 480);
        m_width = 640;
        m_height = 480;
    } else {
	m_finalSize = QSize(720, 576);
        m_width = 768;
        m_height = 576;
    }
    updatePreview();
}

void DvdWizardMenu::updatePreview()
{
    m_scene->setProfile(m_width, m_height);
    QMatrix matrix;
    matrix.scale(0.5, 0.5);
    m_view.menu_preview->setMatrix(matrix);
    m_view.menu_preview->setMinimumSize(m_width / 2 + 4, m_height / 2 + 8);

    if (m_color) m_color->setRect(0, 0, m_width, m_height);

    int safeW = m_width / 20;
    int safeH = m_height / 20;
    if (m_safeRect) m_safeRect->setRect(safeW, safeH, m_width - 2 * safeW, m_height - 2 * safeH);
}

void DvdWizardMenu::setTargets(QStringList list, QStringList targetlist)
{
    m_view.target_list->clear();
    m_view.target_list->addItem(i18n("Play All"), "jump title 1");
    int movieCount = 0;
    for (int i = 0; i < list.count(); i++) {
        if (targetlist.at(i).contains("chapter"))
            m_view.target_list->addItem(list.at(i), targetlist.at(i));
        else {
            m_view.target_list->addItem(KIcon("video-x-generic"), list.at(i), targetlist.at(i));
            movieCount++;
        }
    }
    m_view.back_to_menu->setHidden(movieCount == 1);
}

void DvdWizardMenu::checkBackgroundType(int ix)
{
    if (ix == 0) {
        m_view.background_color->setVisible(true);
        m_view.background_image->setVisible(false);
        m_view.loop_movie->setVisible(false);
        if (m_background->scene() != 0) m_scene->removeItem(m_background);
    } else {
        m_view.background_color->setVisible(false);
        m_view.background_image->setVisible(true);
        if (ix == 1) {
            m_view.background_image->clear();
            m_view.background_image->setFilter("*");
            if (m_background->scene() != 0) m_scene->removeItem(m_background);
            m_view.loop_movie->setVisible(false);
        } else {
            if (m_background->scene() != 0) m_scene->removeItem(m_background);
            m_view.background_image->clear();
            m_view.background_image->setFilter("video/mpeg");
            m_view.loop_movie->setVisible(true);
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
        if (m_background->scene() != 0) m_scene->removeItem(m_background);
        return;
    }
    QPixmap pix;

    if (m_view.background_list->currentIndex() == 1) {
        // image background
        if (!pix.load(m_view.background_image->url().path())) {
            if (m_background->scene() != 0) m_scene->removeItem(m_background);
            return;
        }
        pix = pix.scaled(m_width, m_height);
    } else if (m_view.background_list->currentIndex() == 2) {
        // video background
        m_movieLength = -1;
        QString standard = "dv_pal";
        if (!m_isPal) standard = "dv_ntsc";
	Mlt::Profile profile(standard.toUtf8().constData());
	Mlt::Producer *producer = new Mlt::Producer(profile, m_view.background_image->url().path().toUtf8().data());
	if (producer && producer->is_valid()) {
	    pix = QPixmap::fromImage(KThumb::getFrame(producer, 0, m_finalSize.width(), m_width, m_height));
	    m_movieLength = producer->get_length();
	}
	if (producer) delete producer;
    }
    m_background->setPixmap(pix);
    m_scene->addItem(m_background);
}

void DvdWizardMenu::checkBackground()
{
    if (m_view.background_list->currentIndex() != 1) {
        if (m_background->scene() != 0) m_scene->removeItem(m_background);
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
    //font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
}

void DvdWizardMenu::updateColor()
{
    updateColor(m_view.text_color->color());
    m_view.menu_preview->viewport()->update();
}

void DvdWizardMenu::prepareUnderLines()
{
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
	    QRectF r = list.at(i)->sceneBoundingRect();
	    int bottom = r.bottom() - 1;
	    if (bottom % 2 == 1) bottom = bottom - 1;
	    int underlineHeight = r.height() / 10;
	    if (underlineHeight % 2 == 1) underlineHeight = underlineHeight - 1;
	    underlineHeight = qMin(underlineHeight, 10);
	    underlineHeight = qMax(underlineHeight, 2);
	    r.setTop(bottom - underlineHeight);
	    r.adjust(2, 0, -2, 0);
	    QGraphicsRectItem *underline = new QGraphicsRectItem(r);
	    underline->setData(Qt::UserRole, QString("underline"));
	    m_scene->addItem(underline);
	    list.at(i)->setVisible(false);
	}
    }
}

void DvdWizardMenu::resetUnderLines()
{
    QList<QGraphicsItem *> list = m_scene->items();
    QList<QGraphicsItem *> toDelete;
    for (int i = 0; i < list.count(); i++) {
	if (list.at(i)->data(Qt::UserRole).toString() == "underline") {
	    toDelete.append(list.at(i));
	}
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
	    list.at(i)->setVisible(true);
	}
    }
    while (!toDelete.isEmpty()) {
	QGraphicsItem *item = toDelete.takeFirst();
	delete item;
    }
}

void DvdWizardMenu::updateUnderlineColor(QColor c)
{
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->data(Qt::UserRole).toString() == "underline") {
            QGraphicsRectItem *underline = static_cast < QGraphicsRectItem* >(list.at(i));
	    underline->setPen(Qt::NoPen);
	    c.setAlpha(150);
	    underline->setBrush(c);
        }
    }
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
        if (m_safeRect->scene() != 0) m_scene->removeItem(m_safeRect);
        if (m_color->scene() != 0) m_scene->removeItem(m_color);
        if (m_background->scene() != 0) m_scene->removeItem(m_background);
	prepareUnderLines();
#if QT_VERSION >= 0x040800
        QImage img(m_finalSize.width(), m_finalSize.height(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
	updateUnderlineColor(m_view.text_color->color());
#else
	QImage img(m_finalSize.width(), m_finalSize.height(), QImage::Format_Mono);
        img.fill(Qt::white);
	updateUnderlineColor(Qt::black);
#endif
        QPainter p(&img);
        //p.setRenderHints(QPainter::Antialiasing, false);
        //p.setRenderHints(QPainter::TextAntialiasing, false);
        m_scene->render(&p, QRectF(0, 0, m_finalSize.width(), m_finalSize.height()), QRectF(0, 0, m_width, m_height), Qt::IgnoreAspectRatio);
        p.end();
#if QT_VERSION >= 0x040800
#elif QT_VERSION >= 0x040600 
        img.setColor(0, m_view.text_color->color().rgb());
        img.setColor(1, qRgba(0,0,0,0));
#else
        img.setNumColors(4);
#endif
        img.save(img1);

#if QT_VERSION >= 0x040800
        img.fill(Qt::transparent);
	updateUnderlineColor(m_view.highlighted_color->color());
#else
        img.fill(Qt::white);
#endif
        p.begin(&img);
        //p.setRenderHints(QPainter::Antialiasing, false);
        //p.setRenderHints(QPainter::TextAntialiasing, false);
	m_scene->render(&p, QRectF(0, 0, m_finalSize.width(), m_finalSize.height()), QRectF(0, 0, m_width, m_height), Qt::IgnoreAspectRatio);
        p.end();
#if QT_VERSION >= 0x040800
#elif QT_VERSION >= 0x040600
        img.setColor(0, m_view.highlighted_color->color().rgb());
        img.setColor(1, qRgba(0,0,0,0));
#else
        img.setNumColors(4);
#endif
        img.save(img3);

#if QT_VERSION >= 0x040800
        img.fill(Qt::transparent);
	updateUnderlineColor(m_view.selected_color->color());
#else
        img.fill(Qt::white);
#endif
	p.begin(&img);
        //p.setRenderHints(QPainter::Antialiasing, false);
        //p.setRenderHints(QPainter::TextAntialiasing, false);
        m_scene->render(&p, QRectF(0, 0, m_finalSize.width(), m_finalSize.height()), QRectF(0, 0, m_width, m_height), Qt::IgnoreAspectRatio);
        p.end();
#if QT_VERSION >= 0x040800
#elif QT_VERSION >= 0x040600
        img.setColor(0, m_view.selected_color->color().rgb());
        img.setColor(1, qRgba(0,0,0,0));
#else
        img.setNumColors(4);
#endif
        img.save(img2);
        resetUnderLines();
        m_scene->addItem(m_safeRect);
        m_scene->addItem(m_color);
        if (m_view.background_list->currentIndex() > 0) m_scene->addItem(m_background);
    }
}


void DvdWizardMenu::createBackgroundImage(const QString &overlayMenu, const QString &img1)
{
    m_scene->clearSelection();
    if (m_safeRect->scene() != 0) m_scene->removeItem(m_safeRect);
    bool showBg = false;
    QImage img(m_width, m_height, QImage::Format_ARGB32);
    if (menuMovie() && m_background->scene() != 0) {
	showBg = true;
	m_scene->removeItem(m_background);
	if (m_color->scene() != 0) m_scene->removeItem(m_color);
	if (m_safeRect->scene() != 0) m_scene->removeItem(m_safeRect);
	img.fill(Qt::transparent);
    }
    updateColor(m_view.text_color->color());
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing, true);
    p.setRenderHints(QPainter::TextAntialiasing, true);
    m_scene->render(&p, QRectF(0, 0, img.width(), img.height()));
    p.end();
    img.save(img1);
    m_scene->addItem(m_safeRect);
    if (showBg) {
	m_scene->addItem(m_background);
	m_scene->addItem(m_color);
    }
    return;
	
  
    /*QImage img;
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
    // Overlay Normal menu
    QImage menu(overlayMenu);
    QPainter p(&img);
    QRectF target(0, 0, img.width(), img.height());
    QRectF src(0, 0, menu.width(), menu.height());
    p.drawImage(target, menu, src);
    p.end();
    img.save(img1);*/
}

bool DvdWizardMenu::createMenu() const
{
    return m_view.create_menu->isChecked();
}

bool DvdWizardMenu::loopMovie() const
{
    return m_view.loop_movie->isChecked();
}

bool DvdWizardMenu::menuMovie() const
{
    return m_view.background_list->currentIndex() == 2;
}

QString DvdWizardMenu::menuMoviePath() const
{
    return m_view.background_image->url().path();
}

int DvdWizardMenu::menuMovieLength() const
{
  return m_movieLength;
}


QMap <QString, QRect> DvdWizardMenu::buttonsInfo()
{
    QMap <QString, QRect> info;
    QList<QGraphicsItem *> list = m_scene->items();
    double ratio = (double) m_finalSize.width() / m_width;
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
	    QRectF r = button->sceneBoundingRect();
	    QRect adjustedRect(r.x() * ratio, r.y(), r.width() * ratio, r.height());
	    // Make sure y1 is not odd (requested by spumux)
            if (adjustedRect.height() % 2 == 1) adjustedRect.setHeight(adjustedRect.height() + 1);
            if (adjustedRect.y() % 2 == 1) adjustedRect.setY(adjustedRect.y() - 1);
            QString command = button->command();
            if (button->backMenu()) command.prepend("g1 = 999;");
            info.insertMulti(command, adjustedRect);
        }
    }
    return info;
}

bool DvdWizardMenu::isPalMenu() const
{
    return m_isPal;
}

QDomElement DvdWizardMenu::toXml() const
{
    QDomDocument doc;
    QDomElement xml = doc.createElement("menu");
    doc.appendChild(xml);
    xml.setAttribute("enabled", m_view.create_menu->isChecked());
    if (m_view.background_list->currentIndex() == 0) {
        // Color bg
        xml.setAttribute("background_color", m_view.background_color->color().name());
    } else if (m_view.background_list->currentIndex() == 1) {
        // Image bg
        xml.setAttribute("background_image", m_view.background_image->url().path());
    } else {
        // Video bg
        xml.setAttribute("background_video", m_view.background_image->url().path());
    }
    xml.setAttribute("text_color", m_view.text_color->color().name());
    xml.setAttribute("selected_color", m_view.selected_color->color().name());
    xml.setAttribute("highlighted_color", m_view.highlighted_color->color().name());
    xml.setAttribute("text_shadow", (int) m_view.use_shadow->isChecked());

    QList<QGraphicsItem *> list = m_scene->items();
    int buttonCount = 0;

    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
            buttonCount++;
            DvdButton *button = static_cast < DvdButton* >(list.at(i));
            QDomElement xmlbutton = doc.createElement("button");
            xmlbutton.setAttribute("target", button->target());
            xmlbutton.setAttribute("command", button->command());
            xmlbutton.setAttribute("backtomenu", button->backMenu());
            xmlbutton.setAttribute("posx", button->pos().x());
            xmlbutton.setAttribute("posy", button->pos().y());
            xmlbutton.setAttribute("text", button->toPlainText());
            QFont font = button->font();
            xmlbutton.setAttribute("font_size", font.pixelSize());
            xmlbutton.setAttribute("font_family", font.family());
            xml.appendChild(xmlbutton);
        }
    }
    return doc.documentElement();
}


void DvdWizardMenu::loadXml(QDomElement xml)
{
    kDebug() << "// LOADING MENU";
    if (xml.isNull()) return;
    kDebug() << "// LOADING MENU 1";
    m_view.create_menu->setChecked(xml.attribute("enabled").toInt());
    if (xml.hasAttribute("background_color")) {
        m_view.background_list->setCurrentIndex(0);
        m_view.background_color->setColor(xml.attribute("background_color"));
    } else if (xml.hasAttribute("background_image")) {
        m_view.background_list->setCurrentIndex(1);
        m_view.background_image->setUrl(KUrl(xml.attribute("background_image")));
    } else if (xml.hasAttribute("background_video")) {
        m_view.background_list->setCurrentIndex(2);
        m_view.background_image->setUrl(KUrl(xml.attribute("background_video")));
    }

    m_view.text_color->setColor(xml.attribute("text_color"));
    m_view.selected_color->setColor(xml.attribute("selected_color"));
    m_view.highlighted_color->setColor(xml.attribute("highlighted_color"));

    m_view.use_shadow->setChecked(xml.attribute("text_shadow").toInt());

    QDomNodeList buttons = xml.elementsByTagName("button");
    kDebug() << "// LOADING MENU 2" << buttons.count();

    if (buttons.count() > 0) {
        // Clear existing buttons
        QList<QGraphicsItem *> list = m_scene->items();

        for (int i = 0; i < list.count(); i++) {
            if (list.at(i)->type() == QGraphicsItem::UserType + 1) {
                delete list.at(i);
                i--;
            }
        }
    }

    for (int i = 0; i < buttons.count(); i++) {
        QDomElement e = buttons.at(i).toElement();
        // create menu button
        DvdButton *button = new DvdButton(e.attribute("text"));
        QFont font(e.attribute("font_family"));
        font.setPixelSize(e.attribute("font_size").toInt());
#if KDE_IS_VERSION(4,6,0)
	if (m_view.use_shadow->isChecked()) {
	    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
	    shadow->setBlurRadius(7);
	    shadow->setOffset(4, 4);
	    button->setGraphicsEffect(shadow);
	}
#endif

        //font.setStyleStrategy(QFont::NoAntialias);
        button->setFont(font);
        button->setTarget(e.attribute("target").toInt(), e.attribute("command"));
        button->setBackMenu(e.attribute("backtomenu").toInt());
        button->setDefaultTextColor(m_view.text_color->color());
        button->setZValue(4);
        m_scene->addItem(button);
        button->setPos(e.attribute("posx").toDouble(), e.attribute("posy").toDouble());

    }
}

void DvdWizardMenu::slotZoom()
{
    m_view.menu_preview->scale(2.0, 2.0);
}

void DvdWizardMenu::slotUnZoom()
{
    m_view.menu_preview->scale(0.5, 0.5);
}

