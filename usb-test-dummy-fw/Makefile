SRCDIR=antares
GITURL=https://nekromant@github.com/nekromant/antares.git
OBJDIR=.
TMPDIR=tmp
TOPDIR=.
ANTARES_DIR=./antares

project_sources=src

-include antares/Makefile

ifeq ($(ANTARES_INSTALL_DIR),)
antares:
	git clone $(GITURL) $(SRCDIR)
else
antares:
	ln -sf $(ANTARES_INSTALL_DIR) $(SRCDIR)
endif


