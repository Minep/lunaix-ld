define src_files
	src/main.c
	src/elf32.c
endef

obj_files := $(addsuffix .o,$(src_files))
include_opt := $(addprefix -I,$(INCLUDES))

out := $(BUILD_DIR)/bin

%.c.o: %.c
	$(call status_,CC,$<)
	@$(CC) $(CFLAGS) $(include_opt) -Iincludes -c $< -o $@

$(out)/$(BUILD_NAME): $(obj_files)
	$(call status_,LD,$(@F))
	@$(CC) -T $(LD_SCRIPT) -o $@ $< $(LIBC) $(LDFLAGS)

all: $(out)/$(BUILD_NAME)

clean:
	@rm -f $(obj_files)