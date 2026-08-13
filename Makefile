# GBA rom header
TITLE       := POKEMON EMER
GAME_CODE   := BPEE
MAKER_CODE  := 01
REVISION    := 0
MODERN      ?= 0
KEEP_TEMPS  ?= 0
TESTING     ?= 0
E2E_TESTING ?= 0
TEST_SUITE  ?=
TEST_MIN_EWRAM_FREE ?= 8192
TEST_MIN_IWRAM_STACK ?= 1536

# `File name`.gba ('_modern' will be appended to the modern builds)
FILE_NAME := pokeemerald
BUILD_DIR := build

ifeq ($(TESTING),1)
  ifeq ($(strip $(TEST_SUITE)),)
    $(error TEST_SUITE is required when TESTING=1)
  endif
  MODERN := 1
  FILE_NAME := build/test/roms/$(TEST_SUITE)
endif

ifeq ($(E2E_TESTING),1)
  ifeq ($(TESTING),1)
    $(error TESTING and E2E_TESTING are mutually exclusive)
  endif
  MODERN := 1
  FILE_NAME := build/e2e/fixtures/tower-lobby
endif

# Builds the ROM using a modern compiler
MODERN      ?= 0
# Compares the ROM to a checksum of the original - only makes sense using when non-modern
COMPARE     ?= 0

ifeq (modern,$(MAKECMDGOALS))
  MODERN := 1
endif
ifeq (compare,$(MAKECMDGOALS))
  COMPARE := 1
endif

# Default make rule
all: rom

# Toolchain selection
TOOLCHAIN := $(DEVKITARM)
# don't use dkP's base_tools anymore
# because the redefinition of $(CC) conflicts
# with when we want to use $(CC) to preprocess files
# thus, manually create the variables for the bin
# files, or use arm-none-eabi binaries on the system
# if dkP is not installed on this system
ifneq (,$(TOOLCHAIN))
  ifneq ($(wildcard $(TOOLCHAIN)/bin),)
    export PATH := $(TOOLCHAIN)/bin:$(PATH)
  endif
endif

PREFIX := arm-none-eabi-
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
AS := $(PREFIX)as
LD := $(PREFIX)ld

EXE :=
ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

# use arm-none-eabi-cpp for macOS
# as macOS's default compiler is clang
# and clang's preprocessor will warn on \u
# when preprocessing asm files, expecting a unicode literal
# we can't unconditionally use arm-none-eabi-cpp
# as installations which install binutils-arm-none-eabi
# don't come with it
ifneq ($(MODERN),1)
  ifeq ($(shell uname -s),Darwin)
    CPP := $(PREFIX)cpp
  else
    CPP := $(CC) -E
  endif
else
  CPP := $(PREFIX)cpp
endif

ROM_NAME := $(FILE_NAME).gba
OBJ_DIR_NAME := $(BUILD_DIR)/emerald
MODERN_ROM_NAME := $(FILE_NAME)_modern.gba
MODERN_OBJ_DIR_NAME := $(BUILD_DIR)/modern
ASSETS_DIR_NAME := $(BUILD_DIR)/assets

ELF_NAME := $(ROM_NAME:.gba=.elf)
MAP_NAME := $(ROM_NAME:.gba=.map)
MODERN_ELF_NAME := $(MODERN_ROM_NAME:.gba=.elf)
MODERN_MAP_NAME := $(MODERN_ROM_NAME:.gba=.map)

ifeq ($(TESTING),1)
  MODERN_ROM_NAME := $(FILE_NAME).gba
  MODERN_ELF_NAME := $(FILE_NAME).elf
  MODERN_MAP_NAME := $(FILE_NAME).map
endif
ifeq ($(E2E_TESTING),1)
  MODERN_ROM_NAME := $(FILE_NAME).gba
  MODERN_ELF_NAME := $(FILE_NAME).elf
  MODERN_MAP_NAME := $(FILE_NAME).map
  MODERN_OBJ_DIR_NAME := build/e2e-fixture-obj
endif

# Pick our active variables
ifeq ($(MODERN),0)
  ROM := $(ROM_NAME)
  OBJ_DIR := $(OBJ_DIR_NAME)
else
  ROM := $(MODERN_ROM_NAME)
  OBJ_DIR := $(MODERN_OBJ_DIR_NAME)
endif
ELF := $(ROM:.gba=.elf)
MAP := $(ROM:.gba=.map)
SYM := $(ROM:.gba=.sym)

# Commonly used directories
C_SUBDIR = src
ASM_SUBDIR = asm
DATA_SRC_SUBDIR = src/data
DATA_ASM_SUBDIR = data
MID_SUBDIR = sound/songs/midi

C_BUILDDIR = $(OBJ_DIR)/$(C_SUBDIR)
ASM_BUILDDIR = $(OBJ_DIR)/$(ASM_SUBDIR)
DATA_ASM_BUILDDIR = $(OBJ_DIR)/$(DATA_ASM_SUBDIR)
MID_BUILDDIR = $(OBJ_DIR)/$(MID_SUBDIR)

SHELL := bash -o pipefail

# Set flags for tools
ASFLAGS := -mcpu=arm7tdmi --defsym MODERN=$(MODERN)

INCLUDE_DIRS := include
INCLUDE_CPP_ARGS := $(INCLUDE_DIRS:%=-iquote %)
INCLUDE_SCANINC_ARGS := $(INCLUDE_DIRS:%=-I %)

O_LEVEL ?= 2
CPPFLAGS := $(INCLUDE_CPP_ARGS) -Wno-trigraphs -DMODERN=$(MODERN)
ifeq ($(TESTING),1)
  CPPFLAGS += -DTESTING=1 -DTEST_GAME=1 -iquote tests/include
endif
ifeq ($(E2E_TESTING),1)
  CPPFLAGS += -DE2E_TESTING=1
endif
ifeq ($(MODERN),0)
  CPPFLAGS += -I tools/agbcc/include -I tools/agbcc -nostdinc -undef -std=gnu89
  CC1 := tools/agbcc/bin/agbcc$(EXE)
  override CFLAGS += -mthumb-interwork -Wimplicit -Wparentheses -Werror -O$(O_LEVEL) -fhex-asm -g
  LIBPATH := -L ../../tools/agbcc/lib
  LIB := $(LIBPATH) -lgcc -lc -L../../libagbsyscall -lagbsyscall
else
  # Note: The makefile must be set up to not call these if modern == 0
  MODERNCC := $(PREFIX)gcc
  PATH_MODERNCC := PATH="$(PATH)" $(MODERNCC)
  CC1 := $(shell $(PATH_MODERNCC) --print-prog-name=cc1) -quiet
  override CFLAGS += -mthumb -mthumb-interwork -O$(O_LEVEL) -mabi=apcs-gnu -mtune=arm7tdmi -march=armv4t -fno-toplevel-reorder -Wno-pointer-to-int-cast
  LIBPATH := -L "$(dir $(shell $(PATH_MODERNCC) -mthumb -print-file-name=libgcc.a))" -L "$(dir $(shell $(PATH_MODERNCC) -mthumb -print-file-name=libnosys.a))" -L "$(dir $(shell $(PATH_MODERNCC) -mthumb -print-file-name=libc.a))"
  LIB := $(LIBPATH) -lc -lnosys -lgcc -L../../libagbsyscall -lagbsyscall
endif
# Enable debug info if set
ifeq ($(DINFO),1)
  override CFLAGS += -g
endif

# Variable filled out in other make files
AUTO_GEN_TARGETS :=
include make_tools.mk
# Tool executables
GFX       := $(TOOLS_DIR)/gbagfx/gbagfx$(EXE)
WAV2AGB   := $(TOOLS_DIR)/wav2agb/wav2agb$(EXE)
MID       := $(TOOLS_DIR)/mid2agb/mid2agb$(EXE)
SCANINC   := $(TOOLS_DIR)/scaninc/scaninc$(EXE)
PREPROC   := $(TOOLS_DIR)/preproc/preproc$(EXE)
RAMSCRGEN := $(TOOLS_DIR)/ramscrgen/ramscrgen$(EXE)
FIX       := $(TOOLS_DIR)/gbafix/gbafix$(EXE)
MAPJSON   := $(TOOLS_DIR)/mapjson/mapjson$(EXE)
JSONPROC  := $(TOOLS_DIR)/jsonproc/jsonproc$(EXE)

PERL := perl
SHA1 := $(shell { command -v sha1sum || command -v shasum; } 2>/dev/null) -c

MAKEFLAGS += --no-print-directory

# Clear the default suffixes
.SUFFIXES:
# Don't delete intermediate files
.SECONDARY:
# Delete files that weren't built properly
.DELETE_ON_ERROR:

RULES_NO_SCAN += libagbsyscall clean clean-assets clean-test tidy tidymodern tidynonmodern generated clean-generated list-tests
.PHONY: all rom modern compare retroid clean-e2e check check-all list-tests test-roms e2e e2e-runner check-e2e-runner e2e-fixture-rom
.PHONY: $(RULES_NO_SCAN)

infoshell = $(foreach line, $(shell $1 | sed "s/ /__SPACE__/g"), $(info $(subst __SPACE__, ,$(line))))

# Check if we need to scan dependencies based on the chosen rule OR user preference
NODEP ?= 0
# Check if we need to pre-build tools and generate assets based on the chosen rule.
SETUP_PREREQS ?= 1
# Disable dependency scanning for rules that don't need it.
ifneq (,$(MAKECMDGOALS))
  ifeq (,$(filter-out $(RULES_NO_SCAN),$(MAKECMDGOALS)))
    NODEP := 1
    SETUP_PREREQS := 0
  endif
endif

.SHELLSTATUS ?= 0

ifeq ($(SETUP_PREREQS),1)
  # If set on: Default target or a rule requiring a scan
  # Forcibly execute `make tools` since we need them for what we are doing.
  $(foreach line, $(shell $(MAKE) -f make_tools.mk | sed "s/ /__SPACE__/g"), $(info $(subst __SPACE__, ,$(line))))
  ifneq ($(.SHELLSTATUS),0)
    $(error Errors occurred while building tools. See error messages above for more details)
  endif
  # Oh and also generate mapjson sources before we use `SCANINC`.
  $(foreach line, $(shell $(MAKE) generated | sed "s/ /__SPACE__/g"), $(info $(subst __SPACE__, ,$(line))))
  ifneq ($(.SHELLSTATUS),0)
    $(error Errors occurred while generating map-related sources. See error messages above for more details)
  endif
endif

# Collect sources
C_SRCS_IN := $(wildcard $(C_SUBDIR)/*.c $(C_SUBDIR)/*/*.c $(C_SUBDIR)/*/*/*.c)
C_SRCS := $(foreach src,$(C_SRCS_IN),$(if $(findstring .inc.c,$(src)),,$(src)))
ifneq ($(E2E_TESTING),1)
  C_SRCS := $(filter-out src/e2e_fixture.c,$(C_SRCS))
endif
C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))

C_ASM_SRCS := $(wildcard $(C_SUBDIR)/*.s $(C_SUBDIR)/*/*.s $(C_SUBDIR)/*/*/*.s)
C_ASM_OBJS := $(patsubst $(C_SUBDIR)/%.s,$(C_BUILDDIR)/%.o,$(C_ASM_SRCS))

ASM_SRCS := $(wildcard $(ASM_SUBDIR)/*.s)
ASM_OBJS := $(patsubst $(ASM_SUBDIR)/%.s,$(ASM_BUILDDIR)/%.o,$(ASM_SRCS))

DATA_ASM_SRCS := $(wildcard $(DATA_ASM_SUBDIR)/*.s)
DATA_ASM_OBJS := $(patsubst $(DATA_ASM_SUBDIR)/%.s,$(DATA_ASM_BUILDDIR)/%.o,$(DATA_ASM_SRCS))

MID_SRCS := $(wildcard $(MID_SUBDIR)/*.mid)
MID_OBJS := $(patsubst $(MID_SUBDIR)/%.mid,$(MID_BUILDDIR)/%.o,$(MID_SRCS))

OBJS     := $(C_OBJS) $(C_ASM_OBJS) $(ASM_OBJS) $(DATA_ASM_OBJS) $(MID_OBJS)
ifeq ($(TESTING),1)
  OBJ_DIR := build/test
  C_BUILDDIR := $(OBJ_DIR)/$(C_SUBDIR)
  ASM_BUILDDIR := $(OBJ_DIR)/$(ASM_SUBDIR)
  DATA_ASM_BUILDDIR := $(OBJ_DIR)/$(DATA_ASM_SUBDIR)
  MID_BUILDDIR := $(OBJ_DIR)/$(MID_SUBDIR)
  C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))
  C_ASM_OBJS := $(patsubst $(C_SUBDIR)/%.s,$(C_BUILDDIR)/%.o,$(C_ASM_SRCS))
  ASM_OBJS := $(patsubst $(ASM_SUBDIR)/%.s,$(ASM_BUILDDIR)/%.o,$(ASM_SRCS))
  DATA_ASM_OBJS := $(patsubst $(DATA_ASM_SUBDIR)/%.s,$(DATA_ASM_BUILDDIR)/%.o,$(DATA_ASM_SRCS))
  MID_OBJS := $(patsubst $(MID_SUBDIR)/%.mid,$(MID_BUILDDIR)/%.o,$(MID_SRCS))
  TEST_GAME_OBJS := $(OBJ_DIR)/tests/runner/test_exit.o $(OBJ_DIR)/tests/runner/test_report.o $(OBJ_DIR)/tests/runner/test_fixture.o $(OBJ_DIR)/tests/unit/$(TEST_SUITE).o
  OBJS := $(C_OBJS) $(C_ASM_OBJS) $(ASM_OBJS) $(DATA_ASM_OBJS) $(MID_OBJS) $(TEST_GAME_OBJS)
endif
OBJS_REL := $(patsubst $(OBJ_DIR)/%,%,$(OBJS))

SUBDIRS  := $(sort $(dir $(OBJS)))
$(shell mkdir -p $(SUBDIRS))

# Pretend rules that are actually flags defer to `make all`
modern: all
compare: all

# Other rules
rom: $(ROM)
ifeq ($(COMPARE),1)
	@$(SHA1) rom.sha1
endif

syms: $(SYM)

clean: tidy clean-tools clean-generated clean-assets
	@$(MAKE) clean -C libagbsyscall

clean-assets:
	rm -rf $(ASSETS_DIR_NAME)
	rm -f $(MID_SUBDIR)/*.s
	rm -f $(DATA_ASM_SUBDIR)/layouts/layouts.inc $(DATA_ASM_SUBDIR)/layouts/layouts_table.inc
	rm -f $(DATA_ASM_SUBDIR)/maps/connections.inc $(DATA_ASM_SUBDIR)/maps/events.inc $(DATA_ASM_SUBDIR)/maps/groups.inc $(DATA_ASM_SUBDIR)/maps/headers.inc
	find sound -iname '*.bin' -exec rm {} +
	find . \( -iname '*.1bpp' -o -iname '*.4bpp' -o -iname '*.8bpp' -o -iname '*.gbapal' -o -iname '*.lz' -o -iname '*.rl' -o -iname '*.latfont' -o -iname '*.hwjpnfont' -o -iname '*.fwjpnfont' \) -exec rm {} +
	find $(DATA_ASM_SUBDIR)/maps \( -iname 'connections.inc' -o -iname 'events.inc' -o -iname 'header.inc' \) -exec rm {} +

tidy: tidynonmodern tidymodern clean-test clean-e2e

tidynonmodern:
	rm -f $(ROM_NAME) $(ELF_NAME) $(MAP_NAME)
	rm -rf $(OBJ_DIR_NAME)

tidymodern:
	rm -f $(MODERN_ROM_NAME) $(MODERN_ELF_NAME) $(MODERN_MAP_NAME)
	rm -rf $(MODERN_OBJ_DIR_NAME)

clean-test:
	rm -rf $(BUILD_DIR)/test

clean-e2e:
	rm -rf $(BUILD_DIR)/e2e
	rm -rf $(BUILD_DIR)/e2e-fixture-obj

# Other rules
include graphics_file_rules.mk
include map_data_rules.mk
include json_data_rules.mk
include audio_rules.mk

# NOTE: Tools must have been built prior (FIXME)
# so you can't really call this rule directly
generated: $(AUTO_GEN_TARGETS)
	@: # Silence the "Nothing to be done for `generated'" message, which some people were confusing for an error.


%.s:   ;
%.png: ;
%.pal: ;
%.wav: ;

%.1bpp:   %.png  ; $(GFX) $< $@
%.4bpp:   %.png  ; $(GFX) $< $@
%.8bpp:   %.png  ; $(GFX) $< $@
%.gbapal: %.pal  ; $(GFX) $< $@
%.gbapal: %.png  ; $(GFX) $< $@
%.lz:     %      ; $(GFX) $< $@
%.rl:     %      ; $(GFX) $< $@

clean-generated:
	@rm -f $(AUTO_GEN_TARGETS)
	@echo "rm -f <AUTO_GEN_TARGETS>"

ifeq ($(MODERN),0)
$(C_BUILDDIR)/libc.o: CC1 := $(TOOLS_DIR)/agbcc/bin/old_agbcc$(EXE)
$(C_BUILDDIR)/libc.o: CFLAGS := -O2
$(C_BUILDDIR)/siirtc.o: CFLAGS := -mthumb-interwork
$(C_BUILDDIR)/agb_flash.o: CFLAGS := -O -mthumb-interwork
$(C_BUILDDIR)/agb_flash_1m.o: CFLAGS := -O -mthumb-interwork
$(C_BUILDDIR)/agb_flash_mx.o: CFLAGS := -O -mthumb-interwork
$(C_BUILDDIR)/m4a.o: CC1 := tools/agbcc/bin/old_agbcc$(EXE)
$(C_BUILDDIR)/record_mixing.o: CFLAGS += -ffreestanding
$(C_BUILDDIR)/librfu_intr.o: CC1 := $(TOOLS_DIR)/agbcc/bin/agbcc_arm$(EXE)
$(C_BUILDDIR)/librfu_intr.o: CFLAGS := -O2 -mthumb-interwork -quiet
else
$(C_BUILDDIR)/librfu_intr.o: CFLAGS := -mthumb-interwork -O2 -mabi=apcs-gnu -mtune=arm7tdmi -march=armv4t -fno-toplevel-reorder -Wno-pointer-to-int-cast
$(C_BUILDDIR)/berry_crush.o: override CFLAGS += -Wno-address-of-packed-member
endif

# Dependency rules (for the *.c & *.s sources to .o files)
# Have to be explicit or else missing files won't be reported.

# As a side effect, they're evaluated immediately instead of when the rule is invoked.
# It doesn't look like $(shell) can be deferred so there might not be a better way (Icedude_907: there is soon).

$(C_BUILDDIR)/%.o: $(C_SUBDIR)/%.c
ifneq ($(KEEP_TEMPS),1)
	@echo "$(CC1) <flags> -o $@ $<"
	@$(CPP) $(CPPFLAGS) $< | $(PREPROC) -i -g $(ASSETS_DIR_NAME) $< charmap.txt | $(CC1) $(CFLAGS) -o - - | cat - <(echo -e ".text\n\t.align\t2, 0") | $(AS) $(ASFLAGS) -o $@ -
else
	@$(CPP) $(CPPFLAGS) $< -o $(C_BUILDDIR)/$*.i
	@$(PREPROC) -g $(ASSETS_DIR_NAME) $(C_BUILDDIR)/$*.i charmap.txt | $(CC1) $(CFLAGS) -o $(C_BUILDDIR)/$*.s
	@echo -e ".text\n\t.align\t2, 0\n" >> $(C_BUILDDIR)/$*.s
	$(AS) $(ASFLAGS) -o $@ $(C_BUILDDIR)/$*.s
endif

$(C_BUILDDIR)/%.d: $(C_SUBDIR)/%.c
	$(SCANINC) -M $@ -g $(ASSETS_DIR_NAME) $(INCLUDE_SCANINC_ARGS) -I tools/agbcc/include $<

ifneq ($(NODEP),1)
-include $(addprefix $(OBJ_DIR)/,$(C_SRCS:.c=.d))
endif

$(ASM_BUILDDIR)/%.o: $(ASM_SUBDIR)/%.s
	$(AS) $(ASFLAGS) -o $@ $<

$(ASM_BUILDDIR)/%.d: $(ASM_SUBDIR)/%.s
	$(SCANINC) -M $@ -g $(ASSETS_DIR_NAME) $(INCLUDE_SCANINC_ARGS) -I "" $<

ifneq ($(NODEP),1)
-include $(addprefix $(OBJ_DIR)/,$(ASM_SRCS:.s=.d))
endif

$(C_BUILDDIR)/%.o: $(C_SUBDIR)/%.s
	$(PREPROC) $< charmap.txt | $(CPP) $(INCLUDE_SCANINC_ARGS) - | $(PREPROC) -ie $< charmap.txt | $(AS) $(ASFLAGS) -o $@

$(C_BUILDDIR)/%.d: $(C_SUBDIR)/%.s
	$(SCANINC) -M $@ -g $(ASSETS_DIR_NAME) $(INCLUDE_SCANINC_ARGS) -I "" $<

ifneq ($(NODEP),1)
-include $(addprefix $(OBJ_DIR)/,$(C_ASM_SRCS:.s=.d))
endif

$(DATA_ASM_BUILDDIR)/%.o: $(DATA_ASM_SUBDIR)/%.s
	$(PREPROC) $< charmap.txt | $(CPP) $(INCLUDE_SCANINC_ARGS) - | $(PREPROC) -ie $< charmap.txt | $(AS) $(ASFLAGS) -o $@

$(DATA_ASM_BUILDDIR)/%.d: $(DATA_ASM_SUBDIR)/%.s
	$(SCANINC) -M $@ -g $(ASSETS_DIR_NAME) $(INCLUDE_SCANINC_ARGS) -I "" $<

ifneq ($(NODEP),1)
-include $(addprefix $(OBJ_DIR)/,$(DATA_ASM_SRCS:.s=.d))
endif

$(OBJ_DIR)/sym_bss.ld: sym_bss.txt
	$(RAMSCRGEN) .bss $< ENGLISH > $@

$(OBJ_DIR)/sym_common.ld: sym_common.txt $(C_OBJS) $(wildcard common_syms/*.txt)
	$(RAMSCRGEN) COMMON $< ENGLISH -c $(C_BUILDDIR),common_syms > $@

$(OBJ_DIR)/sym_ewram.ld: sym_ewram.txt
	$(RAMSCRGEN) ewram_data $< ENGLISH > $@

# Linker script
ifeq ($(MODERN),0)
LD_SCRIPT := ld_script.ld
LD_SCRIPT_DEPS := $(OBJ_DIR)/sym_bss.ld $(OBJ_DIR)/sym_common.ld $(OBJ_DIR)/sym_ewram.ld
else
LD_SCRIPT := ld_script_modern.ld
LD_SCRIPT_DEPS :=
endif

# Final rules

libagbsyscall:
	@$(MAKE) -C libagbsyscall TOOLCHAIN=$(TOOLCHAIN) MODERN=$(MODERN)

# Elf from object files
LDFLAGS = -Map ../../$(MAP)
$(ELF): $(LD_SCRIPT) $(LD_SCRIPT_DEPS) $(OBJS) libagbsyscall tools/testing/check_memory_headroom.py
	@cd $(OBJ_DIR) && $(LD) $(LDFLAGS) -T ../../$< --print-memory-usage -o ../../$@ $(OBJS_REL) $(LIB) | cat
	@echo "cd $(OBJ_DIR) && $(LD) $(LDFLAGS) -T ../../$< --print-memory-usage -o ../../$@ <objs> <libs> | cat"
	$(FIX) $@ -t"$(TITLE)" -c$(GAME_CODE) -m$(MAKER_CODE) -r$(REVISION) --silent
ifeq ($(TESTING),1)
	python3 tools/testing/check_memory_headroom.py \
		--elf $@ \
		--objdump $(OBJDUMP) \
		--min-ewram-free $(TEST_MIN_EWRAM_FREE) \
		--min-iwram-stack $(TEST_MIN_IWRAM_STACK)
endif

# Builds the rom from the elf file
$(ROM): $(ELF)
	$(OBJCOPY) -O binary $< $@
	$(FIX) $@ -p --silent

# Build the legacy ROM, then deploy it to the connected Retroid Pocket Classic.
retroid: rom
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/deploy_retroid.ps1 -RomPath "$$(wslpath -w "$(abspath $(ROM_NAME))")"

# Symbol file (`make syms`)
$(SYM): $(ELF)
	$(OBJDUMP) -t $< | sort -u | grep -E "^0[2389]" | $(PERL) -p -e 's/^(\w{8}) (\w).{6} \S+\t(\w{8}) (\S+)$$/\1 \2 \3 \4/g' > $@

# Stage 2 test-runner spike. These small ROMs are intentionally isolated from
# the release object graph so TESTING=1 can never contaminate a normal build.
TEST_BUILD_DIR := $(BUILD_DIR)/test
TEST_ROM_DIR := $(TEST_BUILD_DIR)/roms
TEST_COMMON_OBJS := $(TEST_BUILD_DIR)/runner/start.o $(TEST_BUILD_DIR)/runner/test_report.o
CORE_TEST_NAMES := frontier-common team-lab new-game-tutorial save-load battle-shared frontier-tower frontier-factory frontier-dome frontier-arena frontier-palace frontier-pike frontier-pyramid
DIAGNOSTIC_TEST_NAMES := spike-pass spike-fail spike-hang
TEST_NAMES := $(CORE_TEST_NAMES)

MGBA_DIR ?= ../mgba
E2E_BUILD_DIR := $(BUILD_DIR)/e2e
E2E_RUNNER := $(E2E_BUILD_DIR)/mgba-e2e
HOSTCC ?= cc

$(E2E_RUNNER): tools/testing/e2e/mgba_driver.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror \
		-I$(MGBA_DIR)/include -I$(MGBA_DIR)/build/include \
		$< -L$(MGBA_DIR)/build -lmgba -o $@

e2e-runner: $(E2E_RUNNER)

e2e-fixture-rom:
	@mkdir -p build/e2e/fixtures
	$(MAKE) E2E_TESTING=1 rom

e2e: rom e2e-runner e2e-fixture-rom
	python3 tools/testing/run_e2e.py $(TESTS)

check-e2e-runner: rom e2e-runner
	E2E_INTEGRATION=1 python3 -m unittest discover -s tools/testing -p 'test_e2e_session.py' -v
TEST_ROMS := $(TEST_NAMES:%=$(TEST_ROM_DIR)/%.gba)

$(TEST_BUILD_DIR)/runner/%.o: tests/runner/%.s
	@mkdir -p $(dir $@)
	$(AS) -mcpu=arm7tdmi -o $@ $<

$(TEST_BUILD_DIR)/runner/%.o: tests/runner/%.c
	@mkdir -p $(dir $@)
	$(PREFIX)gcc -mthumb -mthumb-interwork -mcpu=arm7tdmi -ffreestanding -fno-builtin -Os -g \
		-DTESTING=1 -I tests/include -c $< -o $@

$(TEST_BUILD_DIR)/fixtures/%.o: tests/fixtures/%.c
	@mkdir -p $(dir $@)
	$(PREFIX)gcc -mthumb -mthumb-interwork -mcpu=arm7tdmi -ffreestanding -fno-builtin -Os -g \
		-DTESTING=1 -I tests/include -c $< -o $@

$(TEST_BUILD_DIR)/rom_header.o: src/rom_header.s
	@mkdir -p $(dir $@)
	$(PREPROC) $< charmap.txt | $(CPP) $(INCLUDE_SCANINC_ARGS) - | \
		$(PREPROC) -ie $< charmap.txt | $(AS) -mcpu=arm7tdmi --defsym MODERN=1 -o $@ -

$(TEST_ROM_DIR)/%.elf: $(TEST_BUILD_DIR)/rom_header.o $(TEST_COMMON_OBJS) $(TEST_BUILD_DIR)/fixtures/%.o ld_script_test.ld
	@mkdir -p $(dir $@)
	$(LD) -Map $(@:.elf=.map) -T ld_script_test.ld -o $@ $(filter %.o,$^)
	$(FIX) $@ -t"TEST RUNNER" -cBPEE -m01 -r0 --silent

$(TEST_ROM_DIR)/%.gba: $(TEST_ROM_DIR)/%.elf
	$(OBJCOPY) -O binary $< $@
	$(FIX) $@ -p --silent
	$(OBJDUMP) -t $(<) | sort -u > $(@:.gba=.sym)

test-roms:
	@mkdir -p $(TEST_ROM_DIR)
	@for suite in $(CORE_TEST_NAMES); do \
		$(MAKE) TESTING=1 TEST_SUITE=$$suite rom || exit $$?; \
	done

$(TEST_BUILD_DIR)/tests/runner/%.o: tests/runner/%.c
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) $< | $(PREPROC) -i $< charmap.txt | $(CC1) $(CFLAGS) -o - - | cat - <(echo -e ".text\n\t.align\t2, 0") | $(AS) $(ASFLAGS) -o $@ -

$(TEST_BUILD_DIR)/tests/runner/%.o: tests/runner/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(TEST_BUILD_DIR)/tests/unit/%.o: tests/unit/%.c
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) $< | $(PREPROC) -i $< charmap.txt | $(CC1) $(CFLAGS) -o - - | cat - <(echo -e ".text\n\t.align\t2, 0") | $(AS) $(ASFLAGS) -o $@ -

list-tests:
	@printf '%s\n' $(TEST_NAMES)

check: test-roms
	python3 tools/testing/run_tests.py --tests "$(or $(TESTS),team-lab)"

check-all: test-roms
	python3 tools/testing/run_tests.py --tests "$(TEST_NAMES)"

check-runner:
	@$(MAKE) $(DIAGNOSTIC_TEST_NAMES:%=$(TEST_ROM_DIR)/%.gba)
	@python3 tools/testing/run_tests.py --diagnostics --tests spike-pass
	@if python3 tools/testing/run_tests.py --diagnostics --tests spike-fail; then \
		echo "The intentional failure unexpectedly passed" >&2; exit 1; \
	fi
	@grep -q 'TEST_FAIL' $(TEST_BUILD_DIR)/artifacts/spike-fail/emulator.log
	@if TEST_TIMEOUT=1 python3 tools/testing/run_tests.py --diagnostics --tests spike-hang; then \
		echo "The intentional hang unexpectedly passed" >&2; exit 1; \
	fi
	@grep -q '"status": "timed-out"' $(TEST_BUILD_DIR)/artifacts/spike-hang/report.json
