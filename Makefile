include config.mk

PHONY := all
all:
	@for dir in $(BUILD_DIR); do \
		make -C $$dir; \
	done

PHONY += clean
clean:
	rm -f $(shell find -name "*.o") mgx

PHONY += distclean
distclean:
	rm -f $(shell find -name "*.o") mgx
	rm -f $(shell find -name "*.d")

.PHONY: $(PHONY)
