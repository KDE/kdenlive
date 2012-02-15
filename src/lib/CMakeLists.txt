
add_subdirectory(audio)

# Hack. kdenlive_SRCS is defined in the sub-scope and added to this scope;
# now we again need to add it to the parent scope.

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  lib/qtimerWithTime.cpp
  PARENT_SCOPE
)

