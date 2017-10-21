#
#  ScummC example.mak
#  Copyright (C) 2008  Alban Bedel
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

# Default to v6 games
TARGET_VERSION ?= 6

# Load the distrib config if there is one
ifneq ($(wildcard $(MAK_DIR)/scummc-config.mak),)
include $(MAK_DIR)/scummc-config.mak
# Set some default values
GAME_DIR   ?= .
SCUMMC_DIR ?= ../..
BIN_DIR    ?= $(SCUMMC_DIR)/bin
INC_DIR    ?= $(SCUMMC_DIR)/include
MD5SUM     ?= $(shell type md5sum > /dev/null 2>&1 && echo md5sum)
else # Take the config from the build environement

host=$(shell uname -n | cut -d . -f 1)

SRCDIR ?= ../..

include $(SRCDIR)/config.$(host).mak

GAME_DIR = $(SRCDIR)/examples/$(GAME_NAME)
BIN_DIR  = $(SRCDIR)/$(BUILDROOT)/$(TARGET)
INC_DIR  = $(SRCDIR)/include

# For $(EXESUF)
include $(BIN_DIR)/config.mak

endif


COST_DIR ?= $(GAME_DIR)/costumes
FONT_DIR ?= $(GAME_DIR)/fonts
SOUND_DIR ?= $(GAME_DIR)/sounds

REF_FILE = $(GAME_DIR)/$(GAME_NAME).md5

SCC  ?= $(BIN_DIR)/scc$(EXESUF)
SLD  ?= $(BIN_DIR)/sld$(EXESUF)
COST ?= $(BIN_DIR)/cost$(EXESUF)
CHAR ?= $(BIN_DIR)/char$(EXESUF)
SOUN ?= $(BIN_DIR)/soun$(EXESUF)

SCC_FLAGS ?= -V $(TARGET_VERSION) \
             -I $(INC_DIR) \
             -I $(GAME_DIR) \
             -R $(GAME_DIR) \

SLD_FLAGS ?=

OBJS = $(SRCS:%.scc=%.roobj)

OUT_NAME ?= scummc$(TARGET_VERSION)

OUT_FILES = $(OUT_NAME).000 \
            $(OUT_NAME).001 \
            $(OUT_NAME).sou \

all: $(OUT_FILES)

%.cost: $(COST_DIR)/%.scost $(COST)
	$(COST) -o $@ -I $(COST_DIR) \
                -header $*_anim.sch -prefix $*_anim_ $<

%.akos: $(COST_DIR)/%.scost $(COST)
	$(COST) -akos -o $@ -I $(COST_DIR) \
                -header $*_anim.sch -prefix $*_anim_ $<

%_anim.sch: %.cost ;

%.char: $(FONT_DIR)/%.bmp $(CHAR)
	$(CHAR) -ochar $@ -ibmp $<

%.roobj: $(GAME_DIR)/%.scc $(SCC)
	$(SCC) -o $@ $(SCC_FLAGS) $<

%.soun: $(SOUND_DIR)/%.voc $(SOUN)
	$(SOUN) -o $@ -voc $<

%.000: $(OBJS)
	$(SLD) -o $* $(SLD_FLAGS) $^

%.001 %.sou: %.000 ;

## Target to build a DOTT compatible version of v6 games
ifeq ($(TARGET_VERSION),6)
tentacle.000: SLD_FLAGS += -key 0x69

monster.sou: tentacle.sou
	cp $< $@

tentacle: tentacle.000 tentacle.001 monster.sou
endif

## Clean up
clean:
	rm -f $(GAME_NAME).dep \
              $(OUT_FILES) \
              *.char \
              *.roobj \
              *.cost \
              *.soun \
              *._anim.sch \
              tentacle.000 \
              tentacle.001 \
              tentacle.sou \
              monster.sou \

distclean: clean
	rm -f *~


### Dependency tracking

# We expand the source path to allow out of tree build
$(GAME_NAME).dep: $(SRCS:%=$(GAME_DIR)/%)
	$(SCC) -d -o $@ $(SCC_FLAGS) $^

deps: $(GAME_NAME).dep

NO_DEPS_GOALS       = clean \
                      distclean \
                      deps \
                      $(GAME_NAME).dep \

GOALS = $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)

ifneq ($(filter-out $(NO_DEPS_GOALS),$(GOALS)),)
-include $(GAME_NAME).dep
endif

### Regression testing

## Update the reference hash.
ifneq ($(MD5SUM),)
test-ref: $(OUT_FILES)
	@for f in $+ ; do printf "%s: " $$f ; $(MD5SUM) < $$f | cut -d ' ' -f 1 ; done > $(REF_FILE)
	@echo "Reference hash updated."

test: $(OUT_FILES)
	@[ -f $(REF_FILE) ] || ( echo "No reference hash available" ; false )
	@for f in $+ ; do \
           printf "%s: " $$f ; \
           $(MD5SUM) < $$f | cut -d ' ' -f 1 ; \
        done | diff -u $(REF_FILE) -
else
test-ref:
	@echo "md5sum is requiered to run the test."
	@false

test: test-ref
endif

.PHONY: clean \
        distclean \
        test-ref \
        test \
        deps \
        tentacle \

