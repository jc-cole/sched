CC      := gcc
CSTD    := -std=c11

WARN    := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes \
           -Wmissing-prototypes -Wno-unused-parameter

CFLAGS  := $(CSTD) -g -O0 $(WARN) -Iinclude
LDFLAGS :=
LDLIBS  :=

# ---- project layout ----
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

TARGET  := $(BIN_DIR)/sched
TEST_PARSER := $(BIN_DIR)/test_parser

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ---- default target ----
.PHONY: all
all: $(TARGET)

# Ensure output dirs exist
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Link
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile (and auto-generate dependency .d files)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Include generated deps
-include $(DEPS)

# Convenience targets
.PHONY: run
run: $(TARGET)
	./$(TARGET)

.PHONY: test
test: $(TARGET) test-parser
	./tests/run_tests.sh

.PHONY: test-parser
test-parser: $(TEST_PARSER)
	./$(TEST_PARSER)

$(TEST_PARSER): tests/test_parser.c $(OBJ_DIR)/parser.o $(OBJ_DIR)/job.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: valgrind
valgrind: $(TARGET)
	valgrind --leak-check=full --track-fds=yes --trace-children=no ./$(TARGET) --policy rr --quantum 100 workload.txt

.PHONY: clean
clean:
	$(RM) -r $(OBJ_DIR) $(BIN_DIR)

.PHONY: format
format:
	clang-format -i $(SRCS) include/*.h

.PHONY: debug
debug: CFLAGS := $(CFLAGS)
debug: all
