build:
	gcc bmp.c -o bmp -lm -Wall -g

run:
	./bmp

clean: 
	rm -f bmp
