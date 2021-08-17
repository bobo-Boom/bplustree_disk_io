bplustree_demo.out:bplustree_str.o bplustree_int.o bplustree_demo.o
	gcc  *.o -o bplustree_demo.out

bplustree_str.o:bplustree_str.c
	gcc -c bplustree_str.c -o bplustree_str.o

bplustree_int.o:bplustree_int.c
	gcc -c bplustree_int.c -o bplustree_int.o
	
bplustree_demo.o:bplustree_demo.c
	gcc -c bplustree_demo.c -o bplustree_demo.o
	
.PHONY:clean
clean:
	rm -rf *.o 