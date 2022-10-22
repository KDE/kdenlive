#ifndef MARKERCATEGORYCHOOSER_H
#define MARKERCATEGORYCHOOSER_H

#include <QComboBox>
#include <QWidget>

class MarkerListModel;

class MarkerCategoryChooser : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(bool allowAll READ allowAll WRITE setAllowAll NOTIFY changed)
    Q_PROPERTY(bool onlyUsed READ onlyUsed WRITE setOnlyUsed NOTIFY changed)

public:
    MarkerCategoryChooser(QWidget *parent = nullptr);
    /** @brief Set currently selected category by its number. */
    void setCurrentCategory(int category);
    /** @brief get the number of the currently selected category. */
    int currentCategory();
    /** @brief Set the marker model of the chooser. Only needed if @property onlyUsed is true.*/
    void setMarkerModel(const MarkerListModel *model);
    /** @brief Whether the user should be able to select "All Categories" */
    void setAllowAll(bool allowAll);
    /** @brief Show only categories that are used by markers in the model.
     *  If no model is set, all categories will be show. @see setMarkerModel
     */
    void setOnlyUsed(bool onlyUsed);

private:
    const MarkerListModel *m_markerListModel;
    bool m_allowAll;
    bool m_onlyUsed;

    void refresh();
    bool allowAll() { return m_allowAll; };
    bool onlyUsed() { return m_onlyUsed; };

signals:
    void changed();
};

#endif // MARKERCATEGORYCHOOSER_H
