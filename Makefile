.PHONY: all build test style-check clean

all: build

build:
	./scripts/build.sh

test:
	./scripts/test.sh

style-check:
	./scripts/style-check.sh

clean:
	rm -rf build out coverage
