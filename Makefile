
EFFECTS_DIR = $(shell ls -d */ | sed 's/\///')

ifndef INSTALL_PATH
INSTALL_PATH = /usr/local/lib/lv2
endif

all:
	@for fx in $(EFFECTS_DIR); do \
	$(MAKE) -C $$fx; \
	done

install:
	@for fx in $(EFFECTS_DIR); do \
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C $$fx; \
	done

clean:
	@for fx in $(EFFECTS_DIR); do \
	$(MAKE) clean -C $$fx; \
	done
