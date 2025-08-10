// This proxy compilation unit pulls in the library source files so the Arduino
// build system (which only auto-compiles .cpp files located in the sketch
// folder) links them into the final firmware.

#include "../lib/display.cpp"
#include "../lib/State.cpp"
#include "../lib/Comms.cpp"
#include "../lib/AsyncTask.cpp"
