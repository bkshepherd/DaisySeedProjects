# Project Name
TARGET =  guitarpedal

CPP_STANDARD = -std=gnu++20

# Sources
CPP_SOURCES = guitar_pedal.cpp guitar_pedal_storage.cpp $(wildcard */*.cpp) Effect-Modules/Chopper/chopper.cpp

# Compiler options
OPT = -O1

# Boot options
APP_TYPE = BOOT_SRAM

# Library Locations
DAISYSP_DIR ?= DaisySP
LIBDAISY_DIR ?= libDaisy

USE_DAISYSP_LGPL=1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

C_INCLUDES += -I./q/q/q_lib/include -I./q/infra/include

# Parse variants from argument to make, if none is supplied, will build for 125B
# options:
# make -j8 VARIANT=125B
# make -j8 VARIANT=1590B
# make -j8 VARIANT=1590B_SMD
# make -j8 VARIANT=TERRARIUM
ifdef VARIANT
   CFLAGS += -DVARIANT_$(VARIANT)
   $(info VARIANT=$(VARIANT))
endif
