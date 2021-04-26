if(KF5CoreAddons_VERSION VERSION_LESS "5.77")
    INSTALL(FILES
      22/add-subtitle.svg
      DESTINATION ${ICON_INSTALL_DIR}/breeze-dark/actions/22)
endif()

if(KF5CoreAddons_VERSION VERSION_LESS "5.80")
    INSTALL(FILES
      16/keyframe.svg
      16/keyframe-add.svg
      16/keyframe-disable.svg
      16/keyframe-duplicate.svg
      16/keyframe-next.svg
      16/keyframe-previous.svg
      16/keyframe-record.svg
      16/keyframe-remove.svg
      16/add-subtitle.svg
      DESTINATION ${ICON_INSTALL_DIR}/breeze-dark/actions/16)

    INSTALL(FILES
      22/keyframe.svg
      22/keyframe-add.svg
      22/keyframe-disable.svg
      22/keyframe-duplicate.svg
      22/keyframe-next.svg
      22/keyframe-previous.svg
      22/keyframe-record.svg
      22/keyframe-remove.svg
      DESTINATION ${ICON_INSTALL_DIR}/breeze-dark/actions/22)
endif()
