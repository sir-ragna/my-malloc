build:
	nasm -f elf64 -g -F DWARF brk.asm
	nasm -f elf64 -g -F DWARF write.asm
	gcc brk.o write.o allocator.c -g -Wall -Werror -o allocator

clean:
	rm *.o allocator