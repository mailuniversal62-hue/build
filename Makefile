CC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -s -mwindows -Wall
LIBS = -lws2_32 -lshlwapi -ladvapi32 -luser32 -lcrypt32 -lbcrypt

all: dropper.exe loader.exe steal.exe enc.exe

dropper.exe: dropper.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

loader.exe: loader.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

steal.exe: steal.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

enc.exe: enc.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.exe
