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
    void setCurrentCategory(int category);
    int currentCategory();
    void setMarkerModel(const MarkerListModel *model);
    void setAllowAll(bool allowAll);
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
