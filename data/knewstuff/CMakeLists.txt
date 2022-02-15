if(KF5NewStuff_VERSION VERSION_GREATER "5.56.0")
  install(FILES kdenlive_renderprofiles.knsrc kdenlive_wipes.knsrc kdenlive_keyboardschemes.knsrc kdenlive_titles.knsrc kdenlive_luts.knsrc kdenlive_effects.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
else()
  install(FILES kdenlive_renderprofiles.knsrc kdenlive_wipes.knsrc kdenlive_keyboardschemes.knsrc kdenlive_titles.knsrc kdenlive_luts.knsrc kdenlive_effects.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
endif()

