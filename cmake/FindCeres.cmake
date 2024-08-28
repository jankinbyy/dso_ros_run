find_package(Ceres REQUIRED QUIET)
message("Ceres version:" ${Ceres_VERSION})
include_directories(${CERES_INCLUDE_DIRS})
list(APPEND ALL_TARGET_LIBRARIES ${CERES_LIBRARIES})
