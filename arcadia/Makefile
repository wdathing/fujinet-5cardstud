all:
	./build.sh

demo:
	DEMO=1 ./build.sh

run: all
	./run.sh

rundemo: demo
	./run.sh

MAME_DIR ?= $(HOME)/Workspace/mame
FUJINET_TCP ?= 127.0.0.1:9995

smoke: all
	FUJINET_TCP=$(FUJINET_TCP) FUJINET_DEBUG=1 $(MAME_DIR)/mame arcadia \
	  -cartslot fujinet -cart $(PWD)/build/5card.bin \
	  -autoboot_script emu/smoke.lua -video none -sound none \
	  -seconds_to_run 70 -snapshot_directory build \
	  -cfg_directory cfg -nvram_directory build/nvram

resettest: all
	FUJINET_TCP=$(FUJINET_TCP) FUJINET_DEBUG=1 $(MAME_DIR)/mame arcadia \
	  -cartslot fujinet -cart $(PWD)/build/5card.bin \
	  -autoboot_script emu/resettest.lua -video none -sound none \
	  -seconds_to_run 40 -snapshot_directory build \
	  -cfg_directory cfg -nvram_directory build/nvram

assets:
	python3 tools/mkudc.py

clean:
	rm -rf build

.PHONY: all demo run rundemo smoke resettest assets clean
