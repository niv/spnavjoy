LDLIBS=-lspnav
CXXFLAGS=-O2

PREFIX  ?= $(HOME)/.local
UNITDIR ?= $(HOME)/.config/systemd/user
SYSTEMCTL ?= systemctl --user

.PHONY: install clean

spnavjoy: spnavjoy.cpp

install: spnavjoy
	install -Dm755 spnavjoy $(PREFIX)/bin/spnavjoy
	install -Dm644 systemd/spnavjoy.service $(UNITDIR)/spnavjoy.service
	@echo ""
	@echo "Install done."
	@echo ""
	@echo "To enable the service, run:"
	@echo "  $(SYSTEMCTL) daemon-reload"
	@echo "  $(SYSTEMCTL) enable --now spnavjoy.service"
	@echo ""
	@echo "To disable and uninstall spnavjoy, run:"
	@echo "  $(SYSTEMCTL) disable --now spnavjoy.service"
	@echo "  rm $(UNITDIR)/spnavjoy.service $(PREFIX)/bin/spnavjoy"
	@echo "  $(SYSTEMCTL) daemon-reload"
	@echo ""
	@echo "To check the status of the service, run:"
	@echo "  $(SYSTEMCTL) status spnavjoy.service"

clean:
	rm -f spnavjoy
