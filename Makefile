ARCH = x86_64

OBJS = bootapp.o
TARGET = bootapp.efi

CFLAGS = -I/usr/include/efi -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args
LDFLAGS = -shared -Bsymbolic -L/lib -T /lib/elf_x86_64_efi.lds /lib/crt0-efi-x86_64.o
all: $(TARGET)

%.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $^ $@

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $^
