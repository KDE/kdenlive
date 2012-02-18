
add_subdirectory(audio)
add_subdirectory(external)

# Hack. kdenlive_SRCS is defined in the sub-scope and added to this scope;
# now we again need to add it to the parent scope.

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  lib/qtimerWithTime.cpp
  PARENT_SCOPE
)

