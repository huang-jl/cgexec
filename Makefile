build: cgexec.cpp
	g++ -O2 -Wall cgexec.cpp -o cgexec

install: build
	mv cgexec /usr/local/bin/cgexec

