SRCS := \
	driver/dachs/main.go \
	driver/mediator.go \
	prelude/error.go \
	prelude/log.go \
	prelude/position.go \
	prelude/source.go \
	prelude/ordinal.go \
	ast/node.go \
	ast/visitor.go \
	ast/printer.go \
	syntax/token.go \
	syntax/lexer.go \
	syntax/grammar.go \
	syntax/parser.go \
	syntax/block_expr.go \

TESTS := \
	prelude/log_test.go \
	prelude/source_test.go \
	prelude/error_test.go \
	prelude/ordinal_test.go \
	prelude/position_test.go \
	ast/printer_test.go \
	ast/visitor_test.go \
	syntax/token_test.go \
	syntax/lexer_test.go \
	syntax/parser_test.go \
	syntax/block_expr_test.go \

all: build test

build: dachs

dachs: $(SRCS)
	go get -t -d ./...
	if which time > /dev/null; then time go build ./driver/dachs; else go build ./driver/dachs; fi

syntax/grammar.go: syntax/grammar.go.y
	go get golang.org/x/tools/cmd/goyacc
	goyacc -o syntax/grammar.go syntax/grammar.go.y

test: $(TESTS)
	go test ./...

clean:
	rm -f dachs syntax/grammar.go

.PHONY: all build clean test
