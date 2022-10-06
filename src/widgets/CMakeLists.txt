set(kdenlive_SRCS
  ${kdenlive_SRCS}
  widgets/choosecolorwidget.cpp
  widgets/colorpickerwidget.cpp
  widgets/doublewidget.cpp
  widgets/dragvalue.cpp
  widgets/geometrywidget.cpp
  widgets/positionwidget.cpp
  widgets/progressbutton.cpp
  widgets/timecodedisplay.cpp
  PARENT_SCOPE
)

include(ECMAddQtDesignerPlugin)

if (BUILD_DESIGNERPLUGIN)
    ecm_qtdesignerplugin_widget(TimecodeDisplay
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(ColorPickerWidget
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_qtdesignerplugin_widget(ChooseColorWidget
        TOOLTIP "A custom QSpinBox widget to enter timecodes (Kdenlive)"
        GROUP "Kdenlive Widgets"
    )

    ecm_add_qtdesignerplugin(kdenlivewidgets
        NAME KdenliveWidgets
        WIDGETS
            TimecodeDisplay
            ColorPickerWidget
            ChooseColorWidget
        LINK_LIBRARIES
            kdenliveLib
        INSTALL_DESTINATION "${KDE_INSTALL_QTPLUGINDIR}/designer"
        COMPONENT Devel
    )
endif()
