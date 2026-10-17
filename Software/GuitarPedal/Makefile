# --- Project-local GCC toolchain configuration --- [start]
# If .gcc_path exists, use it to set GCC_PATH for the correct toolchain version.
# This must run before the `include $(SYSTEM_FILES_DIR)/Makefile` line below,
# since libDaisy's core Makefile only honors GCC_PATH (to set CC/CXX/etc.) if
# it's already defined at the point that file is included - otherwise CC/CXX
# silently fall back to whatever arm-none-eabi-gcc is on $PATH.
GCC_PATH_FILE := $(wildcard .gcc_path)
ifneq ($(GCC_PATH_FILE),)
  GCC_PATH := $(shell cat $(GCC_PATH_FILE))
  $(info Using GCC toolchain from .gcc_path: $(GCC_PATH))
endif
# --- Project-local GCC toolchain configuration --- [end]

# Project Name
TARGET =  guitarpedal

CPP_STANDARD = -std=gnu++20

# Library Locations
DAISYSP_DIR ?= dependencies/DaisySP
LIBDAISY_DIR ?= dependencies/libDaisy
USE_DAISYSP_LGPL=1

# Sources
CMSIS_DSP_DIR = ${LIBDAISY_DIR}/Drivers/CMSIS-DSP

CMSIS_DSP_SRC = \
$(CMSIS_DSP_DIR)/Source/FilteringFunctions/arm_fir_f32.c \
$(CMSIS_DSP_DIR)/Source/FilteringFunctions/arm_fir_init_f32.c \
$(CMSIS_DSP_DIR)/Source/SupportFunctions/arm_fill_f32.c \
$(CMSIS_DSP_DIR)/Source/BasicMathFunctions/arm_scale_f32.c
C_DEFS += -DARM_MATH_LOOPUNROLL
# (the FFT sources and the table defines that go with the effects that need them come from generated/effects.mk)

CPP_SOURCES = guitar_pedal.cpp guitar_pedal_storage.cpp $(wildcard UI/*.cpp) $(wildcard Util/*.cpp) \
$(wildcard Hardware-Modules/*.cpp)
CPP_SOURCES += Effect-Modules/base_effect_module.cpp
C_SOURCES += $(CMSIS_DSP_SRC)

# Which effects are built is chosen in effects.config (edit it, or use tools/effects.py: `list`, `enable`, `disable`, `preset`, `report`). generated/effects.mk, which is
# generated from it and from effects.json, adds the sources of the chosen effects and of what they need (the CloudSeed pool, the CMSIS FFT tables, the GPL Dattorro engine)
# to CPP_SOURCES, C_SOURCES and C_DEFS; the effect list in loaded_effects.h is generated too. Only the chosen effects are compiled and linked: besides saving build time
# that matters for memory, since global buffers in an object file can stay in the firmware even if nothing uses them (see PERFORMANCE_AND_MEMORY.md).
CPP_SOURCES += Effect-Modules/effect_chain.cpp
include generated/effects.mk

# The NAM models are one file each in Effect-Modules/Nam/models/; this regenerates the list of them (Nam/models_list.inc) when a model has been added or removed
# (it rewrites the file only when its content changes). Nothing to do for other effects, but it is cheap.
$(shell sh Effect-Modules/Nam/update_models.sh)
include generated/defines.mk

# Compiler options
OPT=-Ofast
# OPT=-Og -g

# Boot options
APP_TYPE = BOOT_SRAM

# Place the large NAM A2 history buffer in AXI-SRAM instead of DTCMRAM.
# nam_a2_sections.lds is an INSERT fragment that augments libDaisy's main linker
# script (selected below via APP_TYPE). The ld manual requires the INSERT script
# to appear FIRST on the command line, and libDaisy's core Makefile does
# `LDFLAGS += ... -T$(LDSCRIPT)`, so adding ours here (before the include) places
# it ahead of the main script. See nam_a2_sections.lds and nam_a2_runtime.h.
LDFLAGS += -T$(abspath nam_a2_sections.lds)


# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Regenerate the effect build files when the selection changes (GNU make remakes an included makefile that is out of date, and then starts over)
generated/effects.mk generated/defines.mk: effects.config chains.json effects.json tools/effects.py
	python3 tools/effects.py generate

# The defines of the selection (generated/defines.mk) change how some sources compile (the size of the CloudSeed pool, the CMSIS FFT tables): without this a changed define
# left the old object file in the build, and the firmware had a pool for one reverb engine while two were selected.
$(OBJECTS): generated/defines.mk


# Override libDaisy's `program-dfu` to wait for a DFU-capable device instead
# of failing immediately if one isn't already enumerated. Without -w, you
# have to win a race against the bootloader's DFU window (reset the pedal,
# then run this fast enough); with it, run this first and reset whenever.
.PHONY: program-dfu
program-dfu:
	@echo "Waiting for Daisy in DFU mode -- press RESET on the pedal now."
	dfu-util -w -a 0 -s $(FLASH_ADDRESS):leave -D $(BUILD_DIR)/$(TARGET_BIN) -d ,0483:$(USBPID)

# Included as system instead of regular includes to avoid warnings from the
# 3rd party dependencies
C_INCLUDES += -isystem ./dependencies/q/q/q_lib/include
C_INCLUDES += -isystem ./dependencies/q/infra/include
C_INCLUDES += -isystem ./dependencies/gcem/include

C_INCLUDES += -isystem ./dependencies/CloudSeed
LIBS += -lcloudseed
LIBDIR += -Ldependencies/CloudSeed/build

# Add CMSIS-DSP include path
C_INCLUDES += -I$(CMSIS_DSP_DIR)/Include

# Parse variants from argument to make, if none is supplied, will build for 125B
# options:
# make -j8 VARIANT=125B
# make -j8 VARIANT=1590B
# make -j8 VARIANT=1590B_SMD
# make -j8 VARIANT=TERRARIUM
# make -j8 VARIANT=FUNBOX
ifdef VARIANT
   CFLAGS += -DVARIANT_$(VARIANT)
   $(info VARIANT=$(VARIANT))
endif


