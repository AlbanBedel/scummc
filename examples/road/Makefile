
host=$(shell uname -n | cut -d . -f 1)

SRCDIR?=../..

include $(SRCDIR)/config.$(host).mak

EXP_DIR=$(SRCDIR)/examples/road

REF_FILE=$(EXP_DIR)/road.md5

BIN_PATH=$(SRCDIR)/$(BUILDROOT)/$(TARGET)

SCC=$(BIN_PATH)/scc
SLD=$(BIN_PATH)/sld
COST=$(BIN_PATH)/cost
CHAR=$(BIN_PATH)/char

SCC_FLAGS= -I $(SRCDIR) -I $(EXP_DIR) -R $(EXP_DIR)

all: scummc.000 scummc.001 scummc.sou


%.cost: $(EXP_DIR)/%.scost $(COST)
	$(COST) -o $@ -I $(EXP_DIR) $<

%.char: $(EXP_DIR)/%.bmp $(CHAR)
	$(CHAR) -ochar $@ -ibmp $<

%.roobj: $(EXP_DIR)/%.scumm $(SCC)
	$(SCC) -o $@ $(SCC_FLAGS) $<

scummc.000: road.roobj $(SLD)
	$(SLD) -v $<

scummc.001 scummc.sou: scummc.000

clean:
	rm -f road.dep scummc.0?? *.char scummc.sou *.roobj *.cost

distclean: clean


road.dep: $(EXP_DIR)/road.scumm $(SCC)
	$(SCC) -d -o $@ $(SCC_FLAGS) $<

-include road.dep

### Regression testing

## Update the reference hash.
test-ref: scummc.000 scummc.001 scummc.sou
	@for f in $+ ; do printf "%s: " $$f ; $(MD5SUM) < $$f | cut -d ' ' -f 1 ; done > $(REF_FILE)
	@echo "Reference hash updated."

test: scummc.000 scummc.001 scummc.sou
	@[ -f $(REF_FILE) ] || ( echo "No reference hash available" ; false )
	@for f in $+ ; do \
           printf "%s: " $$f ; \
           $(MD5SUM) < $$f | cut -d ' ' -f 1 ; \
        done | diff -u $(REF_FILE) -

.PHONY: clean distclean test-ref test
