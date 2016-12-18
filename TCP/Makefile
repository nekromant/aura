all:server.out client.out

server.out:server.c
	gcc $< -o $@

client.out:client.c
	gcc $< -o $@

clean:
	rm *.out

.PHONY:clean all
