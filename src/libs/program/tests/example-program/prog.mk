NAME:=example-program
BIN_DIR:=$(TEST_BIN)

EXCLUDE_TEST_COVERAGE:=1

DEPENDENCIES:= \
	libs/program \

SOURCES:= \
	main.cpp \

LIBRARIES:=$(SYSTEM_LIBRARIES)
