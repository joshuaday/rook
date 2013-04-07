#PROFILE = -pg
C_FLAGS = -O0 -Wall $(PROFILE)
OBJECTS = obj/main.o obj/term.o obj/game.o obj/rng.o obj/board.o obj/catalog.o obj/chronicle.o obj/builder.o obj/layer.o obj/items.o
LIBRARIES = -lm -lncurses
DEPS = src/mods.h src/game.h Makefile

rook : Makefile obj $(OBJECTS)
	cc -o rook $(OBJECTS) $(SDL_FLAGS) $(LIBRARIES) $(PROFILE)

obj : 
	mkdir obj

test : rook
	./rook

obj/main.o : src/main.c $(DEPS)
	cc -c src/main.c -o obj/main.o $(C_FLAGS)

obj/term.o : src/term.c $(DEPS)
	cc -c src/term.c -o obj/term.o $(C_FLAGS)

obj/game.o : src/game.c $(DEPS)
	cc -c src/game.c -o obj/game.o $(C_FLAGS)

obj/board.o : src/board.c $(DEPS)
	cc -c src/board.c -o obj/board.o $(C_FLAGS)

obj/catalog.o : src/catalog.c $(DEPS)
	cc -c src/catalog.c -o obj/catalog.o $(C_FLAGS)

obj/items.o : src/items.c $(DEPS)
	cc -c src/items.c -o obj/items.o $(C_FLAGS)

obj/rng.o : src/rng.c $(DEPS)
	cc -c src/rng.c -o obj/rng.o $(C_FLAGS)

obj/chronicle.o : src/chronicle.c $(DEPS)
	cc -c src/chronicle.c -o obj/chronicle.o $(C_FLAGS)

obj/builder.o : src/builder.c $(DEPS)
	cc -c src/builder.c -o obj/builder.o $(C_FLAGS)

obj/layer.o : src/layer.c $(DEPS)
	cc -c src/layer.c -o obj/layer.o $(C_FLAGS)

tar : rook
	rm -f rook-1.0.2.tar.gz
	tar --transform 's,^,rook-1.0.2/,' -czf rook.tar.gz rook README Makefile vim-all.sh $(wildcard src/*)

clean : 
	rm $(OBJECTS) rook

