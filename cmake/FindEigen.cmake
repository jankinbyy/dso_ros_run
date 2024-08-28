#include_directories("third_party/eigen3")
#set(EIGEN3_INCLUDE_DIR "/usr/include/eigen3")

find_package(Eigen3 REQUIRED)
message("Eigen3 Version:" ${Eigen3_VERSION})
include_directories(${EIGEN3_INCLUDE_DIR})
message(${EIGEN3_INCLUDE_DIR})

