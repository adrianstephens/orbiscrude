PLATFORM	= ps3
BINDIR		= $(PROJDIR)\bin
ASSETSDIR	= $(PROJDIR)\assets

build: watch $(BUILDDIR)$(PLATFORM)\changes.txt

always:
	@echo Making assets...
	$(MAKE) --no-print-directory -r -j $(NPROC) -f $(ASSETSDIR)\assets.mak $(ACTION)

clean:
	del $(BUILDDIR)$(PLATFORM)\changes.txt
	$(MAKE) --no-print-directory -r -j $(NPROC) -f $(ASSETSDIR)\assets.mak clean

watch:
	@echo Making sure isowatch is running...
	$(BINDIR)isospawn /n1 /w- /j -p $(BINDIR)\isowatch $(ASSETSDIR) $(BUILDDIR)changes.txt

$(BUILDDIR)$(PLATFORM)\changes.txt: $(BUILDDIR)changes.txt
	@echo Making assets... ($(MAKE) --no-print-directory -r -j $(NPROC) -f $(ASSETSDIR)\assets.mak ASSETSDIR=$(realpath $(ASSETSDIR)) $(ACTION))
	$(MAKE) --no-print-directory -r -j $(NPROC) -f $(ASSETSDIR)\assets.mak ASSETSDIR=$(ASSETSDIR) $(ACTION)
	copy $< $@ /y >nul
