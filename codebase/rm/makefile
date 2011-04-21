
include ../makefile.inc

all: tags librm.a rmtest test_tuple

tags: *.h *.cc
	-@rm tags
	-@ctags *.h *.cc

# lib file dependencies
librm.a: librm.a(rm.o)  # and possibly other .o files

# c file dependencies
rm.o: rm.h

rmtest.o: rm.h

# binary dependencies
rmtest: rmtest.o librm.a $(CODEROOT)/pf/libpf.a

test_tuple: test_tuple.cc rm.h librm.a $(CODEROOT)/pf/libpf.a
	$(CC) -o test_tuple test_tuple.cc librm.a $(CODEROOT)/pf/libpf.a

# dependencies to compile used libraries
.PHONY: $(CODEROOT)/pf/libpf.a
$(CODEROOT)/pf/libpf.a:
	$(MAKE) -C $(CODEROOT)/pf libpf.a

.PHONY: clean
clean:
	-rm test_tuple test_tuple.data rmtest *.a *.o *~
	$(MAKE) -C $(CODEROOT)/pf clean
