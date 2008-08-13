CXXFLAGS=-I/usr/include/libxml2

inotifyd: inotifyd.o Config.o
	g++ -o $@ $^ -lc -lboost_filesystem -lxml2
	
Config.o: Config.cc Config.h
