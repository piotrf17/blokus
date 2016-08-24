PROJECT = blokus
SOURCES = stringpiece.cc tile.cc board.cc game.cc http_server.cc blokus_main.cc
OBJDIR = obj

LIBS = -lglog -lgflags -lmicrohttpd -ljsoncpp

LD = g++
CXX = g++

CXX_FLAGS = -std=c++1y -Wall -Wextra -Werror -O2 -g
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
	rm -rf $(OBJDIR); rm -f $(PROJECT)
