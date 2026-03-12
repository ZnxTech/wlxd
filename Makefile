#################
# wlxd Makefile #
#################

CC				 := clang
CC_FLAGS_COMP	 := -Wall -std=c23 -Isrc
CC_FLAGS_LINK	 := -Wall -lwayland-server -lxcb -lxcb-randr -lxcb-xinput -lxcb-xkb -lxcb-dri3 -lxcb-present
CC_FLAGS_RELEASE := -O3 -DRELEASE
CC_FLAGS_DEBUG	 := -g -DDEBUG
CC_BIN			 := wlxd
CC_BIN_DEBUG 	 := wlxd-debug

C_PROTO		:= $(shell find -path "./proto/*.xml")
C_PROTO_SRC	:= $(patsubst ./proto/%.xml, ./src/proto/%.c, $(C_PROTO))
C_PROTO_HDR	:= $(patsubst ./proto/%.xml, ./src/proto/%.h, $(C_PROTO))

C_SRC		:= $(shell find -path "./src/*.c" -not -path "./src/proto/*") $(C_PROTO_SRC)
C_HDR		:= $(shell find -path "./src/*.h" -not -path "./src/proto/*") $(C_PROTO_HDR)
C_OBJ		:= $(patsubst ./src/%.c, ./build/obj/%.o,   $(C_SRC))
C_OBJ_DEBUG	:= $(patsubst ./src/%.c, ./build/obj/%_d.o, $(C_SRC))
C_BIN		:= $(addprefix ./build/bin/, $(CC_BIN))
C_BIN_DEBUG	:= $(addprefix ./build/bin/, $(CC_BIN_DEBUG))

C_TESTS_SRC := $(shell find -path "./tests/test_*.c")
C_TESTS_BIN := $(patsubst ./tests/%.c, ./build/tests/%, $(C_TESTS_SRC))

.PHONY: all
all: debug tests

.PHONY: format
format: $(C_SRC) $(C_HDR) $(C_TESTS_SRC)
	@clang-format -i $?

.PHONY: clean
clean:
	@rm -r ./build

.PHONY: release
release: $(C_BIN) $(addprefix)
$(C_BIN): $(C_OBJ)
	@mkdir ./build/bin -p
	@$(CC) $(C_OBJ) -o $@ $(CC_FLAGS_LINK)

.PHONY: debug
debug: $(C_BIN_DEBUG)
$(C_BIN_DEBUG): $(C_OBJ_DEBUG)
	@mkdir ./build/bin -p
	@$(CC) $(C_OBJ_DEBUG) -o $@ $(CC_FLAGS_LINK)

.PHONY: tests
tests: $(C_TESTS_BIN)
$(C_TESTS_BIN): ./build/tests/% : ./tests/%.c
	@mkdir $(dir $@) -p
	@$(CC) $< -o $@ $(CC_FLAGS_COMP) $(CC_FLAGS_LINK) $(CC_FLAGS_DEBUG)

.PHONY: obj
obj: $(C_OBJ)
$(C_OBJ): ./build/obj/%.o : ./src/%.c $(C_HDR)
	@mkdir $(dir $@) -p
	@$(CC) -c $< -o $@ $(CC_FLAGS_COMP) $(CC_FLAGS_RELEASE)

.PHONY: obj_d
obj_d: $(C_OBJ_DEBUG)
$(C_OBJ_DEBUG): ./build/obj/%_d.o : ./src/%.c $(C_HDR)
	@mkdir $(dir $@) -p
	@$(CC) -c $< -o $@ $(CC_FLAGS_COMP) $(CC_FLAGS_DEBUG)

.PHONY: wlsrc
wlsrc: $(C_PROTO_SRC) $(C_PROTO_HDR)
$(C_PROTO_SRC): ./src/proto/%.c : ./proto/%.xml
	@mkdir $(dir $@) -p
	@wayland-scanner private-code $< $@

$(C_PROTO_HDR): ./src/proto/%.h : ./proto/%.xml
	@mkdir $(dir $@) -p
	@wayland-scanner server-header $< $@
