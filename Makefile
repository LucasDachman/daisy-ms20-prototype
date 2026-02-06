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
	src/fx_chain.cpp

# Additional include paths
C_INCLUDES += -Isrc

# Use the standard Daisy build system
SYSTEM_FILES_DIR = $(DAISYEXAMPLES_DIR)/libDaisy/core
include $(SYSTEM_FILES_DIR)/Makefile
