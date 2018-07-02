
# Main executable
add_executable(scinit main.cpp ChildProcess.cpp ChildProcess.h Config.cpp Config.h ConfigParseException.cpp log.h ChildProcessException.cpp ChildProcessException.h)
target_link_libraries(scinit yaml-cpp pthread cap ${Boost_LIBRARIES})