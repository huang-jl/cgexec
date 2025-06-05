build: cgexec.cpp
	g++ -O2 -Wall cgexec.cpp -o cgexec

.PHONY: install
install: build
	mv cgexec /usr/local/bin/cgexec

