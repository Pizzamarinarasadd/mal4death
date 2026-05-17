CC      = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
HOSTCC  = gcc
CFLAGS  = -Wall -Wextra -std=c11
LDFLAGS = -lkernel32 -luser32 -lshell32 -lwinmm -lm
SRC     = src/main.c src/ui.c src/difficulty.c src/levels.c \
          src/validation.c src/tools.c src/timer.c src/hints.c src/audio.c
RC_OBJ  = assets/app_res.o
OUT     = mal4death.exe

KEYGEN      = tools/keygen
KEYGEN_SRC  = tools/keygen.c
SAMPLE_SRC  = tools/malus_sample_src.c
MALUS_EXE   = assets/malus_sample.exe
KEYS_H      = src/generated_keys.h

.PHONY: all clean test_validation test_difficulty test_levels FORCE

all: $(KEYS_H) $(OUT)

$(RC_OBJ): assets/app.rc assets/icon.ico
	$(WINDRES) assets/app.rc -O coff -o $(RC_OBJ)

$(OUT): $(SRC) $(KEYS_H) $(RC_OBJ)
	$(CC) $(CFLAGS) $(SRC) $(RC_OBJ) -o $(OUT) $(LDFLAGS)

# Always rebuild malus_sample.exe so placeholders are fresh before patching
$(KEYS_H): $(KEYGEN) FORCE
	$(CC) $(CFLAGS) -mwindows $(SAMPLE_SRC) -o $(MALUS_EXE) -lkernel32 -luser32
	./$(KEYGEN)

$(KEYGEN): $(KEYGEN_SRC)
	$(HOSTCC) -o $(KEYGEN) $(KEYGEN_SRC)

FORCE:

test_validation:
	$(CC) $(CFLAGS) tests/test_validation.c src/validation.c src/levels.c \
	    -o test_validation.exe $(LDFLAGS)

test_difficulty:
	$(CC) $(CFLAGS) tests/test_difficulty.c src/difficulty.c \
	    -o test_difficulty.exe $(LDFLAGS)

test_levels:
	$(CC) $(CFLAGS) tests/test_levels.c src/levels.c \
	    -o test_levels.exe $(LDFLAGS)

clean:
	rm -f *.exe assets/malus_sample.exe assets/malus_sample_hard.exe $(KEYGEN) $(KEYS_H) $(RC_OBJ)
