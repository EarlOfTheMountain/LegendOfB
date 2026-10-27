# Build on the Raspberry Pi itself (Raspbian Buster, ARM1176 / VideoCore IV).
#   sudo apt install build-essential libasound2-dev     (libraspberrypi-dev is preinstalled on Raspbian)
#   make            build bin/game
#   make run        build and run from the project root (so ./assets is found)
#   make debug      build with -g, -O0 and GL error checking
#   make clean

CC      ?= gcc
VC      ?= /opt/vc
TARGET  := bin/game
BUILD   := build

# Raspbian's gcc already targets ARMv6 hard-float. Override if cross compiling,
# e.g. make ARCHFLAGS="-mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard"
ARCHFLAGS ?=

CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -Wall -Wextra -Wno-psabi $(ARCHFLAGS) -MMD -MP -Iinclude \
           -I$(VC)/include -I$(VC)/include/interface/vcos/pthreads \
           -I$(VC)/include/interface/vmcs_host/linux \
           -D_REENTRANT -D_LINUX -DTARGET_POSIX -DHAVE_LIBBCM_HOST \
           -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM

LDFLAGS += -L$(VC)/lib -Wl,-rpath,$(VC)/lib
LDLIBS  += -lbrcmGLESv2 -lbrcmEGL -lbcm_host -lvcos -lvchiq_arm -lasound -lpthread -lm

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-g -O0 -DGF_DEBUG"

clean:
	rm -rf $(BUILD) bin

-include $(OBJS:.o=.d)

.PHONY: all run debug clean
