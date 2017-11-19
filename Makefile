main: 	
	gcc -std=gnu99 -c ErrCheck.c client.c
	gcc -std=gnu99 ErrCheck.o client.o -o client
	gcc -std=gnu99 -c ErrCheck.c server.c
	gcc -std=gnu99 ErrCheck.o server.o -o server
	xterm -e ./server 5000 &
	xterm -e ./client 127.0.0.1 5000 &
clear:
	rm client server
