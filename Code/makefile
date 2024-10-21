#
# MAIN CONFIGURATION
#

EXEC = serveur client editeur
OBJECTS = functions.o
PROJECT_NAME = Projet_DRV_SNT

OBJECTS_DIR = obj
INCLUDE_DIR = includes
BIN_DIR = bin

#
# SUFFIXES (must not change it)
#

.SUFFIXES: .c .o

#
# OBJECTS (must not change it)
#

EXEC_O = $(EXEC:=.o)
OBJECTS_O = $(OBJECTS) $(EXEC_O)

#
# ARGUMENTS AND COMPILER (to configure)
#

CC = gcc
CCFLAGS_STD = -Wall -O3 -Werror
CCFLAGS_DEBUG = -D _DEBUG_
CCFLAGS = $(CCFLAGS_STD)
CCLIBS = -lncurses -lpthread

#
# RULES (must not change it)
#

all: msg $(OBJECTS) $(EXEC_O)
	@echo "Create executables..."
	@for i in $(EXEC); do \
	$(CC) -o $(BIN_DIR)/$$i $(OBJECTS_DIR)/$$i.o $(addprefix $(OBJECTS_DIR)/, $(OBJECTS)) $(CCLIBS) -I../$(INCLUDE_DIR)/; \
	done
	@echo "Done."

msg:
	@echo "Create objects..."

debug: CCFLAGS = $(CCFLAGS_STD) $(CCFLAGS_DEBUG)
debug: all

#
# DEFAULT RULES (must not change it)
#

%.o : src/%.c
	@cd $(dir $<) && ${CC} ${CCFLAGS} -c $(notdir $<) -o ../$(OBJECTS_DIR)/$(notdir $@) -I../$(INCLUDE_DIR)/

#
# MAIN RULES (must not change it)
#

# You can add your own commands
clean:
	@echo "Delete objects, temporary files..."
	@rm -f $(addprefix $(OBJECTS_DIR)/,$(OBJECTS)) $(addprefix $(OBJECTS_DIR)/,$(EXEC_O))
	@rm -f $(INCLUDE_DIR)/*~ $(INCLUDE_DIR)/*# src/*~ src/*#
	@rm -f $(addprefix $(BIN_DIR)/,$(EXEC))
	@rm -f dependancies
	@echo "Done."

depend:
	@echo "Create dependancies..."
	@sed -e "/^# DEPENDANCIES/,$$ d" makefile > dependancies
	@echo "# DEPENDANCIES" >> dependancies
	@for i in $(OBJECTS_O); do \
	$(CC) -MM -MT src/$$i $(CCFLAGS) src/`echo $$i | sed "s/\(.*\)\\.o$$/\1.c/"` -I$(INCLUDE_DIR)/ >> dependancies; \
	done
	@cat dependancies > makefile
	@rm dependancies
	@echo "Done."

#
# CREATE ARCHIVE (must not modify)
#

ARCHIVE_FILES = *

archive: clean
	@echo "Create archive $(PROJECT_NAME)_$(shell date '+%y%m%d.tar.gz')..."
	@REP=`basename "$$PWD"`; cd .. && tar zcf $(PROJECT_NAME)_$(shell date '+%y%m%d.tar.gz') $(addprefix "$$REP"/,$(ARCHIVE_FILES))
	@echo "Done."

# DEPENDANCIES
src/functions.o: src/functions.c includes/functions.h
src/serveur.o: src/serveur.c includes/serveur.h
src/client.o: src/client.c includes/functions.h includes/serveur.h
