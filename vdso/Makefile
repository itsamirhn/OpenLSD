OBJDIRS += vdso

$(OBJDIR)/vdso/vdso.o: vdso/vdso.c
	@echo + cc[vdso] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(CFLAGS) -fPIC $(CC_IO) $(CC_DEPS)

$(OBJDIR)/vdso/vdso.so: $(OBJDIR)/vdso/vdso.o
	@echo + ld[vdso] $@
	$(V)$(LD) -shared -o $@ $<

$(OBJDIR)/vdso/vdso_blob.o: $(OBJDIR)/vdso/vdso.so
	@echo + oc[vdso] $@
	$(V)$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--redefine-sym _binary_obj_vdso_vdso_so_start=vdso_blob_start \
		--redefine-sym _binary_obj_vdso_vdso_so_end=vdso_blob_end \
		$< $@
