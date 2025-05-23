# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>, Julius Künzel <julius.kuenzel@kde.org>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  widgets/choosecolorwidget.cpp
  widgets/colorpickerwidget.cpp
  widgets/doublewidget.cpp
  widgets/dragvalue.cpp
  widgets/elidedfilelinklabel.cpp
  widgets/geometrywidget.cpp
  widgets/markercategorychooser.cpp
  widgets/markercategorybutton.cpp
  widgets/positionwidget.cpp
  widgets/precisionspinbox.cpp
  widgets/progressbutton.cpp
  widgets/timecodedisplay.cpp
  widgets/subtitletextedit.cpp
  PARENT_SCOPE
)

include(ECMAddQtDesignerPlugin)

if (BUILD_DESIGNERPLUGIN)
    ecm_qtdesignerplugin_widget(TimecodeDisplay
        INCLUDE_FILE widgets/timecodedisplay.h
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(ColorPickerWidget
        INCLUDE_FILE widgets/colorpickerwidget.h
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(ChooseColorWidget
        INCLUDE_FILE widgets/choosecolorwidget.h
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(MarkerCategoryChooser
        INCLUDE_FILE widgets/markercategorychooser.h
        TOOLTIP "A QComboBox that can be used to select a marker category (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(MarkerCategoryButton
        INCLUDE_FILE widgets/markercategorybutton.h
        TOOLTIP "A QToolButton that can be used to select one or several marker categories (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(PrecisionSpinBox
        INCLUDE_FILE widgets/precisionspinbox.h
        TOOLTIP "A double spinbox allowing an arbitrary nubmer of decimals (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(ElidedFileLinkLabel
        INCLUDE_FILE widgets/elidedfilelinklabel.h
        TOOLTIP "An elided label with clickable text to open a and highlight a file in the file explorer (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_add_qtdesignerplugin(kdenlivewidgets
        NAME KdenliveWidgets
        WIDGETS
            TimecodeDisplay
            ColorPickerWidget
            ChooseColorWidget
            MarkerCategoryChooser
            MarkerCategoryButton
            PrecisionSpinBox
            ElidedFileLinkLabel
        LINK_LIBRARIES
            kdenliveLib
        INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
        COMPONENT Devel
    )
endif()
