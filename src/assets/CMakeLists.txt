set(kdenlive_SRCS
  ${kdenlive_SRCS}
  assets/assetlist/view/qmltypes/asseticonprovider.cpp
  assets/assetlist/view/assetlistwidget.cpp
  assets/assetlist/model/assetfilter.cpp
  assets/assetlist/model/assettreemodel.cpp
  assets/assetpanel.cpp
  assets/keyframes/model/keyframemodel.cpp
  assets/keyframes/model/keyframemodellist.cpp
  assets/keyframes/view/keyframeview.cpp
  assets/model/assetparametermodel.cpp
  assets/model/assetcommand.cpp
  assets/view/assetparameterview.cpp
  assets/view/widgets/abstractparamwidget.cpp
  #assets/view/widgets/animationwidget.cpp
  assets/view/widgets/boolparamwidget.cpp
  assets/view/widgets/doubleparamwidget.cpp
  assets/view/widgets/colorwheel.cpp
  assets/view/widgets/lumaliftgainparam.cpp
#  assets/view/widgets/keyframeedit.cpp
  assets/view/widgets/keyframewidget.cpp
  assets/view/widgets/listparamwidget.cpp
  assets/view/widgets/geometryeditwidget.cpp
  assets/view/widgets/positioneditwidget.cpp
  assets/view/widgets/coloreditwidget.cpp
  PARENT_SCOPE)

