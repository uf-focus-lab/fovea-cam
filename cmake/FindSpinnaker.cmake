# Find the Spinnaker library
# --------------------------
# Spinnaker_FOUND        - True if Spinnaker was found.
# Spinnaker_LIBRARIES    - The libraries needed to use Spinnaker
# Spinnaker_INCLUDE_DIRS - Location of Spinnaker.h

unset(Spinnaker_FOUND)
unset(Spinnaker_INCLUDE_DIRS)
unset(Spinnaker_LIBRARIES)

find_path(Spinnaker_INCLUDE_DIRS
  NAMES
  Spinnaker.h
  PATHS
  /opt/spinnaker/include/
  /usr/include/spinnaker/
  /usr/local/include/spinnaker/
)
find_library(Spinnaker_LIBRARIES
  NAMES
  Spinnaker
  PATHS
  /usr/lib
  /usr/local/lib
  /opt/spinnaker/lib/
)

message(STATUS "Spinnaker_INCLUDE_DIRS = ${Spinnaker_INCLUDE_DIRS}")
message(STATUS "Spinnaker_LIBRARIES = ${Spinnaker_LIBRARIES}")

if(Spinnaker_INCLUDE_DIRS AND Spinnaker_LIBRARIES)
  message(STATUS "Spinnaker found in the system")
  set(Spinnaker_FOUND 1)
endif(Spinnaker_INCLUDE_DIRS AND Spinnaker_LIBRARIES)
