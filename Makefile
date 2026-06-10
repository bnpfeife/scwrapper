#
# Compiler flags
#
CC      ?= gcc
CFLAGS  += -Wall -pedantic -MMD
LDFLAGS += -lm

BUILD ?= debug
ifdef RELEASE
    ifeq ($(RELEASE),1)
        BUILD = release
    endif
endif

ifdef STATIC
    CFLAGS += -static
endif

#
# Debug build settings
#
ifeq ($(BUILD),debug)
    CFLAGS += -g -fno-omit-frame-pointer
    ifeq ($(ASAN),1)
        CFLAGS += -fsanitize=address
    endif
endif

#
# Release build settings
#
ifeq ($(BUILD),release)
    CFLAGS += -O2 -s
endif

#
# iniparser
#
INIPARSER_DIR   := vendor/iniparser
INIPARSER_BUILD := $(INIPARSER_DIR)/build
INIPARSER_LIB   := $(INIPARSER_BUILD)/libiniparser.a

#
# Sources, Objects, Dependencies, and Static Libraries
#
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=obj/%.o)
DEPS := $(OBJS:obj/%.o=obj/%.d)
LIBS := $(INIPARSER_LIB)

#
# Targets
#
bin/scwrapper: $(OBJS) $(LIBS)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

$(INIPARSER_LIB): | submodules
	cmake -S $(INIPARSER_DIR) -B $(INIPARSER_BUILD)
	make -C $(INIPARSER_BUILD) iniparser-static

.PHONY: submodules
submodules:
	@if [ ! -f .skip-submodules ]; then \
		git submodule update --init --recursive --depth 1; \
	fi

.PHONY: clean
clean:
	rm -rf bin obj scwrapper.mister.tar.gz $(INIPARSER_BUILD)

#
# Docker Toolchains
#
.PHONY: toolchain-x86_64
toolchain-x86_64:
	docker build -f toolchains/Dockerfile.x86_64 -t $@ toolchains

.PHONY: toolchain-arm7vl
toolchain-arm7vl:
	docker build -f toolchains/Dockerfile.armv7l -t $@ toolchains

#
# Static Release Builds
#
UID := $(shell id -u)
GID := $(shell id -g)

.PHONY: scwrapper.x86_64
scwrapper.x86_64: toolchain-x86_64
	docker run -it -v "${PWD}:/project" --user "$(UID):$(GID)" --rm toolchain-x86_64 bash -c 'cd /project && make STATIC=1 RELEASE=1'

.PHONY: scwrapper.arm7vl
scwrapper.arm7vl: toolchain-arm7vl
	docker run -it -v "${PWD}:/project" --user "$(UID):$(GID)" --rm toolchain-arm7vl bash -c 'cd /project && make STATIC=1 RELEASE=1'

-include $(DEPS)
