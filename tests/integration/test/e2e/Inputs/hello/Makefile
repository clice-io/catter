# main.o is compiled by make directly; util.o goes through `cd ... &&`, which
# makes make run the recipe with /bin/sh -c, so the capture has to follow the
# shell into its child.
CXXFLAGS = -DCATTER_E2E=1 -Iinclude

hello: main.o util.o
	$(CXX) main.o util.o -o hello

main.o: src/main.cc include/util.h
	$(CXX) $(CXXFLAGS) -c src/main.cc -o main.o

util.o: src/util.cc include/util.h
	cd src && $(CXX) -DCATTER_E2E=1 -I../include -c util.cc -o ../util.o
