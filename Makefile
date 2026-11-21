.PHONY: psp
psp:
	mkdir -p .build/psp
	ln -sfn ../../engine .build/psp/engine
	$(MAKE) -B -C .build/psp -f $(CURDIR)/runtime/psp/psp.mk NATIVE_ROOT="$(CURDIR)" STREET_ROD_ENGINE=1 STREET_ROD_EDITION=se
