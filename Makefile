# Mostly boilerplate arduino/avr Makefile.
#
PORT = /dev/ttyUSB0
TARGETS = lights passthrough flasher reset
SRC_flasher = uart.c timer1.c
SRC_passthrough = uart.c timer1.c
SRC_lights = timer1.c
MCU = atmega328p
F_CPU = 16000000
FORMAT = ihex
UPLOAD_RATE = 57600

# Name of this Makefile (used for "make depend").
MAKEFILE = Makefile

# Debugging format.
# Native formats for AVR-GCC's -g are stabs [default], or dwarf-2.
# AVR (extended) COFF requires stabs, plus an avr-objcopy run.
DEBUG = stabs

OPT = s

# Place -D or -U options here
CDEFS = -DF_CPU=$(F_CPU)

# Place -I options here
CINCS = -I$(ARDUINO)

# Compiler flag to set the C Standard level.
# c89   - "ANSI" C
# gnu89 - c89 plus GCC extensions
# c99   - ISO C99 standard (not yet fully implemented)
# gnu99 - c99 plus GCC extensions
CSTANDARD = -std=gnu99
CDEBUG = -g$(DEBUG)
CWARN = -Wall -Wstrict-prototypes
CTUNING = -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
#CEXTRA = -Wa,-adhlns=$(<:.c=.lst)

CFLAGS = $(CDEBUG) $(CDEFS) $(CINCS) -O$(OPT) $(CWARN) $(CSTANDARD) $(CEXTRA)
#ASFLAGS = -Wa,-adhlns=$(<:.S=.lst),-gstabs
LDFLAGS = 

# Programming support using avrdude. Settings and variables.
AVRDUDE_PROGRAMMER = arduino
AVRDUDE_PORT = $(PORT)
AVRDUDE_FLAGS = -F -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER) \
  -b $(UPLOAD_RATE)

# Program settings
CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE = avr-size
NM = avr-nm
AVRDUDE = avrdude
REMOVE = rm -f
MV = mv -f

# Define all object files.
$(foreach t, $(TARGETS), $(eval OBJ_$(t) = $(t).o $$(SRC_$(t):.c=.o) $$(ASRC_$(t):.S=.o)))

# Define all listing files.
#LST = $(ASRC:.S=.lst) $(SRC:.c=.lst)
$(foreach t, $(TARGETS), $(eval LST_$(t) = $$(ASRC_$(t):.S=.lst) $(t).lst $$(SRC_$(t):.c=.lst)))

# Combine all necessary flags and optional flags.
# Add target processor to flags.
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS)
ALL_ASFLAGS = -mmcu=$(MCU) -I. -x assembler-with-cpp $(ASFLAGS)

# Default target.
all: build

build: elf hex

elf hex eep lss sym: %: $(TARGETS:=.%)

# Program the device.
upload: flasher.hex
upload_%: %.hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$<
program-radio-addr0:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U eeprom:w:0x73,0x6c,0x30,0x73,0x6c,0x31:m
program-radio-addr1:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U eeprom:w:0x73,0x6c,0x31,0x73,0x6c,0x30:m

# Convert ELF to COFF for use in debugging / simulating in AVR Studio or VMLAB.
COFFCONVERT=$(OBJCOPY) --debugging \
--change-section-address .data-0x800000 \
--change-section-address .bss-0x800000 \
--change-section-address .noinit-0x800000 \
--change-section-address .eeprom-0x810000

coff: $(TARGETS:=.elf)
	for t in target; do
		$(COFFCONVERT) -O coff-avr $$t.elf $$t.cof
	done

extcoff: $(TARGETS:=.elf)
	for t in target; do
		$(COFFCONVERT) -O coff-ext-avr $$t.elf $$t.cof
	done

.SUFFIXES: .elf .hex .eep .lss .sym

.elf.hex:
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

.elf.eep:
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 -O $(FORMAT) $< $@

# Create extended listing file from ELF output file.
.elf.lss:
	$(OBJDUMP) -h -S $< > $@

# Create a symbol table from ELF output file.
.elf.sym:
	$(NM) -n $< > $@

# Link: create ELF output file from object files.
.SECONDEXPANSION:
%.elf: $$(OBJ_%)
	$(CC) $(ALL_CFLAGS) $^ --output $@ $(LDFLAGS)

# Compile: create object files from C source files.
.c.o:
	$(CC) -c $(ALL_CFLAGS) $< -o $@

# Compile: create assembler files from C source files.
.c.s:
	$(CC) -S $(ALL_CFLAGS) $< -o $@

# Assemble: create object files from assembler source files.
.S.o:
	$(CC) -c $(ALL_ASFLAGS) $< -o $@

# Target: clean project.
clean:
	$(REMOVE) {$(TARGETS)}{.hex,.eep,.cof,.elf,.map,.sym,/lss} \
	$(OBJ) $(LST) $(SRC:.c=.s) $(SRC:.c=.d)

depend:
	if grep '^# DO NOT DELETE' $(MAKEFILE) >/dev/null; \
	then \
		sed -e '/^# DO NOT DELETE/,$$d' $(MAKEFILE) > \
			$(MAKEFILE).$$$$ && \
		$(MV) $(MAKEFILE).$$$$ $(MAKEFILE); \
	fi
	echo '# DO NOT DELETE THIS LINE -- make depend depends on it.' \
		>> $(MAKEFILE); \
	$(CC) -M -mmcu=$(MCU) $(CDEFS) $(CINCS) $(SRC) $(ASRC) >> $(MAKEFILE)

.PHONY:	all build elf hex eep lss sym program coff extcoff clean depend
