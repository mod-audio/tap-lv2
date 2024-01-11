
# compiler
CC ?= gcc

# flags
CFLAGS += -O3 -funroll-loops -ffast-math -fomit-frame-pointer -fstrength-reduce -Wall -Werror -fPIC -DPIC -I../utils
LDFLAGS += -shared -Wl,-O1 -Wl,--as-needed -Wl,--no-undefined -Wl,--strip-all -lm -lrt

ifneq ($(NOOPT),true)
ifneq ($(findstring $(shell uname -m),x86_64 amd64 i386 i686),)
CFLAGS += -mtune=generic -msse -msse2 -mfpmath=sse
endif
endif

# remove command
RM = rm -f

# plugin name
PLUGIN = $(shell basename $(shell pwd) | tr A-Z a-z)
PLUGIN_SO = tap_$(PLUGIN).so

# effect path
EFFECT_PATH = $(PLUGIN).lv2

# installation path
ifndef INSTALL_PATH
INSTALL_PATH = /usr/local/lib/lv2
endif
INSTALLATION_PATH = $(DESTDIR)$(INSTALL_PATH)/tap-$(EFFECT_PATH)

# sources and objects
SRC = $(wildcard *.c)

## rules
all: $(PLUGIN_SO)

$(PLUGIN_SO): $(SRC) $(wildcard *.h) ../utils/tap_utils.h
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(PLUGIN_SO)

clean:
	$(RM) *.so *.o *~

install: all
	mkdir -p $(INSTALLATION_PATH)
	cp -r *.so *.ttl modgui $(INSTALLATION_PATH)
