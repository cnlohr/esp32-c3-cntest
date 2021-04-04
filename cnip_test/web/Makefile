.PHONY : all clean push
all : mfsmaker page.mfs tftppush

CC = gcc
CFLAGS = $(OPTS)
LDLIBS = -lusb-1.0


VERSSTR:=test
#previously called uniq, but something's wrong.

PAGE_SCRIPTS?=$(notdir $(wildcard page/jquery*gz) page/menuinterface.js $(wildcard page/*.js) )
PAGE_SCRIPT = $(foreach s, $(PAGE_SCRIPTS), <script language="javascript" type="text/javascript" src="$(s)"></script>)
PROJECT_NAME=esp32c3test
PAGE_HEADING=${PROJECT_NAME}
PROJECT_URL=
PAGE_INFO=Some description
PAGE_TITLE=$(PROJECT_NAME)

mfsmaker : mfsmaker.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lusb-1.0

page.mfs : mfsmaker $(wildcard page/*) component.mk
	rm -rf tmp
	cp -r page tmp
	# Inception level over 9000 for poor man's template substitution
	#
	$(info bla$ ${\n} blubb)
	$(foreach p, $(filter PAGE_%,$(.VARIABLES)) $(filter PROJECT_%,$(.VARIABLES)) VERSSTR, \
		sed -i "s/{{$(p)}}/$$(echo -n '$($(p))' | sed -e 's/[\/&"]/\\&/g')/" tmp/index.html && \
	) true
	./mfsmaker tmp page.mfs

#flash : page.mfs
#	esptool.py  write_flash -z 0x110000 page.mfs

page.fat : $(wildcard page/*)
	mkdir -p mount
	dd if=/dev/zero of=page.fat bs=983040 count=1
	mkfs.vfat page.fat
	sudo mount -o loop page.fat mount
	sudo cp -r page mount/
	sudo umount page.fat


tftppush :
	tftp esp82xx.local < pushscript.txt


flash : page.fat
	esptool.py  write_flash -z 0x110000 page.fat

clean :
	$(RM) -rf mfsmaker page.mfs page.fat tmp/*
