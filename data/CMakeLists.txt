install(FILES banner.png timeline_nothumbs.png timeline_athumbs.png timeline_vthumbs.png timeline_avthumbs.png transition.png metadata.properties blacklisted_effects.txt blacklisted_transitions.txt DESTINATION ${DATA_INSTALL_DIR}/kdenlive )
install (FILES kdenlive.xpm DESTINATION share/pixmaps)
install (FILES kdenlive.menu DESTINATION share/menu RENAME kdenlive)
install (FILES dark DESTINATION ${DATA_INSTALL_DIR}/kdenlive/themes)
