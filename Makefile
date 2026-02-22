# daisy-ms20 Makefile
# Based on the DaisyExamples project template.
#
# Expects DaisyExamples (with libDaisy + DaisySP built) to be a sibling
# directory. Adjust DAISYEXAMPLES_DIR if your layout differs.

TARGET = daisy-ms20

# Path to DaisyExamples root (contains libDaisy/ and DaisySP/)
DAISYEXAMPLES_DIR = ../DaisyExamples

# C++ source files
CPP_SOURCES = \
	src/main.cpp \
	src/voice.cpp \
	src/fx_chain.cpp \
	src/eye_renderer.cpp

# Additional include paths
C_INCLUDES += -Isrc

# Use the standard Daisy build system
LIBDAISY_DIR = $(DAISYEXAMPLES_DIR)/libDaisy
DAISYSP_DIR = $(DAISYEXAMPLES_DIR)/DaisySP
USE_DAISYSP_LGPL = 1
CPP_STANDARD = -std=gnu++17
SYSTEM_FILES_DIR = $(DAISYEXAMPLES_DIR)/libDaisy/core
include $(SYSTEM_FILES_DIR)/Makefile

# --- Hardware test targets ---
gpio-test:
	$(MAKE) TARGET=gpio-test CPP_SOURCES=test/gpio_test.cpp

gpio-test-program-dfu:
	$(MAKE) TARGET=gpio-test CPP_SOURCES=test/gpio_test.cpp program-dfu

adc-diag:
	$(MAKE) TARGET=adc-diag CPP_SOURCES=test/adc_diag.cpp

adc-diag-program-dfu:
	$(MAKE) TARGET=adc-diag CPP_SOURCES=test/adc_diag.cpp program-dfu
