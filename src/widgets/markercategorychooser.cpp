#include "markercategorychooser.h"
#include "../bin/model/markerlistmodel.hpp"

#include <KLocalizedString>

MarkerCategoryChooser::MarkerCategoryChooser(QWidget *parent)
    : QComboBox(parent)
    , m_markerListModel(nullptr)
    , m_allowAll(true)
    , m_onlyUsed(false)
{
    refresh();
    connect(this, &MarkerCategoryChooser::changed, this, &MarkerCategoryChooser::refresh);
}

void MarkerCategoryChooser::refresh()
{
    QList<CommentedTime> markers;
    clear();
    if (m_markerListModel) {
        markers = m_markerListModel->getAllMarkers();
    }
    if (!m_markerListModel || (m_onlyUsed && markers.isEmpty())) {
        setEnabled(false);
        setPlaceholderText(i18n("Nothing to select"));
        return;
    }
    setPlaceholderText(QString());
    setEnabled(true);
    if (m_allowAll) {
        addItem(i18n("All Categories"), -1);
    }
    // Set up guide categories
    static std::array<QColor, 9> markerTypes = m_markerListModel->markerTypes;
    QPixmap pixmap(32, 32);

    for (uint i = 0; i < markerTypes.size(); ++i) {
        if (m_onlyUsed && m_markerListModel->getAllMarkers(i).isEmpty()) {
            continue;
        }
        pixmap.fill(markerTypes[size_t(i)]);
        QIcon colorIcon(pixmap);
        addItem(colorIcon, i18n("Category %1", i), i);
    }
}

void MarkerCategoryChooser::setCurrentCategory(int category)
{
    setCurrentIndex(findData(category));
}

int MarkerCategoryChooser::currentCategory()
{
    return currentData().toInt();
}

void MarkerCategoryChooser::setMarkerModel(const MarkerListModel *model)
{
    m_markerListModel = model;
    emit changed();
}

void MarkerCategoryChooser::setAllowAll(bool allowAll)
{
    m_allowAll = allowAll;
    emit changed();
}

void MarkerCategoryChooser::setOnlyUsed(bool onlyUsed)
{
    m_onlyUsed = onlyUsed;
    emit changed();
}
