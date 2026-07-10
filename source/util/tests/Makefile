# Host unit tests for OrionHEN shared libs + util cheats (no PS5 SDK).
# Usage (from this directory):
#   make
#   make test
#   ORION_TEST_VERBOSE=1 make test

UTIL_ROOT := $(abspath ..)
REPO_ROOT := $(abspath $(UTIL_ROOT)/../..)
BUILD_DIR := $(UTIL_ROOT)/build/host-tests
BIN := $(BUILD_DIR)/orion-host-tests

HOST_CC ?= cc
HOST_CXX ?= c++
KEYSTONE_PREFIX ?= /opt/homebrew
KEYSTONE_CFLAGS := -I$(KEYSTONE_PREFIX)/include
KEYSTONE_LDFLAGS := -L$(KEYSTONE_PREFIX)/lib -lkeystone

COMMON_FLAGS := -Wall -Wextra -g -O0 -DORION_HOST_TEST=1 -DCBC=1 \
	-I$(UTIL_ROOT)/include \
	-I$(UTIL_ROOT)/tests \
	-I$(REPO_ROOT)/source/include \
	-I$(REPO_ROOT)/source/liborion_settings/include \
	-I$(REPO_ROOT)/source/liborion_platform/include \
	-I$(REPO_ROOT)/source/liborion_ready \
	-I$(REPO_ROOT)/source/extern/cJSON \
	-I$(REPO_ROOT)/source/shellui/include \
	$(KEYSTONE_CFLAGS)

# ready.h lives under source/include/orion/
COMMON_FLAGS += -I$(REPO_ROOT)/source/include

CFLAGS := $(COMMON_FLAGS) -std=c11
CXXFLAGS := $(COMMON_FLAGS) -std=c++20
LDFLAGS := $(KEYSTONE_LDFLAGS) -pthread

# Production sources under test
PROD_C_SRCS := \
	$(UTIL_ROOT)/source/cheats/cheat_engine_utils.c \
	$(UTIL_ROOT)/source/cheats/cheat_engine_parser_shnext.c \
	$(UTIL_ROOT)/source/cheats/third_party/aes.c \
	$(UTIL_ROOT)/source/cheats/third_party/base64.c \
	$(UTIL_ROOT)/source/cheats/third_party/miniz.c \
	$(UTIL_ROOT)/source/cheats/third_party/sha256.c \
	$(REPO_ROOT)/source/liborion_platform/source/fs.c \
	$(REPO_ROOT)/source/liborion_platform/source/log.c \
	$(REPO_ROOT)/source/liborion_platform/source/notify.c \
	$(REPO_ROOT)/source/liborion_ready/source/ready.c

PROD_CXX_SRCS := \
	$(UTIL_ROOT)/source/cheats/JsonCheatParser.cpp \
	$(UTIL_ROOT)/source/cheats/XmlCheatParser.cpp \
	$(UTIL_ROOT)/source/cheats/Mc4CheatParser.cpp \
	$(UTIL_ROOT)/source/cheats/ShnExtCheatParser.cpp \
	$(UTIL_ROOT)/source/cheats/CheatParserFactory.cpp \
	$(REPO_ROOT)/source/extern/cJSON/cJSON.cpp \
	$(REPO_ROOT)/source/liborion_settings/source/settings.cpp \
	$(REPO_ROOT)/source/shellui/src/ps5_settings_ui.cpp \
	$(REPO_ROOT)/source/shellui/src/toolbox_route.cpp

TEST_C_SRCS := \
	test_harness.c \
	test_support.c \
	test_cheat_utils.c \
	test_ready.c \
	test_platform_fs.c \
	test_platform_log.c \
	test_platform_notify.c \
	stubs/host_stubs.c

TEST_CXX_SRCS := \
	test_cheat_parsers.cpp \
	test_settings.cpp \
	test_msg_protocol.cpp \
	test_ps5_settings_ui.cpp \
	test_toolbox_route.cpp \
	test_main.cpp

PROD_C_OBJS := $(patsubst $(UTIL_ROOT)/%.c,$(BUILD_DIR)/prod/%.o,$(filter $(UTIL_ROOT)/%,$(PROD_C_SRCS)))
PROD_C_OBJS += $(BUILD_DIR)/prod/liborion_platform/fs.o
PROD_C_OBJS += $(BUILD_DIR)/prod/liborion_platform/log.o
PROD_C_OBJS += $(BUILD_DIR)/prod/liborion_platform/notify.o
PROD_C_OBJS += $(BUILD_DIR)/prod/liborion_ready/ready.o

PROD_CXX_OBJS := $(patsubst $(UTIL_ROOT)/%.cpp,$(BUILD_DIR)/prod/%.o,$(filter $(UTIL_ROOT)/%,$(PROD_CXX_SRCS)))
PROD_CXX_OBJS += $(BUILD_DIR)/prod/extern/cJSON.o
PROD_CXX_OBJS += $(BUILD_DIR)/prod/liborion_settings/settings.o
PROD_CXX_OBJS += $(BUILD_DIR)/prod/shellui/ps5_settings_ui.o
PROD_CXX_OBJS += $(BUILD_DIR)/prod/shellui/toolbox_route.o

TEST_C_OBJS := $(patsubst %.c,$(BUILD_DIR)/tests/%.o,$(TEST_C_SRCS))
TEST_CXX_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_CXX_SRCS))

ALL_OBJS := $(PROD_C_OBJS) $(PROD_CXX_OBJS) $(TEST_C_OBJS) $(TEST_CXX_OBJS)

# Soften third_party noise
THIRD_PARTY_FLAGS := -Wno-error -Wno-unused-function -Wno-sign-compare \
	-Wno-unused-parameter -Wno-missing-field-initializers

.PHONY: all test clean

all: $(BIN)

test: $(BIN)
	cd $(CURDIR) && $(BIN)

$(BIN): $(ALL_OBJS) | $(BUILD_DIR)
	$(HOST_CXX) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/prod/source/cheats/third_party/%.o: $(UTIL_ROOT)/source/cheats/third_party/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) $(THIRD_PARTY_FLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/source/cheats/cheat_engine_parser_shnext.o: $(UTIL_ROOT)/source/cheats/cheat_engine_parser_shnext.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

$(BUILD_DIR)/prod/%.o: $(UTIL_ROOT)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/%.o: $(UTIL_ROOT)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/extern/cJSON.o: $(REPO_ROOT)/source/extern/cJSON/cJSON.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) $(THIRD_PARTY_FLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/liborion_settings/settings.o: $(REPO_ROOT)/source/liborion_settings/source/settings.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/shellui/ps5_settings_ui.o: $(REPO_ROOT)/source/shellui/src/ps5_settings_ui.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/shellui/toolbox_route.o: $(REPO_ROOT)/source/shellui/src/toolbox_route.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/liborion_ready/ready.o: $(REPO_ROOT)/source/liborion_ready/source/ready.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/liborion_platform/fs.o: $(REPO_ROOT)/source/liborion_platform/source/fs.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/liborion_platform/log.o: $(REPO_ROOT)/source/liborion_platform/source/log.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/prod/liborion_platform/notify.o: $(REPO_ROOT)/source/liborion_platform/source/notify.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tests/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
