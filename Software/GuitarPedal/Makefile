# Project Name
TARGET =  guitarpedal

# Sources
CPP_SOURCES = guitar_pedal.cpp guitar_pedal_storage.cpp $(wildcard */*.cpp) Effect-Modules/Chopper/chopper.cpp

OPT = -Os

# Library Locations
DAISYSP_DIR ?= /home/bojidar/src/daisy/DaisySP
LIBDAISY_DIR ?= /home/bojidar/src/daisy/libDaisy

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

