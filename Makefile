.PHONY: all build test style-check clean

all: build

build:
	./scripts/build.sh

test:
	./scripts/test.sh

style-check:
	./scripts/style-check.sh
	./scripts/check-configs.sh

clean:
	rm -rf build out coverage
