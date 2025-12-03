APPNAME ?= raylib
COMPILED_FORMAT ?= .rc.data # raylib uses .rc.data for compiled rc data

.PHONY: clean all

all: $(APPNAME).rc.data

$(APPNAME).rc.data:
	windres $(APPNAME).rc -O coff -o $(APPNAME)$(COMPILED_FORMAT) # `-O coff` for Common Objcet File format (gcc), for MCVC use `-O res`

clean:
	rm $(APPNAME)$(COMPILED_FORMAT)