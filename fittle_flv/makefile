
#MinGW

BIN  = flv.fap
OBJS = main.o flv.o
LIBS = -lkernel32 -luser32 -lshlwapi

ifeq ($(UNICODE),1)
BIN  = flvu.fap
DEF  = fap_unicode.def
CFLAGS   = -Wall -O2 -D_UNICODE -DUNICODE -finput-charset=CP932 -fexec-charset=CP932
CXXFLAGS = -Wall -O2 -D_UNICODE -DUNICODE -finput-charset=CP932 -fexec-charset=CP932
else
DEF  = fap.def
CFLAGS   = -Wall -O2 -finput-charset=CP932 -fexec-charset=CP932
CXXFLAGS = -Wall -O2 -finput-charset=CP932 -fexec-charset=CP932
endif
LDFLAGS  = -mwindows -s
RCFLAGS  = --language=0411

all : $(BIN)

$(BIN) : $(OBJS)
	dllwrap -k -def $(DEF) --driver-name gcc $(LDFLAGS) $(OBJS) $(LIBS) -o $@

resource.o : resource.rc
	windres $(RCFLAGS) -o resource.o resource.rc

clean :
	$(RM) $(BIN) $(OBJS)
