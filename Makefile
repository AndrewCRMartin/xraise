BIN    = $(HOME)/bin
CC     = cc -ansi -pedantic -Wall
OFILES = xraise.o
LIBS   = -lX11

xraise : $(OFILES)
	$(CC) -o $@ $< $(LIBS)
	\rm -f xlower
	ln -fs xraise xlower

.c.o :
	$(CC) -c $<

clean :
	\rm -f $(OFILES)

install : xraise
	cp xraise $(BIN)
	(cd $(BIN); \rm -f xlower; ln -s xraise xlower)
