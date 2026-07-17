API ?= 35
OUTDIR ?= build

TARGET_HEADER := src/targets/pa3q-S938NKSUACZF1/target.h
TARGET_CC := $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$(API)-clang

ifeq ($(wildcard $(TARGET_CC)),)
$(error set ANDROID_NDK_HOME to an Android NDK containing $(TARGET_CC))
endif

PRELOAD := $(OUTDIR)/cve-2026-43499
ROOT_HELPER := $(OUTDIR)/cve-2026-43499-root

PRELOAD_SRCS := \
  src/main.c \
  src/util.c \
  src/slide.c \
  src/fops.c \
  src/pipe.c \
  src/root.c \
  src/preload.c

COMMON_CFLAGS := \
  -O2 -g0 -Wall -Wextra \
  -Wno-unused-parameter -Wno-sign-compare \
  -Isrc

.DEFAULT_GOAL := all

.PHONY: all clean info

all: $(PRELOAD) $(ROOT_HELPER)

$(OUTDIR):
	mkdir -p $@

$(PRELOAD): $(PRELOAD_SRCS) $(TARGET_HEADER) src/offset.h src/common.h src/kernelsnitch/*.h | $(OUTDIR)
	$(TARGET_CC) -fPIC $(COMMON_CFLAGS) $(PRELOAD_SRCS) \
	  -shared -pthread -o $@

$(ROOT_HELPER): src/su_daemon.c | $(OUTDIR)
	$(TARGET_CC) -fPIE -pie -O2 -g0 -Wall -Wextra $< -o $@

info:
	@echo "TARGET=Samsung Galaxy S25 Ultra SM-S938N S938NKSUACZF1"
	@echo "TARGET_CC=$(TARGET_CC)"
	@echo "PRELOAD=$(PRELOAD)"
	@echo "ROOT_HELPER=$(ROOT_HELPER)"

clean:
	rm -rf $(OUTDIR)
