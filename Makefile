run: imp
	./imp

imp: imp.c++ Makefile
	#g++ -std=c++11 -o $@ $<
	g++ -g -std=c++11 -o $@ $<
