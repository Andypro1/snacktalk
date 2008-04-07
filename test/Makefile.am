check_PROGRAMS = ptyalloc

ptyalloc_SOURCES = \
	ptyalloc.c \
	../src/getpty.c

TESTS = sanity ptyalloc

INCLUDES = -I..
LIBS = @LIBS@

DEFS = @DEFS@ @SYSTEM_DEFS@ -DYTALK_TEST
