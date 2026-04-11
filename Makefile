all: build/cx build/toys

build/cx: cx.c
	mkdir -p build
	gcc -Wall -o build/cx cx.c

build/toys: toys.c
	mkdir -p build
	gcc -Wall -o build/toys toys.c

clean:
	rm -rf build
