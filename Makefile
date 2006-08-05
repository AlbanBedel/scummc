#
#  ScummC Makefile
#  Copyright (C) 2005  Alban Bedel
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

##
## If you need to add a new program or some sources to it, you just 
## need to edit Makefile.defs.
##
## If you need to add a new external lib, first edit configure to
## have it define the needed cflags and ldflags, then edit
## Makefile.defs.
##
##
## This build system has been designed to allow easy build with cross
## compilers and on compile farms where a single home directory
## is shared between many hosts. To allow this the top level config
## and the build directory include the hostname in their name.
## That way make can automatically use the right config for each host.
## The build directory contain a directory for each build target
## as defined by the compiler target system, the compiler name,
## the compiler version and the build type (debug or release).
##
## All this allow parallel build of debug and release builds with
## various compilers from various host.
##
## This makefile is mainly a front end to Makefile.target.
## Except for distclean and the log stuff it just call
## down make in the right target directory.
##
##

##
## Get the basic build config (builddir, etc)
## and check that it is for the right host
##

host=$(shell uname -n | cut -d . -f 1)

ifeq ($(wildcard config.$(host).mak),)
$(error "config.$(host).mak not found. Please run configure first.")
endif

include config.$(host).mak

ifneq ($(host),$(HOST))
$(error "config.$(host).mak was generated for $(HOST). Don't rename the config.*.mak files, they are host dependent.")
endif

## Make a list of all the configured targets
TARGET_LIST=$(patsubst $(BUILDROOT)/%/config.mak,%,$(wildcard $(BUILDROOT)/*/config.mak))

## Check that the default target is valid
ifeq ($(findstring $(TARGET),$(TARGET_LIST)),)
$(error "Default target $(TARGET) is not a valid target. You can set the default target with: ./configure --set-default-target new-target")
endif

## Ensure that the default target is the first one
TARGETS=$(TARGET) $(filter-out $(TARGET),$(TARGET_LIST))

## Get the program defs
include Makefile.defs

## The basic subtargets
## We keep these separated for the help output
SUBTARGETS=                                      \
	$(shell echo $(GROUPS) | tr [A-Z] [a-z]) \
	all                                      \

CLEAN_SUBTARGETS=                                \
	cleanlog                                 \
	clean                                    \
	cleangen                                 \
	cleanbin                                 \

## All possible subtargets
ALL_SUBTARGETS=                                  \
	$(SUBTARGETS)                            \
	$(CLEAN_SUBTARGETS)                      \
	$(foreach grp,$(GROUPS),$($(grp)))       \

##
## Generate all our targets
##

## Template for a subtarget
define TARGET_SUB_template
$(1)_$(2):
	@$$(MAKE) -C "$(BUILDROOT)/$(1)" $(2)

PHONY_TARGETS+=$(1)_$(2)
endef

## For each target define a default and all subtargets
define TARGET_template
$(1):
	@$$(MAKE) -C "$(BUILDROOT)/$(1)"

$(foreach sub,$(ALL_SUBTARGETS),$(eval $(call TARGET_SUB_template,$(1),$(sub))))

PHONY_TARGETS+=$(1)
endef

$(foreach target,$(TARGETS),$(eval $(call TARGET_template,$(target))))

## Define the default subtargets, so that 'make utils' build
## the utils for the last configured target.
## Also define all_programs, all_utils, etc which build
## for all targets.
define DEF_ALL_TARGET_template
$(1): $$(TARGET)_$(1)

## Trick to go around the bug with
## too bigs dependency list in templates
all_$(1):
	@$$(MAKE) $(TARGETS:%=%_$(1))

PHONY_TARGETS+=$(1) all_$(1)

endef

$(foreach sub,$(ALL_SUBTARGETS),$(eval $(call DEF_ALL_TARGET_template,$(sub))))

##
## The targets implemented here: distclean and log
##

## We handle disclean from here because we might
## need to change the default target if we distclean it.
## If no other target is left then destroy the build
## directory and config.$(HOST).mak.

define DISTCLEAN_template
$(1)_distclean:
	@if [ -n "$(filter-out $(1),$(TARGETS))" ] ; then     \
	   echo "rm -rf $(BUILDROOT)/$(1)" ;                  \
	   rm -rf $(BUILDROOT)/$(1) ;                         \
	   if [ "$(1)" = "$(TARGET)" ] ; then                 \
	     ./configure --set-default-target                 \
	        $(firstword $(filter-out $(1),$(TARGETS))) ;  \
	   fi                                                 \
	 else                                                 \
	   echo "rm -rf $(BUILDROOT) config.$(HOST).mak" ;    \
	   rm -rf $(BUILDROOT) config.$(HOST).mak ;           \
	 fi

$(1)_log:
	@cat $(BUILDROOT)/$(1)/build.log 2> /dev/null ; true

$(1)_viewlog: $(BUILDROOT)/$(1)/build.log
	$$(if $(PAGER),,$$(error "You need a pager to view the logs."))
	@$(PAGER) $(BUILDROOT)/$(1)/build.log

PHONY_TARGETS+=$(1)_distclean $(1)_viewlog $(1)_log
endef

$(foreach target,$(TARGETS),$(eval $(call DISTCLEAN_template,$(target))))

distclean: $(TARGET)_distclean

all_distclean:
	rm -rf $(BUILDROOT) config.$(HOST).mak

PHONY_TARGETS+=distclean all_distclean

log: $(TARGET)_log

all_log:
	@cat $(BUILDROOT)/*/build.log 2> /dev/null ; true

viewlog: $(TARGET)_viewlog

all_viewlog:
	$(if $(wildcard $(BUILDROOT)/*/build.log),,$(error "No build log found."))
	$(if $(PAGER),,$(error "You need a pager to view the logs."))
	@cat $(BUILDROOT)/*/build.log | $(PAGER)

$(BUILDROOT)/%/build.log:
	$(error "No build log found for $(TARGET)")

PHONY_TARGETS+= log viewlog all_log all_viewlog

##
## Include the compile farm makefile if a username is defined
##

ifneq ($(CF_USER),)
include cf/Makefile.cf
endif

##
## Include user defined targets.
##

ifneq ($(wildcard Makefile.user),)
include Makefile.user
endif

##
## Useful for scripts and as a quick help
##

targets:
	@for target in $(TARGETS) ; do   \
	   echo "  $$target"  ;          \
	 done

subtargets:
	@for sub in $(SUBTARGETS) viewlog log ; do              \
	   echo "  $$sub"  ;                                    \
	 done
	@for list in $(foreach grp,$(GROUPS),'$($(grp))') ; do  \
	   for sub in $$list ; do                               \
	     echo "  $$sub" ;                                   \
	   done                                                 \
	 done
	@for sub in $(CLEAN_SUBTARGETS) distclean ; do          \
	   echo "  $$sub"  ;                                    \
	 done

##
## Some help
##

help:
	@echo
	@echo "Usage:"
	@echo
	@echo "  $(MAKE)                   build for $(TARGET)"
	@echo "  $(MAKE) SUBTARGET         build a subtarget for $(TARGET)"
	@echo
	@echo "  $(MAKE) TARGET            build for the given target"
	@echo "  $(MAKE) TARGET_SUBTARGET  build a subtarget for the given target"
	@echo
	@echo "  $(MAKE) all_SUBTARGET     build a subtarget for all targets"
	@echo
	@echo "  $(MAKE) help              display this"
	@echo "  $(MAKE) targets           show a list of the configured targets"
	@echo "  $(MAKE) subtargets        show a list of the existing subtargets"
	@echo
	@echo "Targets:"
	@echo
	@for target in $(TARGETS) ; do   \
	   echo "  $$target"  ;          \
	 done
	@echo
	@echo "Subtargets:"
	@echo
	@echo "  $(strip $(SUBTARGETS)) viewlog log"
	@for list in $(foreach grp,$(GROUPS),'$($(grp))') ; do \
	   echo "  $$list" ;                                   \
	 done
	@echo "  $(strip $(CLEAN_SUBTARGETS)) distclean" 
	@echo
	@echo "Examples:"
	@echo
	@echo "  make $(word 2,$(SUBTARGETS)), make all_$(firstword $($(firstword $(GROUPS)))), make $(TARGET),"
	@echo "  make $(TARGET)_log"
	@echo

##
## Nearly everything is phony
##

.PHONY: $(PHONY_TARGETS)    \
	targets             \
	subtargets          \
	help                \

##
## Be pedantic and refuse to build anything directly from this makefile
##

%:
	$(error "Target $@ doesn't exist. Run '$(MAKE) help' for some help.")

