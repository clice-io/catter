all: a.o b.o

%.o: %.cc
	$(CXX) -DCATTER_TEST=1 -c $< -o $@
