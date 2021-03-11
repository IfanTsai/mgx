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

LDFLAGS := -lpthread -rdynamic

ifeq ($(USE_HTTP), true)
  CFLAGS += -DUSE_HTTP
endif

ifeq ($(DEBUG), true)
  VERSION = debug
  CFLAGS += -g
else
  VERSION = release
endif

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

BIN := $(addprefix $(BUILD_ROOT)/, $(BIN))

LINK_OBJ_DIR = $(BUILD_ROOT)/app/.link_obj
DEP_DIR = $(BUILD_ROOT)/app/.dep

$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
LINK_OBJ += $(OBJS)

all: $(DEPS) $(OBJS) $(BIN)

ifneq ("$(wildcard $(DEPS))", "")
  include $(DEPS)
endif

$(BIN): $(LINK_OBJ)
	@echo "------------------------------- build $(VERSION) version  ------------------------------------"
	$(CC) -o $@ $^ $(LDFLAGS)

$(LINK_OBJ_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cpp, $^)

$(DEP_DIR)/%.d: %.cpp
	echo -n $(LINK_OBJ_DIR)/ > $@
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -MM $^ >> $@
