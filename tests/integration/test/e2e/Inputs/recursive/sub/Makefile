all: a.o b.o

%.o: %.cc
	$(CXX) -DCATTER_E2E=1 -c $< -o $@
