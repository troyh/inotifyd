CXXFLAGS=-I/usr/include/libxml2

all: inotifyd fsevents

inotifyd: inotifyd.o 
	g++ -o $@ $^ -lc -lboost_filesystem -lxml2
	
fsevents: fsevents.o
	g++ -o $@ $^ -lc

clean:
	rm -f *.o inotifyd
	
install: /usr/sbin/inotifyd /usr/bin/fsevents /etc/init.d/inotifyd /etc/rc2.d/S15inotifyd

/etc/rc2.d/S15inotifyd: /etc/init.d/inotifyd
	if [ ! -f $@ ]; then ln -s /etc/init.d/inotifyd $@; fi
	
/usr/sbin/inotifyd: inotifyd
	cp inotifyd $@

/etc/init.d/inotifyd: install/inotifyd.init.sh
	cp $< $@
	
/usr/bin/fsevents: fsevents
	cp $< $@
