NAME:=boc-shell
BIN_DIR:=$(BUILD_BIN)

DEPENDENCIES:= \
	libs/system \
	libs/program \
	libs/text \
	libs/ast \
	libs/system \

SOURCES:= \
	argument_visitor.cpp \
	assignment_visitor.cpp \
	ast.cpp \
	expression_visitor.cpp \
	lexer.cpp \
	main.cpp \
	parser.cpp \
	program_visitor.cpp \
	sexpr.cpp \
	sexpr_helpers.cpp \
	stack.cpp \
	word_visitor.cpp \

LIBRARIES:=$(SYSTEM_LIBRARIES)