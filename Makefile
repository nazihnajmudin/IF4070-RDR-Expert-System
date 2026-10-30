# RDR Expert System build script.
#
# Targets
#   make            build GUI and CLI for the host (Linux or WSL2)
#   make gui        build GUI only
#   make cli        build CLI only
#   make test       build and run unit tests (with ASan and UBSan)
#   make e2e        build and run end-to-end tests (CLI and scripted GUI)
#   make windows    cross-compile Windows executables with mingw-w64
#   make clean      remove build directory
#
# Variables
#   TARGET=linux|windows   output platform (default linux)
#   DEBUG=1                build with debug info and no optimisation
#   TESTHOOK=1             build GUI with scripted test hook support

TARGET     ?= linux
BUILD_ROOT ?= build
HOSTCC     ?= cc
DEBUG      ?= 0
TESTHOOK   ?= 0

OUT := $(BUILD_ROOT)/$(TARGET)
GEN := $(OUT)/gen

# ---------------------------------------------------------------------
# Platform Configuration
# ---------------------------------------------------------------------

GLFW_COMMON := context.c init.c input.c monitor.c platform.c vulkan.c window.c \
               egl_context.c osmesa_context.c null_init.c null_monitor.c \
               null_window.c null_joystick.c

ifeq ($(TARGET),windows)
  CC            := x86_64-w64-mingw32-gcc
  AR            := x86_64-w64-mingw32-ar
  EXE           := .exe
  GLFW_DEFS     := -D_GLFW_WIN32
  GLFW_PLATFORM := win32_module.c win32_time.c win32_thread.c win32_init.c \
                   win32_joystick.c win32_monitor.c win32_window.c wgl_context.c
  GUI_LDFLAGS   := -static -mwindows
  GUI_LDLIBS    := -lopengl32 -lgdi32 -luser32 -lshell32
  CLI_LDFLAGS   := -static
  CLI_LDLIBS    :=
else
  CC            ?= cc
  EXE           :=
  GLFW_DEFS     := -D_GLFW_X11
  GLFW_PLATFORM := posix_module.c posix_time.c posix_thread.c posix_poll.c \
                   x11_init.c x11_monitor.c x11_window.c xkb_unicode.c \
                   glx_context.c linux_joystick.c
  GUI_LDFLAGS   :=
  GUI_LDLIBS    := -lGL -lm -ldl -lpthread
  CLI_LDFLAGS   :=
  CLI_LDLIBS    := -lm
endif

# ---------------------------------------------------------------------
# Compiler Flags
# ---------------------------------------------------------------------

ifeq ($(DEBUG),1)
  OPT := -O0 -g
else
  OPT := -O2
endif

WARN      := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
CFLAGS    := -std=c99 $(WARN) $(OPT) -Iinclude
GUI_FLAGS := -Isrc/gui -Ithird_party/nuklear -Ithird_party/glfw/include -I$(GEN)
TP_FLAGS  := -std=gnu99 -w $(OPT) -D_DEFAULT_SOURCE

ifeq ($(TESTHOOK),1)
  GUI_FLAGS += -DRDR_TESTHOOK=1
endif

# ---------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------

CORE_SRC := $(wildcard src/core/*.c src/app/*.c src/platform/*.c)
GUI_SRC  := $(wildcard src/gui/*.c)
CLI_SRC  := $(wildcard src/cli/*.c)
GLFW_SRC := $(addprefix third_party/glfw/src/,$(GLFW_COMMON) $(GLFW_PLATFORM))
TEST_SRC := $(wildcard tests/unit/test_*.c)

EMBED_SRC := third_party/fonts/DejaVuSansMono.ttf:font_mono \
             third_party/fonts/DejaVuSansMono-Bold.ttf:font_mono_bold \
             assets/help/help_id.txt:help_text_id \
             assets/help/help_en.txt:help_text_en
EMBED_C   := $(GEN)/font_mono.c $(GEN)/font_mono_bold.c $(GEN)/help_text_id.c $(GEN)/help_text_en.c

CORE_OBJ  := $(CORE_SRC:%.c=$(OUT)/obj/%.o)
GUI_OBJ   := $(GUI_SRC:%.c=$(OUT)/obj/%.o)
CLI_OBJ   := $(CLI_SRC:%.c=$(OUT)/obj/%.o)
GLFW_OBJ  := $(GLFW_SRC:%.c=$(OUT)/obj/%.o)
EMBED_OBJ := $(EMBED_C:%.c=%.o)

CORE_LIB  := $(OUT)/librdrcore.a
GUI_BIN   := $(OUT)/rdr-expert$(EXE)
CLI_BIN   := $(OUT)/rdr-cli$(EXE)
EMBED_BIN := $(BUILD_ROOT)/host/embed

# ---------------------------------------------------------------------
# Main Targets
# ---------------------------------------------------------------------

.PHONY: all gui cli test e2e windows clean
.SECONDARY:

all: gui cli

gui: $(GUI_BIN)

cli: $(CLI_BIN)

windows:
	$(MAKE) TARGET=windows all

clean:
	rm -rf $(BUILD_ROOT)

$(GUI_BIN): $(GUI_OBJ) $(GLFW_OBJ) $(EMBED_OBJ) $(CORE_LIB)
	$(CC) $(GUI_LDFLAGS) -o $@ $^ $(GUI_LDLIBS)

$(CLI_BIN): $(CLI_OBJ) $(CORE_LIB)
	$(CC) $(CLI_LDFLAGS) -o $@ $^ $(CLI_LDLIBS)

$(CORE_LIB): $(CORE_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) rcs $@ $^

# ---------------------------------------------------------------------
# Object Rules
# ---------------------------------------------------------------------

$(OUT)/obj/src/gui/%.o: src/gui/%.c $(EMBED_C)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GUI_FLAGS) -MMD -MP -c -o $@ $<

$(OUT)/obj/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(OUT)/obj/third_party/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(TP_FLAGS) $(GLFW_DEFS) -c -o $@ $<

$(GEN)/%.o: $(GEN)/%.c
	$(CC) -std=c99 -w $(OPT) -c -o $@ $<

# ---------------------------------------------------------------------
# Embedded Resources
# ---------------------------------------------------------------------

$(EMBED_BIN): tools/embed.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=c99 -O2 -o $@ $<

define EMBED_RULE
$(GEN)/$(2).c: $(1) $(EMBED_BIN)
	@mkdir -p $(GEN)
	$(EMBED_BIN) $(1) $(2) $$@
endef
$(foreach pair,$(EMBED_SRC),$(eval $(call EMBED_RULE,$(word 1,$(subst :, ,$(pair))),$(word 2,$(subst :, ,$(pair))))))

# ---------------------------------------------------------------------
# End-to-end Tests
# ---------------------------------------------------------------------

e2e:
	$(MAKE) cli
	$(MAKE) BUILD_ROOT=$(BUILD_ROOT)/hook TESTHOOK=1 gui
	sh tests/e2e/run_e2e.sh

# ---------------------------------------------------------------------
# Unit Tests
# ---------------------------------------------------------------------

SAN       := -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_OUT  := $(BUILD_ROOT)/test
TEST_CORE := $(CORE_SRC:%.c=$(TEST_OUT)/obj/%.o)
TEST_BINS := $(TEST_SRC:tests/unit/%.c=$(TEST_OUT)/%)

test: $(TEST_BINS)
	@status=0; for t in $(TEST_BINS); do $$t || status=1; done; \
	if [ $$status -eq 0 ]; then echo "ALL UNIT TESTS PASSED"; else echo "UNIT TESTS FAILED"; fi; \
	exit $$status

$(TEST_OUT)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=c99 $(WARN) -O0 -g $(SAN) -Iinclude -MMD -MP -c -o $@ $<

$(TEST_OUT)/test_%: $(TEST_OUT)/obj/tests/unit/test_%.o $(TEST_CORE)
	$(HOSTCC) $(SAN) -o $@ $^ -lm

-include $(CORE_OBJ:.o=.d) $(GUI_OBJ:.o=.d) $(CLI_OBJ:.o=.d)
-include $(TEST_CORE:.o=.d) $(TEST_BINS:$(TEST_OUT)/%=$(TEST_OUT)/obj/tests/unit/%.d)
