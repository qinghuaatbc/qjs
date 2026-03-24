# ── config ────────────────────────────────────────────────────────────────────
CXX      := g++
CC       := gcc
QJS_VERSION := $(shell cat quickjs/VERSION)
# No -I./quickjs in CXXFLAGS: on macOS (case-insensitive FS) the stdlib's
# #include <version> would resolve to quickjs/VERSION causing parse errors.
# The runtime/*.cpp files reference quickjs headers via relative "../quickjs/" paths.
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS   := -O2 -Wall -I./quickjs -D_GNU_SOURCE -DCONFIG_VERSION=\"$(QJS_VERSION)\"
LDFLAGS      :=
INSTALL_DIR  := /usr/local/bin

UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    LDFLAGS += -lpthread -lreadline -L/opt/homebrew/opt/readline/lib
    CXXFLAGS += -I/opt/homebrew/opt/readline/include
else
    LDFLAGS += -lpthread -lm -ldl -lreadline
endif

# ── sources ───────────────────────────────────────────────────────────────────
QJS_DIR  := quickjs
QJS_SRC  := $(QJS_DIR)/quickjs.c \
             $(QJS_DIR)/quickjs-libc.c \
             $(QJS_DIR)/cutils.c \
             $(QJS_DIR)/libregexp.c \
             $(QJS_DIR)/libunicode.c \
             $(QJS_DIR)/dtoa.c

CPP_SRC  := runtime/main.cpp \
             runtime/console.cpp \
             runtime/process.cpp \
             runtime/fs.cpp \
             runtime/http.cpp \
             runtime/child_process.cpp \
             runtime/os.cpp \
             runtime/crypto.cpp \
             runtime/net.cpp

# ── objects ───────────────────────────────────────────────────────────────────
BUILD    := build
QJS_OBJ  := $(patsubst $(QJS_DIR)/%.c, $(BUILD)/qjs_%.o, $(QJS_SRC))
CPP_OBJ  := $(patsubst runtime/%.cpp, $(BUILD)/rt_%.o, $(CPP_SRC))

TARGET   := qjs

# ── rules ─────────────────────────────────────────────────────────────────────
all: $(BUILD) $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(QJS_OBJ) $(CPP_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "✓ Built $(TARGET)"

$(BUILD)/qjs_%.o: $(QJS_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/rt_%.o: runtime/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@echo "✓ Installed $(TARGET) to $(INSTALL_DIR)/$(TARGET)"

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "✓ Removed $(INSTALL_DIR)/$(TARGET)"

run: $(TARGET)
	./$(TARGET) $(FILE)

repl: $(TARGET)
	./$(TARGET)

# ── test targets ──────────────────────────────────────────────────────────────
test-hello: $(TARGET)
	./$(TARGET) examples/hello.js

test-closures: $(TARGET)
	./$(TARGET) examples/closures.js

test-async: $(TARGET)
	./$(TARGET) examples/async.js

test-oop: $(TARGET)
	./$(TARGET) examples/oop.js

test-fs: $(TARGET)
	./$(TARGET) examples/fs_test.js

test-http: $(TARGET)
	@echo "Starting HTTP server test (will run for 3s)..."
	./$(TARGET) examples/http_server.js &
	sleep 1 && curl -s http://localhost:3000/ && curl -s http://localhost:3000/api/hello
	@sleep 2; echo ""

test-all: test-hello test-closures test-async test-oop
	@echo "✓ All tests passed"

.PHONY: all clean install uninstall run repl test-hello test-closures test-async test-oop test-all
