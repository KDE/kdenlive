#ifndef RENDERPRESETDIALOG_H
#define RENDERPRESETDIALOG_H

#include "ui_editrenderpreset_ui.h"

class RenderPresetModel;
class Monitor;

class RenderPresetDialog : public QDialog, Ui::EditRenderPreset_UI
{
    Q_OBJECT
public:
    enum Mode {
        New = 0,
        Edit,
        SaveAs
    };
    explicit RenderPresetDialog(QWidget *parent, RenderPresetModel *preset = nullptr, Mode mode = Mode::New);
    /** @returns the name that was finally under which the preset has been saved */
    ~RenderPresetDialog();
    QString saveName();

private:
    QString m_saveName;
    QStringList m_uiParams;
    Monitor *m_monitor;

private slots:
    void slotUpdateParams();
};

#endif // RENDERPRESETDIALOG_H
