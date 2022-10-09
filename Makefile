client: client.c helpers.c buffer.c requests.c parson.c
	gcc -o client client.c helpers.c buffer.c requests.c parson.c -Wall -lm

run: client
	./client

clean:
	rm -f *.o client