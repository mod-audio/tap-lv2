
ifndef INSTALL_PATH
INSTALL_PATH = /usr/local/lib/lv2
endif

all:
	$(MAKE) -C autopan
	$(MAKE) -C chorusflanger
	$(MAKE) -C deesser
	$(MAKE) -C doubler
	$(MAKE) -C dynamics
	$(MAKE) -C echo
	$(MAKE) -C eq
	$(MAKE) -C eqbw
	$(MAKE) -C limiter
	$(MAKE) -C pinknoise
	$(MAKE) -C pitch
	$(MAKE) -C reflector
	$(MAKE) -C reverb
	$(MAKE) -C rotspeak
	$(MAKE) -C sigmoid
	$(MAKE) -C tremolo
	$(MAKE) -C tubewarmth
	$(MAKE) -C vibrato

install:
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C autopan
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C chorusflanger
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C deesser
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C doubler
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C dynamics
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C echo
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C eq
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C eqbw
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C limiter
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C pinknoise
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C pitch
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C reflector
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C reverb
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C rotspeak
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C sigmoid
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C tremolo
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C tubewarmth
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C vibrato
	$(MAKE) install INSTALL_PATH=$(INSTALL_PATH) -C modguis

clean:
	$(MAKE) clean -C autopan
	$(MAKE) clean -C chorusflanger
	$(MAKE) clean -C deesser
	$(MAKE) clean -C doubler
	$(MAKE) clean -C dynamics
	$(MAKE) clean -C echo
	$(MAKE) clean -C eq
	$(MAKE) clean -C eqbw
	$(MAKE) clean -C limiter
	$(MAKE) clean -C pinknoise
	$(MAKE) clean -C pitch
	$(MAKE) clean -C reflector
	$(MAKE) clean -C reverb
	$(MAKE) clean -C rotspeak
	$(MAKE) clean -C sigmoid
	$(MAKE) clean -C tremolo
	$(MAKE) clean -C tubewarmth
	$(MAKE) clean -C vibrato
