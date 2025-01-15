# Project Name
TARGET =  guitarpedal

CPP_STANDARD = -std=gnu++20

# Sources

CPP_SOURCES = guitar_pedal.cpp guitar_pedal_storage.cpp $(wildcard UI/*.cpp) $(wildcard Util/*.cpp) \
$(wildcard Hardware-Modules/*.cpp)
CPP_SOURCES += Effect-Modules/base_effect_module.cpp

# Naming the module sources explicitly to make it easier to add/remove from build. Otherwise all global attributes
#  will be included in the build and take up memory resources, even when the .h file is commented out. 

CPP_SOURCES += Effect-Modules/amp_module.cpp
CPP_SOURCES += Effect-Modules/autopan_module.cpp
CPP_SOURCES += Effect-Modules/Chopper/chopper.cpp
CPP_SOURCES += Effect-Modules/chopper_module.cpp
CPP_SOURCES += Effect-Modules/chorus_module.cpp
CPP_SOURCES += Effect-Modules/cloudseed_module.cpp
CPP_SOURCES += Effect-Modules/compressor_module.cpp
CPP_SOURCES += Effect-Modules/crusher_module.cpp
CPP_SOURCES += Effect-Modules/delay_module.cpp
CPP_SOURCES += Effect-Modules/geq_module.cpp
CPP_SOURCES += Effect-Modules/ImpulseResponse/ImpulseResponse.cpp
CPP_SOURCES += Effect-Modules/ImpulseResponse/dsp.cpp
CPP_SOURCES += Effect-Modules/looper_module.cpp
CPP_SOURCES += Effect-Modules/metro_module.cpp
CPP_SOURCES += Effect-Modules/midi_keys_module.cpp
#CPP_SOURCES += Effect-Modules/modal_keys_module.cpp
CPP_SOURCES += Effect-Modules/modulated_tremolo_module.cpp
CPP_SOURCES += Effect-Modules/multi_delay_module.cpp
CPP_SOURCES += Effect-Modules/nam_module.cpp
CPP_SOURCES += Effect-Modules/noise_gate_module.cpp
CPP_SOURCES += Effect-Modules/overdrive_module.cpp
CPP_SOURCES += Effect-Modules/peq_module.cpp
#CPP_SOURCES += Effect-Modules/pitch_shifter_module.cpp
#CPP_SOURCES += Effect-Modules/pluckecho_module.cpp
CPP_SOURCES += Effect-Modules/polyoctave_module.cpp
CPP_SOURCES += Effect-Modules/reverb_module.cpp
#CPP_SOURCES += Effect-Modules/reverb_delay_module.cpp
CPP_SOURCES += Effect-Modules/scifi_module.cpp
CPP_SOURCES += Effect-Modules/scope_module.cpp
CPP_SOURCES += Effect-Modules/spectral_delay_module.cpp
#CPP_SOURCES += Effect-Modules/string_keys_module.cpp
CPP_SOURCES += Effect-Modules/tuner_module.cpp


# Compiler options
OPT=-Ofast

# Boot options
APP_TYPE = BOOT_SRAM

# Library Locations
DAISYSP_DIR ?= dependencies/DaisySP
LIBDAISY_DIR ?= dependencies/libDaisy

USE_DAISYSP_LGPL=1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

C_INCLUDES += -I./dependencies/q/q/q_lib/include
C_INCLUDES += -I./dependencies/q/infra/include 
C_INCLUDES += -I./dependencies/gcem/include
C_INCLUDES += -I./dependencies/eigen
C_INCLUDES += -I./dependencies/RTNeural

CPPFLAGS += -DRTNEURAL_DEFAULT_ALIGNMENT=8 -DRTNEURAL_NO_DEBUG=1 -DRTNEURAL_USE_EIGEN=1

C_INCLUDES += -I./dependencies/CloudSeed
LIBS += -lcloudseed
LIBDIR += -Ldependencies/CloudSeed/build

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
