all: vec ls

vec: main.c monitor_neighbors.h
	gcc -pthread -o vec_router main.c monitor_neighbors.h

ls: main.cpp monitor_neighbors.h
	g++ -pthread -o ls_router main.cpp monitor_neighbors.h

.PHONY: clean
clean:
	rm *.o vec_router ls_router
