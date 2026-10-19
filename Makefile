CC = gcc-11
AR = ar
CPPFLAGS += -Iinclude $(shell mysql_config --cflags)
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -fPIC
MYSQL_LIBS = $(shell mysql_config --libs)
LDLIBS += $(MYSQL_LIBS) -lcrypto
B_SRC = b_db b_session doctor visit medical_record member_b_menu
A_SRC = session password auth patient appointment
C_SRC = medicine prescription
B_OBJ = $(addprefix build/,$(addsuffix .o,$(B_SRC)))
A_OBJ = $(addprefix build/,$(addsuffix .o,$(A_SRC)))
C_OBJ = $(addprefix build/,$(addsuffix .o,$(C_SRC)))
.PHONY: all clean check test
all: build/medical_system
build:
	mkdir -p build
build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
build/libdb.so: build/db.o
	$(CC) -shared $(LDFLAGS) -Wl,-soname,libdb.so -o $@ $^ $(MYSQL_LIBS)
build/libcommon.a: build/common.o build/member_a_internal.o
	$(AR) rcs $@ $^
build/libmember_b.a: $(B_OBJ)
	$(AR) rcs $@ $^
build/medical_system: build/main.o build/menu.o $(A_OBJ) $(C_OBJ) build/libmember_b.a build/libcommon.a build/libdb.so
	$(CC) $(LDFLAGS) -o $@ build/main.o build/menu.o $(A_OBJ) $(C_OBJ) -Lbuild -lmember_b -lcommon -ldb $(LDLIBS) -Wl,-rpath,'$$ORIGIN'
build/test_member_b: test/test_member_b.c $(A_OBJ) $(C_OBJ) $(filter-out build/member_b_menu.o,$(B_OBJ)) build/libcommon.a build/libdb.so
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(A_OBJ) $(C_OBJ) $(filter-out build/member_b_menu.o,$(B_OBJ)) -Lbuild -lcommon -ldb $(LDLIBS) -Wl,-rpath,'$$ORIGIN'
check: all build/test_member_b
	@echo "Build complete; run tests against a fresh member_b_test_* database."
test: build/test_member_b
	./build/test_member_b
clean:
	rm -f build/*.o build/*.d build/*.a build/libdb.so build/medical_system build/test_member_b
-include $(wildcard build/*.d)
