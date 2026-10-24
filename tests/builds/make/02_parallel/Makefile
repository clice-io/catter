SOURCES := $(sort $(wildcard src/*.c src/*.cc))
OBJECTS := $(addsuffix .o,$(basename $(SOURCES)))
CPPFLAGS = -DCATTER_TEST=1

libmany.a: $(OBJECTS)
	$(AR) rcs $@ $^
