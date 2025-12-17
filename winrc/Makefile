RCFILE ?= raylib
COMPILED_FORMAT ?= .rc.data # raylib uses .rc.data for compiled rc data

.PHONY: clean all

all: $(RCFILE).rc.data

# `-O coff` for Common Object File format (gcc), for MCVC use `-O res`
$(RCFILE).rc.data:
	windres $(RCFILE).rc -O coff -o $(RCFILE)$(COMPILED_FORMAT)

clean:
	rm $(RCFILE)$(COMPILED_FORMAT)