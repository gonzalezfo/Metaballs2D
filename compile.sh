
clang -c -g -Wpedantic -O2 -mfpu=vfp -o obj/metaballs.o metaballs.c
clang -c -g -Wall -O2 -o obj/chrono.o chrono.c

cd obj

clang -o ../metaballs.elf metaballs.o  chrono.o  -lm -lSDL 
