#
# Makefile fragment for OpenLSD kernel.
# This is NOT a complete makefile;
# you must run GNU make in the top-level directory
# where the GNUmakefile is located.
#

OBJDIRS += kernel

include kernel/Makefiles/lab?-files.mk

# Only build files if they exist.
KERNEL_SRCFILES := $(wildcard $(KERNEL_SRCFILES))

KERNEL_OBJFILES := $(patsubst %.c, $(OBJDIR)/%.o, $(KERNEL_SRCFILES))
KERNEL_OBJFILES := $(patsubst %.S, $(OBJDIR)/%.o, $(KERNEL_OBJFILES))
KERNEL_OBJFILES := $(patsubst $(OBJDIR)/lib/%, $(OBJDIR)/kernel/%, $(KERNEL_OBJFILES))

KERNEL_BINFILES := $(patsubst %, $(OBJDIR)/%, $(KERNEL_BINFILES))

-include $(KERNEL_OBJFILES:.o=.d)

# How to build kernel object files
$(OBJDIR)/kernel/%.o: kernel/%.c $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) $(CC_IO) $(CC_DEPS)

$(OBJDIR)/kernel/%.o: kernel/%.S $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + as $<
	@mkdir -p $(@D)
	$(V)$(AS) $(KERNEL_CFLAGS) $(CC_IO)

$(OBJDIR)/kernel/%.o: lib/%.c $(OBJDIR)/.vars.KERNEL_CFLAGS
	@echo + cc $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERNEL_CFLAGS) $(CC_IO) $(CC_DEPS)

# Special flags for kernel/init
$(OBJDIR)/kernel/main.o: override KERNEL_CFLAGS+=$(INIT_CFLAGS)
$(OBJDIR)/kernel/main.o: $(OBJDIR)/.vars.INIT_CFLAGS

# How to build the kernel itself
$(OBJDIR)/kernel/kernel: $(KERNEL_OBJFILES) $(KERNEL_BINFILES) kernel/kernel.ld \
	  $(OBJDIR)/.vars.KERNEL_LDFLAGS
	@echo + ld $@
	$(V)$(LD) -o $@.elf $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(KERNEL_BINFILES)
	$(V)$(OBJDUMP) -S $@.elf > $@.asm
	$(V)$(NM) -n $@.elf > $@.sym

	@# Extract the symbol and string tables
	$(V)$(OBJCOPY) --dump-section .symtab=$(OBJDIR)/kernel/ksymtab.bin \
	               --dump-section .strtab=$(OBJDIR)/kernel/kstrtab.bin \
	               $@.elf

	@# Turn the extracted binary blobs into linkable items
	$(V)$(OBJCOPY) -I binary \
	               -O elf64-x86-64 \
	               -B i386:x86-64 \
	               --rename-section .data=.ksymtab,alloc,load,readonly,data,contents \
	               --redefine-sym _binary_obj_kernel_ksymtab_bin_start=ksymtab_start \
	               --redefine-sym _binary_obj_kernel_ksymtab_bin_end=ksymtab_end \
	               $(OBJDIR)/kernel/ksymtab.bin \
	               $(OBJDIR)/kernel/ksymtab.o \

	$(V)$(OBJCOPY) -I binary \
	               -O elf64-x86-64 \
	               -B i386:x86-64 \
	               --rename-section .data=.kstrtab,alloc,load,readonly,data,contents \
	               --redefine-sym _binary_obj_kernel_kstrtab_bin_start=kstrtab_start \
	               --redefine-sym _binary_obj_kernel_kstrtab_bin_end=kstrtab_end \
	               $(OBJDIR)/kernel/kstrtab.bin \
	               $(OBJDIR)/kernel/kstrtab.o \

	@# Link back into the kernel
	$(V)$(LD) -o $@ $(KERNEL_LDFLAGS) $(KERNEL_OBJFILES) $(OBJDIR)/kernel/kstrtab.o $(OBJDIR)/kernel/ksymtab.o $(KERNEL_BINFILES)

# How to build the kernel disk image
$(OBJDIR)/kernel/kernel.img: $(OBJDIR)/kernel/kernel $(OBJDIR)/boot/boot
	@echo + mk $@
	$(V)truncate -s %512 $(OBJDIR)/boot/boot
	$(V)dd if=$(OBJDIR)/boot/boot of=$(OBJDIR)/kernel/kernel.img~ 2>/dev/null
	$(V)dd if=$(OBJDIR)/kernel/kernel >>$(OBJDIR)/kernel/kernel.img~ 2>/dev/null
	$(V)truncate -s 5M $(OBJDIR)/kernel/kernel.img~
	$(V)mv $(OBJDIR)/kernel/kernel.img~ $(OBJDIR)/kernel/kernel.img

# Build a swap image.
$(OBJDIR)/kernel/swap.img:
	@echo + mk $@
	@mkdir -p $(OBJDIR)/kernel
	$(V)echo -n "SWAP" > $(OBJDIR)/kernel/swap.img~
	$(V)truncate -s 128M $(OBJDIR)/kernel/swap.img~
	$(V)mv $(OBJDIR)/kernel/swap.img~ $(OBJDIR)/kernel/swap.img

all: $(OBJDIR)/kernel/kernel.img $(OBJDIR)/kernel/swap.img

grub: $(OBJDIR)/lsd-grub

$(OBJDIR)/lsd-grub: $(OBJDIR)/kernel/kernel
	@echo + oc $@
	$(V)$(OBJCOPY) --adjust-vma=0x10000000 $^ $@
