# Project Name
TARGET =  guitarpedal

# Sources
CPP_SOURCES = guitar_pedal.cpp guitar_pedal_storage.cpp $(wildcard */*.cpp) Effect-Modules/Chopper/chopper.cpp

# Compiler options
OPT = -O1

# Boot options
APP_TYPE = BOOT_SRAM

# Library Locations
DAISYSP_DIR ?= DaisySP
LIBDAISY_DIR ?= libDaisy

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
