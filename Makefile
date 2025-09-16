# -*- mode: makefile-gmake -*-

TARGETS := rperf

X86_64_DIR := x86_64
AARCH64_DIR := aarch64
MACOS_DIR := macos

CC_X86_64 := $(shell pwd)/cc_x86_64
CC_AARCH64 := $(shell pwd)/cc_aarch64
CC_MACOS := $(shell pwd)/cc_macos


FLAGS :=
TARPATH := debug
ifdef RELEASE
	FLAGS += --release
	TARPATH := release
endif

ifdef HUGEPAGE
	FLAGS += --features hugepage
endif

.PHONY: all
all: $(addsuffix _x86_64,$(TARGETS)) $(addsuffix _aarch64,$(TARGETS))
x86_64: $(addsuffix _x86_64,$(TARGETS))
aarch64: $(addsuffix _aarch64,$(TARGETS))

.PHONY: $(TARGETS)
$(TARGETS): %: %_x86_64 %_aarch64

.PHONY: %_x86_64
%_x86_64:
	${HOME}/.cargo/bin/cross build --target x86_64-unknown-linux-gnu --bin $(patsubst %_x86_64,%,$@) $(FLAGS)
	mkdir -p $(X86_64_DIR)/$(TARPATH)
	cp -r target/x86_64-unknown-linux-gnu/$(TARPATH)/$(patsubst %_x86_64,%,$@) $(X86_64_DIR)/$(TARPATH)/

.PHONY: %_aarch64
%_aarch64:
	${HOME}/.cargo/bin/cross build --target aarch64-unknown-linux-gnu --bin $(patsubst %_aarch64,%,$@) $(FLAGS)
	mkdir -p $(AARCH64_DIR)/$(TARPATH)
	cp -r target/aarch64-unknown-linux-gnu/$(TARPATH)/$(patsubst %_aarch64,%,$@) $(AARCH64_DIR)/$(TARPATH)/

.PHONY: %_macos
%_macos:
	cargo zigbuild --target aarch64-apple-darwin $(FLAGS)
	mkdir -p $(MACOS_DIR)
	cp -r target/aarch64-apple-darwin/$(TARPATH)/* $(MACOS_DIR)/
