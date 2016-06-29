PROJECT = blokus
SOURCES = tile.cc board.cc game.cc blokus_main.cc
OBJDIR = obj

LIBS = -lglog -lgflags

LD = g++
CXX = g++

CXX_FLAGS = -std=c++14 -Wall -Wextra -Werror -O2 -g
LD_FLAGS = $(LIBS)

#=========== Implementation details below ==============#

OBJFILES = $(patsubst %.cc, $(OBJDIR)/%.o, $(SOURCES))

$(PROJECT): $(OBJFILES)
	@echo +ld $(@)
	$(LD) $(OBJFILES) $(LD_FLAGS) -o $@

$(OBJDIR)/%.o: %.cc
	@echo +cc $<
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

clean:
	rrm -rf $(OBJDIR); rm -f $(PROJECT)
