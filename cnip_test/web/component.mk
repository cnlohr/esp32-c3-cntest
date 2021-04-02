# ...

VERSSTR:=test
PAGE_SCRIPTS?=$(wildcard web/page/*.js)
PAGE_SCRIPT = $(foreach s, $(call uniq, $(notdir $(wildcard web/page/jquery*gz) menuinterface.js $(PAGE_SCRIPTS))), <script language="javascript" type="text/javascript" src="$(s)"></script>)
PAGE_HEADING=${PROJECT_NAME}
PROJECT_URL=
PAGE_INFO=Some description

web/mfsmaker : web/mfsmaker.c
	gcc -o $@ $^

web/page.mfs : web/mfsmaker $(wildcard web/page/*) web/component.mk
	rm -rf web/tmp
	cp -r web/page web/tmp
	# Inception level over 9000 for poor man's template substitution
	#
	$(info bla$ ${\n} blubb)
	$(foreach p, $(filter PAGE_%,$(.VARIABLES)) $(filter PROJECT_%,$(.VARIABLES)) VERSSTR, \
		sed -i "s/{{$(p)}}/$$(echo -n '$($(p))' | sed -e 's/[\/&"]/\\&/g')/" web/tmp/index.html && \
	) true
	web/mfsmaker web/tmp web/page.mfs

flash_mfs : web/page.mfs
	$(ESPTOOLPY_WRITE_FLASH) -z 0x110000 web/page.mfs

clean :
	$(RM) -rf mfsmaker web/page.mfs pushtodev execute_reflash tmp/*



