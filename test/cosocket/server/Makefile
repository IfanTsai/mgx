CROSS_COMPILE =
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
CC      = $(CROSS_COMPILE)g++
CPP     = $(CC) -E
AR      = $(CROSS_COMPILE)ar
NM      = $(CROSS_COMPILE)nm

STRIP       = $(CROSS_COMPILE)strip
OBJCOPY     = $(CROSS_COMPILE)objcopy
OBJDUMP     = $(CROSS_COMPILE)objdump

CFLAGS := -Wall -O2 -std=c++11
LDFLAGS := -rdynamic
INCLUDE_PATH := ../../../include/

BIN = cosocket_server_test

all: $(BIN)

LINK_OBJ = cosocket_server_test.o \
	../../../misc/mgx_coroutine.o \
	../../../misc/mgx_coroutine_scheduler.o \
	../../../misc/mgx_util.o \
	../../../misc/mgx_cosocket.o \
	../../../misc/mgx_io.o \
	../../../app/mgx_log.o \
	../../../app/mgx_printf.o \
	../../../app/mgx_conf.o \

$(BIN): $(LINK_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp $(INCLUDE_PATH)/*
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cpp, $^)

PHONY += clean
clean:
	rm -rf $(LINK_OBJ) $(BIN)

.PHONY: $(PHONY)

