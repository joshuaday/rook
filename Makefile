C_FLAGS = -O0 -Wall
OBJECTS = obj/main.o obj/term.o obj/game.o obj/rng.o obj/board.o obj/catalog.o obj/chronicle.o obj/builder.o obj/layer.o
LIBRARIES = -lm -lncurses

rook : Makefile obj $(OBJECTS)
	cc -o rook $(OBJECTS) $(SDL_FLAGS) $(LIBRARIES)

obj : 
	mkdir obj

test : rook
	./rook

obj/main.o : src/main.c src/mods.h
	cc -c src/main.c -o obj/main.o $(C_FLAGS)

obj/term.o : src/term.c src/mods.h
	cc -c src/term.c -o obj/term.o $(C_FLAGS)

obj/game.o : src/game.c src/mods.h
	cc -c src/game.c -o obj/game.o $(C_FLAGS)

obj/board.o : src/board.c src/mods.h
	cc -c src/board.c -o obj/board.o $(C_FLAGS)

obj/catalog.o : src/catalog.c src/mods.h
	cc -c src/catalog.c -o obj/catalog.o $(C_FLAGS)

obj/rng.o : src/rng.c src/mods.h
	cc -c src/rng.c -o obj/rng.o $(C_FLAGS)

obj/chronicle.o : src/chronicle.c src/mods.h
	cc -c src/chronicle.c -o obj/chronicle.o $(C_FLAGS)

obj/builder.o : src/builder.c src/mods.h
	cc -c src/builder.c -o obj/builder.o $(C_FLAGS)

obj/layer.o : src/layer.c src/mods.h
	cc -c src/layer.c -o obj/layer.o $(C_FLAGS)

tar : rook
	rm -f rook.tar.gz
	tar -czf rook.tar.gz rook README Makefile vim-all.sh $(wildcard src/*)

clean : 
	rm $(OBJECTS) rook

