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

#include "doc/kthumb.h"
#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <KColorScheme>
#include <QGraphicsDropShadowEffect>

#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

enum { DvdButtonItem = QGraphicsItem::UserType + 1, DvdButtonUnderlineItem = QGraphicsItem::UserType + 2 };

DvdScene::DvdScene(QObject *parent)
    : QGraphicsScene(parent)

{
}
void DvdScene::setProfile(int width, int height)
{
    m_width = width;
    m_height = height;
    setSceneRect(0, 0, m_width, m_height);
}

int DvdScene::gridSize() const
{
    return m_gridSize;
}

void DvdScene::setGridSize(int gridSize)
{
    m_gridSize = gridSize;
}

void DvdScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mouseReleaseEvent(mouseEvent);
    emit sceneChanged();
}

void DvdScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    // draw the grid if needed
    if (gridSize() <= 1) {
        return;
    }

    QPen pen(QColor(255, 0, 0, 100));
    painter->setPen(pen);

    qreal left = int(rect.left()) - (int(rect.left()) % m_gridSize);
    qreal top = int(rect.top()) - (int(rect.top()) % m_gridSize);
    QVector<QPointF> points;
    for (qreal x = left; x < rect.right(); x += m_gridSize) {
        for (qreal y = top; y < rect.bottom(); y += m_gridSize) {
            points.append(QPointF(x, y));
        }
    }
    painter->drawPoints(points.data(), points.size());
}

DvdButton::DvdButton(const QString &text)
    : QGraphicsTextItem(text)
    , m_target(0)
    , m_command(QStringLiteral("jump title 1"))
    , m_backToMenu(false)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}
void DvdButton::setTarget(int t, const QString &c)
{
    m_target = t;
    m_command = c;
}
int DvdButton::target() const
{
    return m_target;
}
QString DvdButton::command() const
{
    return m_command;
}
bool DvdButton::backMenu() const
{
    return m_backToMenu;
}
int DvdButton::type() const
{
    // Enable the use of qgraphicsitem_cast with this item.
    return UserType + 1;
}
void DvdButton::setBackMenu(bool back)
{
    m_backToMenu = back;
}

QVariant DvdButton::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && (scene() != nullptr)) {
        QPoint newPos = value.toPoint();

        if (QApplication::mouseButtons() == Qt::LeftButton && (qobject_cast<DvdScene *>(scene()) != nullptr)) {
            auto *customScene = qobject_cast<DvdScene *>(scene());
            int gridSize = customScene->gridSize();
            int xV = (newPos.x() / gridSize) * gridSize;
            int yV = (newPos.y() / gridSize) * gridSize;
            newPos = QPoint(xV, yV);
        }

        QRectF sceneShape = sceneBoundingRect();
        auto *sc = static_cast<DvdScene *>(scene());
        newPos.setX(qMax(newPos.x(), 0));
        newPos.setY(qMax(newPos.y(), 0));
        if (newPos.x() + sceneShape.width() > sc->width()) {
            newPos.setX(sc->width() - sceneShape.width());
        }
        if (newPos.y() + sceneShape.height() > sc->height()) {
            newPos.setY(sc->height() - sceneShape.height());
        }

        sceneShape.translate(newPos - pos());
        QList<QGraphicsItem *> list = scene()->items(sceneShape, Qt::IntersectsItemShape);
        list.removeAll(this);
        if (!list.isEmpty()) {
            for (int i = 0; i < list.count(); ++i) {
                if (list.at(i)->type() == Type) {
                    return pos();
                }
            }
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

DvdWizardMenu::DvdWizardMenu(DVDFORMAT format, QWidget *parent)
    : QWizardPage(parent)
    , m_color(nullptr)
    , m_safeRect(nullptr)
    , m_finalSize(720, 576)
    , m_movieLength(-1)
{
    m_view.setupUi(this);
    m_view.play_text->setText(i18n("Play"));
    m_scene = new DvdScene(this);
    m_view.menu_preview->setScene(m_scene);
    m_view.menu_preview->setMouseTracking(true);
    connect(m_view.create_menu, &QAbstractButton::toggled, m_view.menu_box, &QWidget::setEnabled);
    connect(m_view.create_menu, &QAbstractButton::toggled, this, &QWizardPage::completeChanged);

    m_view.add_button->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    m_view.delete_button->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    m_view.zoom_button->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    m_view.unzoom_button->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));

    m_view.add_button->setToolTip(i18n("Add new button"));
    m_view.delete_button->setToolTip(i18n("Delete current button"));

    changeProfile(format);

    // Create color background
    m_color = new QGraphicsRectItem(0, 0, m_width, m_height);
    m_color->setBrush(m_view.background_color->color());
    m_color->setZValue(2);
    m_scene->addItem(m_color);

    // create background image
    m_background = new QGraphicsPixmapItem();
    m_background->setZValue(3);
    // m_scene->addItem(m_background);

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
    // font.setStyleStrategy(QFont::NoAntialias);
    if (m_view.use_shadow->isChecked()) {
        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(7);
        shadow->setOffset(4, 4);
        button->setGraphicsEffect(shadow);
    }
    connect(m_view.use_shadow, &QCheckBox::stateChanged, this, &DvdWizardMenu::slotEnableShadows);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    button->setZValue(4);
    QRectF r = button->sceneBoundingRect();
    m_scene->addItem(button);
    button->setPos((m_width - r.width()) / 2, (m_height - r.height()) / 2);
    button->setSelected(true);

    if (m_view.use_grid->isChecked()) {
        m_scene->setGridSize(10);
    }
    connect(m_view.use_grid, &QAbstractButton::toggled, this, &DvdWizardMenu::slotUseGrid);

    // m_view.menu_preview->resizefitInView(0, 0, m_width, m_height);

    connect(m_view.play_text, &QLineEdit::textChanged, this, &DvdWizardMenu::buildButton);
    connect(m_view.text_color, &KColorButton::changed, this, static_cast<void (DvdWizardMenu::*)(const QColor &)>(&DvdWizardMenu::updateColor));
    connect(m_view.font_size, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &DvdWizardMenu::buildButton);
    connect(m_view.font_family, &QFontComboBox::currentFontChanged, this, &DvdWizardMenu::buildButton);
    connect(m_view.background_image, &KUrlRequester::textChanged, this, &DvdWizardMenu::buildImage);
    connect(m_view.background_color, &KColorButton::changed, this, &DvdWizardMenu::buildColor);

    connect(m_view.background_list, static_cast<void (KComboBox::*)(int)>(&KComboBox::currentIndexChanged), this, &DvdWizardMenu::checkBackgroundType);

    connect(m_view.target_list, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &DvdWizardMenu::setButtonTarget);
    connect(m_view.back_to_menu, &QAbstractButton::toggled, this, &DvdWizardMenu::setBackToMenu);

    connect(m_view.add_button, &QAbstractButton::pressed, this, &DvdWizardMenu::addButton);
    connect(m_view.delete_button, &QAbstractButton::pressed, this, &DvdWizardMenu::deleteButton);
    connect(m_view.zoom_button, &QAbstractButton::pressed, this, &DvdWizardMenu::slotZoom);
    connect(m_view.unzoom_button, &QAbstractButton::pressed, this, &DvdWizardMenu::slotUnZoom);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &DvdWizardMenu::buttonChanged);
    connect(m_scene, &DvdScene::sceneChanged, this, &QWizardPage::completeChanged);

    // red background for error message
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
    QPalette p = m_view.error_message->palette();
    p.setColor(QPalette::Background, scheme.background(KColorScheme::NegativeBackground).color());
    m_view.error_message->setAutoFillBackground(true);
    m_view.error_message->setPalette(p);
    m_view.menu_box->setEnabled(false);

    m_menuMessage = new KMessageWidget;
    auto *s = static_cast<QGridLayout *>(layout());
    s->addWidget(m_menuMessage, 7, 0, 1, -1);
    m_menuMessage->hide();
    m_view.error_message->hide();
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
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            if (enable != 0) {
                auto *shadow = new QGraphicsDropShadowEffect(this);
                shadow->setBlurRadius(7);
                shadow->setOffset(4, 4);
                list.at(i)->setGraphicsEffect(shadow);
            } else {
                list.at(i)->setGraphicsEffect(nullptr);
            }
        }
    }
}

void DvdWizardMenu::setButtonTarget(int ix)
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            auto *button = static_cast<DvdButton *>(list.at(i));
            button->setTarget(ix, m_view.target_list->itemData(ix).toString());
            break;
        }
    }
    emit completeChanged();
}

void DvdWizardMenu::setBackToMenu(bool backToMenu)
{
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            auto *button = static_cast<DvdButton *>(list.at(i));
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
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            m_view.play_text->blockSignals(true);
            m_view.font_size->blockSignals(true);
            m_view.font_family->blockSignals(true);
            m_view.target_list->blockSignals(true);
            m_view.back_to_menu->blockSignals(true);
            foundButton = true;
            m_view.tabWidget->widget(0)->setEnabled(true);
            auto *button = static_cast<DvdButton *>(list.at(i));
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
    if (!foundButton) {
        m_view.tabWidget->widget(0)->setEnabled(false);
    }
}

void DvdWizardMenu::addButton()
{
    m_scene->clearSelection();
    DvdButton *button = new DvdButton(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    if (m_view.use_shadow->isChecked()) {
        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(7);
        shadow->setOffset(4, 4);
        button->setGraphicsEffect(shadow);
    }
    // font.setStyleStrategy(QFont::NoAntialias);
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
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            delete list.at(i);
            break;
        }
    }
}

void DvdWizardMenu::changeProfile(DVDFORMAT format)
{
    m_format = format;
    switch (m_format) {
    case PAL_WIDE:
        m_finalSize = QSize(720, 576);
        m_width = 1024;
        m_height = 576;
        break;
    case NTSC_WIDE:
        m_finalSize = QSize(720, 480);
        m_width = 853;
        m_height = 480;
        break;
    case NTSC:
        m_finalSize = QSize(720, 480);
        m_width = 640;
        m_height = 480;
        break;
    default:
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

    if (m_color) {
        m_color->setRect(0, 0, m_width, m_height);
    }

    int safeW = m_width / 20;
    int safeH = m_height / 20;
    if (m_safeRect) {
        m_safeRect->setRect(safeW, safeH, m_width - 2 * safeW, m_height - 2 * safeH);
    }
}

void DvdWizardMenu::setTargets(const QStringList &list, const QStringList &targetlist)
{
    m_view.target_list->clear();
    m_view.target_list->addItem(i18n("Play All"), "jump title 1");
    int movieCount = 0;
    for (int i = 0; i < list.count(); ++i) {
        if (targetlist.at(i).contains(QStringLiteral("chapter"))) {
            m_view.target_list->addItem(list.at(i), targetlist.at(i));
        } else {
            m_view.target_list->addItem(QIcon::fromTheme(QStringLiteral("video-x-generic")), list.at(i), targetlist.at(i));
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
        if (m_background->scene() != nullptr) {
            m_scene->removeItem(m_background);
        }
    } else {
        m_view.background_color->setVisible(false);
        m_view.background_image->setVisible(true);
        if (ix == 1) {
            m_view.background_image->clear();
            m_view.background_image->setFilter(QStringLiteral("*"));
            if (m_background->scene() != nullptr) {
                m_scene->removeItem(m_background);
            }
            m_view.loop_movie->setVisible(false);
        } else {
            if (m_background->scene() != nullptr) {
                m_scene->removeItem(m_background);
            }
            m_view.background_image->clear();
            m_view.background_image->setMimeTypeFilters({QStringLiteral("video/mpeg")});
            m_view.loop_movie->setVisible(true);
        }
    }
    emit completeChanged();
}

void DvdWizardMenu::buildColor()
{
    m_color->setBrush(m_view.background_color->color());
}

void DvdWizardMenu::slotUseGrid(bool useGrid)
{
    if (useGrid) {
        m_scene->setGridSize(10);
    } else {
        m_scene->setGridSize(1);
    }
    m_scene->update();
}

void DvdWizardMenu::buildImage()
{
    emit completeChanged();
    if (m_view.background_image->url().isEmpty()) {
        if (m_background->scene() != nullptr) {
            m_scene->removeItem(m_background);
        }
        return;
    }
    QPixmap pix;

    if (m_view.background_list->currentIndex() == 1) {
        // image background
        if (!pix.load(m_view.background_image->url().toLocalFile())) {
            if (m_background->scene() != nullptr) {
                m_scene->removeItem(m_background);
            }
            return;
        }
        pix = pix.scaled(m_width, m_height);
    } else if (m_view.background_list->currentIndex() == 2) {
        // video background
        m_movieLength = -1;
        QString profileName = DvdWizardVob::getDvdProfile(m_format);
        Mlt::Profile profile(profileName.toUtf8().constData());
        profile.set_explicit(1);
        Mlt::Producer *producer = new Mlt::Producer(profile, m_view.background_image->url().toLocalFile().toUtf8().constData());
        if ((producer != nullptr) && producer->is_valid()) {
            pix = QPixmap::fromImage(KThumb::getFrame(producer, 0, m_width, m_height));
            m_movieLength = producer->get_length();
        }
        delete producer;
    }
    m_background->setPixmap(pix);
    m_scene->addItem(m_background);
}

void DvdWizardMenu::buildButton()
{
    DvdButton *button = nullptr;
    QList<QGraphicsItem *> list = m_scene->selectedItems();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            button = static_cast<DvdButton *>(list.at(i));
            break;
        }
    }
    if (button == nullptr) {
        return;
    }
    button->setPlainText(m_view.play_text->text());
    QFont font = m_view.font_family->currentFont();
    font.setPixelSize(m_view.font_size->value());
    // font.setStyleStrategy(QFont::NoAntialias);
    button->setFont(font);
    button->setDefaultTextColor(m_view.text_color->color());
    // Check for button overlapping in case we changed text / size
    emit completeChanged();
}

void DvdWizardMenu::updateColor()
{
    updateColor(m_view.text_color->color());
    m_view.menu_preview->viewport()->update();
}

void DvdWizardMenu::prepareUnderLines()
{
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            QRectF r = list.at(i)->sceneBoundingRect();
            int bottom = r.bottom() - 1;
            if (bottom % 2 == 1) {
                bottom = bottom - 1;
            }
            int underlineHeight = r.height() / 10;
            if (underlineHeight % 2 == 1) {
                underlineHeight = underlineHeight - 1;
            }
            underlineHeight = qMin(underlineHeight, 10);
            underlineHeight = qMax(underlineHeight, 2);
            r.setTop(bottom - underlineHeight);
            r.setBottom(bottom);
            r.adjust(2, 0, -2, 0);
            auto *underline = new DvdButtonUnderline(r);
            m_scene->addItem(underline);
            list.at(i)->setVisible(false);
        }
    }
}

void DvdWizardMenu::resetUnderLines()
{
    QList<QGraphicsItem *> list = m_scene->items();
    QList<QGraphicsItem *> toDelete;
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonUnderlineItem) {
            toDelete.append(list.at(i));
        }
        if (list.at(i)->type() == DvdButtonItem) {
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
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonUnderlineItem) {
            auto *underline = static_cast<DvdButtonUnderline *>(list.at(i));
            underline->setPen(Qt::NoPen);
            c.setAlpha(150);
            underline->setBrush(c);
        }
    }
}

void DvdWizardMenu::updateColor(const QColor &c)
{
    DvdButton *button = nullptr;
    QList<QGraphicsItem *> list = m_scene->items();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            button = static_cast<DvdButton *>(list.at(i));
            button->setDefaultTextColor(c);
        }
    }
}

void DvdWizardMenu::createButtonImages(const QString &selected_image, const QString &highlighted_image, bool letterbox)
{
    if (m_view.create_menu->isChecked()) {
        m_scene->clearSelection();
        QRectF source(0, 0, m_width, m_height);
        QRectF target;
        if (!letterbox) {
            target = QRectF(0, 0, m_finalSize.width(), m_finalSize.height());
        } else {
            // Scale the button images to fit a letterbox image
            double factor = (double)m_width / m_finalSize.width();
            int letterboxHeight = m_height / factor;
            target = QRectF(0, (m_finalSize.height() - letterboxHeight) / 2, m_finalSize.width(), letterboxHeight);
        }
        if (m_safeRect->scene() != nullptr) {
            m_scene->removeItem(m_safeRect);
        }
        if (m_color->scene() != nullptr) {
            m_scene->removeItem(m_color);
        }
        if (m_background->scene() != nullptr) {
            m_scene->removeItem(m_background);
        }
        prepareUnderLines();
        QImage img(m_finalSize.width(), m_finalSize.height(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        updateUnderlineColor(m_view.highlighted_color->color());

        QPainter p;
        p.begin(&img);
        // p.setRenderHints(QPainter::Antialiasing, false);
        // p.setRenderHints(QPainter::TextAntialiasing, false);
        m_scene->render(&p, target, source, Qt::IgnoreAspectRatio);
        p.end();
        img.setColor(0, m_view.highlighted_color->color().rgb());
        img.setColor(1, qRgba(0, 0, 0, 0));
        img.save(highlighted_image);
        img.fill(Qt::transparent);
        updateUnderlineColor(m_view.selected_color->color());

        p.begin(&img);
        // p.setRenderHints(QPainter::Antialiasing, false);
        // p.setRenderHints(QPainter::TextAntialiasing, false);
        m_scene->render(&p, target, source, Qt::IgnoreAspectRatio);
        p.end();
        img.setColor(0, m_view.selected_color->color().rgb());
        img.setColor(1, qRgba(0, 0, 0, 0));
        img.save(selected_image);
        resetUnderLines();
        m_scene->addItem(m_safeRect);
        m_scene->addItem(m_color);
        if (m_view.background_list->currentIndex() > 0) {
            m_scene->addItem(m_background);
        }
    }
}

void DvdWizardMenu::createBackgroundImage(const QString &img1, bool letterbox)
{
    Q_UNUSED(letterbox)

    m_scene->clearSelection();
    if (m_safeRect->scene() != nullptr) {
        m_scene->removeItem(m_safeRect);
    }
    bool showBg = false;
    QImage img(m_width, m_height, QImage::Format_ARGB32);

    // TODO: Should the image be scaled when letterboxing?
    /*
    QRectF source(0, 0, m_width, m_height);
    QRectF target;
    if (!letterbox) target = QRectF(0, 0, m_finalSize.width(), m_finalSize.height());
    else {
    // Scale the button images to fit a letterbox image
    double factor = (double) m_width / m_finalSize.width();
    int letterboxHeight = m_height / factor;
    target = QRectF(0, (m_finalSize.height() - letterboxHeight) / 2, m_finalSize.width(), letterboxHeight);
    }*/

    if (menuMovie()) {
        showBg = true;
        if (m_background->scene() != nullptr) {
            m_scene->removeItem(m_background);
        }
        if (m_color->scene() != nullptr) {
            m_scene->removeItem(m_color);
        }
        img.fill(Qt::transparent);
    }
    updateColor(m_view.text_color->color());
    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing, true);
    p.setRenderHints(QPainter::TextAntialiasing, true);
    // set image grid to "1" to ensure we don't display dots all over
    // the image when rendering
    int oldSize = m_scene->gridSize();
    m_scene->setGridSize(1);
    m_scene->render(&p, QRectF(0, 0, img.width(), img.height()));
    m_scene->setGridSize(oldSize);
    // m_scene->render(&p, target, source, Qt::IgnoreAspectRatio);
    p.end();
    img.save(img1);
    m_scene->addItem(m_safeRect);
    if (showBg) {
        m_scene->addItem(m_background);
        m_scene->addItem(m_color);
    }
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
    return m_view.background_image->url().toLocalFile();
}

int DvdWizardMenu::menuMovieLength() const
{
    return m_movieLength;
}

QMap<QString, QRect> DvdWizardMenu::buttonsInfo(bool letterbox)
{
    QMap<QString, QRect> info;
    QList<QGraphicsItem *> list = m_scene->items();
    double ratiox = (double)m_finalSize.width() / m_width;
    double ratioy = 1;
    int offset = 0;
    if (letterbox) {
        int letterboxHeight = m_height * ratiox;
        ratioy = (double)letterboxHeight / m_finalSize.height();
        offset = (m_finalSize.height() - letterboxHeight) / 2;
    }
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            auto *button = static_cast<DvdButton *>(list.at(i));
            QRectF r = button->sceneBoundingRect();
            QRect adjustedRect(r.x() * ratiox, offset + r.y() * ratioy, r.width() * ratiox, r.height() * ratioy);
            // Make sure y1 is not odd (requested by spumux)
            if (adjustedRect.height() % 2 == 1) {
                adjustedRect.setHeight(adjustedRect.height() + 1);
            }
            if (adjustedRect.y() % 2 == 1) {
                adjustedRect.setY(adjustedRect.y() - 1);
            }
            QString command = button->command();
            if (button->backMenu()) {
                command.prepend(QStringLiteral("g1 = 999;"));
            }
            info.insertMulti(command, adjustedRect);
        }
    }
    return info;
}

QDomElement DvdWizardMenu::toXml() const
{
    QDomDocument doc;
    QDomElement xml = doc.createElement(QStringLiteral("menu"));
    doc.appendChild(xml);
    xml.setAttribute(QStringLiteral("enabled"), static_cast<int>(m_view.create_menu->isChecked()));
    if (m_view.background_list->currentIndex() == 0) {
        // Color bg
        xml.setAttribute(QStringLiteral("background_color"), m_view.background_color->color().name());
    } else if (m_view.background_list->currentIndex() == 1) {
        // Image bg
        xml.setAttribute(QStringLiteral("background_image"), m_view.background_image->url().toLocalFile());
    } else {
        // Video bg
        xml.setAttribute(QStringLiteral("background_video"), m_view.background_image->url().toLocalFile());
    }
    xml.setAttribute(QStringLiteral("text_color"), m_view.text_color->color().name());
    xml.setAttribute(QStringLiteral("selected_color"), m_view.selected_color->color().name());
    xml.setAttribute(QStringLiteral("highlighted_color"), m_view.highlighted_color->color().name());
    xml.setAttribute(QStringLiteral("text_shadow"), (int)m_view.use_shadow->isChecked());

    QList<QGraphicsItem *> list = m_scene->items();
    int buttonCount = 0;

    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->type() == DvdButtonItem) {
            buttonCount++;
            auto *button = static_cast<DvdButton *>(list.at(i));
            QDomElement xmlbutton = doc.createElement(QStringLiteral("button"));
            xmlbutton.setAttribute(QStringLiteral("target"), button->target());
            xmlbutton.setAttribute(QStringLiteral("command"), button->command());
            xmlbutton.setAttribute(QStringLiteral("backtomenu"), static_cast<int>(button->backMenu()));
            xmlbutton.setAttribute(QStringLiteral("posx"), (int)button->pos().x());
            xmlbutton.setAttribute(QStringLiteral("posy"), (int)button->pos().y());
            xmlbutton.setAttribute(QStringLiteral("text"), button->toPlainText());
            QFont font = button->font();
            xmlbutton.setAttribute(QStringLiteral("font_size"), font.pixelSize());
            xmlbutton.setAttribute(QStringLiteral("font_family"), font.family());
            xml.appendChild(xmlbutton);
        }
    }
    return doc.documentElement();
}

void DvdWizardMenu::loadXml(DVDFORMAT format, const QDomElement &xml)
{
    // qCDebug(KDENLIVE_LOG) << "// LOADING MENU";
    if (xml.isNull()) {
        return;
    }
    // qCDebug(KDENLIVE_LOG) << "// LOADING MENU 1";
    changeProfile(format);
    m_view.create_menu->setChecked(xml.attribute(QStringLiteral("enabled")).toInt() != 0);
    if (xml.hasAttribute(QStringLiteral("background_color"))) {
        m_view.background_list->setCurrentIndex(0);
        m_view.background_color->setColor(xml.attribute(QStringLiteral("background_color")));
    } else if (xml.hasAttribute(QStringLiteral("background_image"))) {
        m_view.background_list->setCurrentIndex(1);
        m_view.background_image->setUrl(QUrl(xml.attribute(QStringLiteral("background_image"))));
    } else if (xml.hasAttribute(QStringLiteral("background_video"))) {
        m_view.background_list->setCurrentIndex(2);
        m_view.background_image->setUrl(QUrl(xml.attribute(QStringLiteral("background_video"))));
    }

    m_view.text_color->setColor(xml.attribute(QStringLiteral("text_color")));
    m_view.selected_color->setColor(xml.attribute(QStringLiteral("selected_color")));
    m_view.highlighted_color->setColor(xml.attribute(QStringLiteral("highlighted_color")));

    m_view.use_shadow->setChecked(xml.attribute(QStringLiteral("text_shadow")).toInt() != 0);

    QDomNodeList buttons = xml.elementsByTagName(QStringLiteral("button"));
    // qCDebug(KDENLIVE_LOG) << "// LOADING MENU 2" << buttons.count();

    if (!buttons.isEmpty()) {
        // Clear existing buttons
        for (QGraphicsItem *item : m_scene->items()) {
            if (item->type() == DvdButtonItem) {
                m_scene->removeItem(item);
                delete item;
            }
        }
    }

    for (int i = 0; i < buttons.count(); ++i) {
        QDomElement e = buttons.at(i).toElement();
        // create menu button
        DvdButton *button = new DvdButton(e.attribute(QStringLiteral("text")));
        QFont font(e.attribute(QStringLiteral("font_family")));
        font.setPixelSize(e.attribute(QStringLiteral("font_size")).toInt());
        if (m_view.use_shadow->isChecked()) {
            auto *shadow = new QGraphicsDropShadowEffect(this);
            shadow->setBlurRadius(7);
            shadow->setOffset(4, 4);
            button->setGraphicsEffect(shadow);
        }

        // font.setStyleStrategy(QFont::NoAntialias);
        button->setFont(font);
        button->setTarget(e.attribute(QStringLiteral("target")).toInt(), e.attribute(QStringLiteral("command")));
        button->setBackMenu(e.attribute(QStringLiteral("backtomenu")).toInt() != 0);
        button->setDefaultTextColor(m_view.text_color->color());
        button->setZValue(4);
        m_scene->addItem(button);
        button->setPos(e.attribute(QStringLiteral("posx")).toInt(), e.attribute(QStringLiteral("posy")).toInt());
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
