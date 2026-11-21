TARGET = native_shell
OBJS = main.o port_common.o port_psp.o
INCDIR = $(NATIVE_ROOT)/runtime/include
PSP_EBOOT_TITLE = DOS2PSP Native V2
MAIN_SOURCE = $(NATIVE_ROOT)/games/shell/main.c
ifeq ($(STREET_ROD_LICENSE),1)
TARGET = street_rod_license
PSP_EBOOT_TITLE = Street Rod License Preview
MAIN_SOURCE = $(NATIVE_ROOT)/games/street_rod/license_preview.c
OBJS += street_rod_picture.o street_rod_ads.o port_keyboard.o
endif
ifeq ($(STREET_ROD_ENGINE),1)
TARGET = street_rod
PSP_EBOOT_TITLE = Street Rod
MAIN_SOURCE = $(NATIVE_ROOT)/games/street_rod/engine_main.c
SR_SRC = engine/srport/src
SR_SOURCES = $(wildcard $(SR_SRC)/*.c $(SR_SRC)/*/*.c)
ifeq ($(STREET_ROD_EDITION),se)
TARGET = street_rod_se
PSP_EBOOT_TITLE = Street Rod SE
SR_CFLAGS = -DSTREET_ROD_SE
else ifeq ($(STREET_ROD_EDITION),2)
TARGET = street_rod_2
PSP_EBOOT_TITLE = Street Rod 2
SR_CFLAGS = -DSTREET_ROD_2
SR_SRC = engine/sr2port/src
INCDIR += engine/fmopl
OBJS += engine_fmopl.o
endif
SR_OBJS = $(addprefix sr_,$(notdir $(SR_SOURCES:.c=.o)))
INCDIR += $(SR_SRC) $(NATIVE_ROOT)/games/street_rod
OBJS += engine_host.o port_keyboard.o $(SR_OBJS)
endif
ifeq ($(PILOT),1)
OBJS += lifted_pilot.o pilot.o test_pilot_mips.o
INCDIR += $(NATIVE_ROOT)/out/external/pcrecomp/runtime $(NATIVE_ROOT)/out/pilot $(NATIVE_ROOT)/portability/c_semantic_bridge
CFLAGS_PILOT = -DNATIVE_PILOT
endif
CFLAGS = -std=gnu11 -O2 -G0 -Wall -Wextra -Werror -Wno-misleading-indentation -ffunction-sections -fdata-sections $(CFLAGS_PILOT)
ifneq ($(SR_CFLAGS),)
CFLAGS += $(SR_CFLAGS)
endif
ifeq ($(SELFTEST),1)
CFLAGS += -DNATIVE_SELFTEST
endif
LDFLAGS = -Wl,--gc-sections
LIBS = -lpspgu -lpspaudio -lpsppower -lm
BUILD_PRX = 1
PSP_LARGE_MEMORY = 0
EXTRA_TARGETS = EBOOT.PBP
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
main.o: $(MAIN_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@
ifeq ($(STREET_ROD_ENGINE),1)
engine_host.o: $(NATIVE_ROOT)/games/street_rod/engine_host.c
	$(CC) $(CFLAGS) -c $< -o $@
engine_fmopl.o: $(NATIVE_ROOT)/games/street_rod/engine_fmopl.c engine/fmopl/fmopl.c engine/fmopl/fmopl.h
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
define build_sr_object
sr_$(notdir $(1:.c=.o)): $(1)
	$$(CC) $$(filter-out -Werror,$$(CFLAGS)) -fno-strict-aliasing -Wno-unused-parameter -Wno-format-truncation -c $$< -o $$@
endef
$(foreach source,$(SR_SOURCES),$(eval $(call build_sr_object,$(source))))
endif
street_rod_picture.o: $(NATIVE_ROOT)/games/street_rod/picture.c $(NATIVE_ROOT)/games/street_rod/picture.h
	$(CC) $(CFLAGS) -c $< -o $@
street_rod_ads.o: $(NATIVE_ROOT)/games/street_rod/ads.c $(NATIVE_ROOT)/games/street_rod/ads.h
	$(CC) $(CFLAGS) -c $< -o $@
port_common.o: $(NATIVE_ROOT)/runtime/common/port_common.c
	$(CC) $(CFLAGS) -c $< -o $@
port_keyboard.o: $(NATIVE_ROOT)/runtime/common/port_keyboard.c
	$(CC) $(CFLAGS) -c $< -o $@
port_psp.o: $(NATIVE_ROOT)/runtime/psp/port_psp.c
	$(CC) $(CFLAGS) -c $< -o $@
lifted_pilot.o: $(NATIVE_ROOT)/out/pilot/lifted_pilot.c
	$(CC) $(CFLAGS) -c $< -o $@
pilot.o: $(NATIVE_ROOT)/portability/c_semantic_bridge/pilot.c
	$(CC) $(CFLAGS) -c $< -o $@
test_pilot_mips.o: $(NATIVE_ROOT)/tests/test_pilot_mips.c
	$(CC) $(CFLAGS) -c $< -o $@
