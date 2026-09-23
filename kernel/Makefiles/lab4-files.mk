# LAB 4 code
KERNEL_SRCFILES += \
	kernel/time.c \
	kernel/vma/find.c \
	kernel/vma/insert.c \
	kernel/vma/merge.c \
	kernel/vma/pfault.c \
	kernel/vma/populate.c \
	kernel/vma/protect.c \
	kernel/vma/remove.c \
	kernel/vma/show.c \
	kernel/vma/split.c \
	kernel/vma/syscall.c \
	kernel/vma/user.c \
	kernel/vma/walk.c

ifneq ($(filter VDSO,$(BONUS)),)
KERNEL_BINFILES += vdso/vdso_blob.o
endif
