ZIMMERSNET=http://www.zimmers.net/anonftp/pub/cbm/firmware/computers/c64

all:: roms/basic roms/chargen roms/kernal
	pio run || true
	patch -p1 < patches/littlefs.patch
	patch -p1 < patches/webauthentication.patch
	pio run

upload::
	pio run -t upload

clean::
	pio run -t clean

roms/basic:
	curl $(ZIMMERSNET)/basic.901226-01.bin -o $@

roms/chargen:
	curl $(ZIMMERSNET)/characters.901225-01.bin -o $@

roms/kernal:
	curl $(ZIMMERSNET)/kernal.901227-03.bin -o $@


