#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include <SDL/SDL.h>

#include "chrono.h"

static int cpu_mhz = 0;

static const int kNumberOfMetaballs = 40;
static const int kSizeOfMetaball = 256;

typedef struct
{
  unsigned int* pixels;
  int stride;
  int w, h;
} t_screen;

typedef struct
{
    float posx, posy;
    float init_posx, init_posy;
    float speedx, speedy;
} Metaball;


static void ChronoShow ( char* name, int computations) 
{
    float ms = ChronoWatchReset();
    float cycles = ms * (1000000.0f/1000.0f) * (float)cpu_mhz;
    float cyc_per_comp = cycles / (float)computations;
    fprintf ( stdout, "%s: %f ms, %d cycles, %f cycles/iteration\n", name, ms, (int)cycles,    cyc_per_comp);
}


// Limits framerate. Excess time is given back to the OS with "sleep"
static void LimitFramerate (int fps) 
{
    static unsigned int frame_time = 0;
    unsigned int t = GetMsTime();
    unsigned int elapsed = t - frame_time;

    const unsigned int limit = 1000 / fps;  // 1000 miliseconds / minimum framerate desired
    if ( elapsed < limit)
        usleep (( limit - elapsed) * 1000); // arg in microseconds
    frame_time = GetMsTime();
}


static void InitMetaballs(Metaball* metaball)
{
    for(int i = 0; i < kNumberOfMetaballs; ++i)
    {
        metaball[i].init_posx = 150.0f;
        metaball[i].init_posy = 150.0f;
        metaball[i].speedx = (float)(rand() % 5) * 0.01f + 0.1f;
        metaball[i].speedy = (float)(rand() % 5) * 0.01f + 0.1f;

        metaball[i].posx = metaball[i].init_posx;
        metaball[i].posy = metaball[i].init_posy;
    }
}


static void ClearEnergyCanvas(const t_screen* scr, unsigned char* canvas_energies)
{
	//unrolled for speed
    int i;
    for(i = 0; i < scr->w * scr->h; i += 4) 
	{
        canvas_energies[i] = 0;
        canvas_energies[i + 1] = 0;
        canvas_energies[i + 2] = 0;
        canvas_energies[i + 3] = 0;
    }
}


static void EnergyToColor(t_screen* scr, unsigned char* canvas_energies, unsigned int* palette)
{
	//unrolled for speed
    int i;
    for(i = 0; i < scr->w * scr->h; i += 4) 
	{
        scr->pixels[i] = palette[canvas_energies[i]];
        scr->pixels[i + 1] = palette[canvas_energies[i + 1]];
        scr->pixels[i + 2] = palette[canvas_energies[i + 2]];
        scr->pixels[i + 3] = palette[canvas_energies[i + 3]];
    }
}


static float distance(int x, int y, int radius)
{
   return (float)((x - radius) * (x - radius)) + ((y - radius) * (y - radius));
}



static void PrecalculateMetaball(unsigned char* metaball_energies)
{
    int radius = kSizeOfMetaball >> 1;

    for(int y = 0; y < kSizeOfMetaball; ++y)
    {
        for(int x = 0; x < kSizeOfMetaball; ++x)
        {
            float dist = distance(x, y, radius) * 0.009f;  

            dist = (float)radius / dist;

            if (dist < 0)
            {
                metaball_energies[x + (y * kSizeOfMetaball)] = 0;
            } 
            else if(dist > 255)
            {
                metaball_energies[x + (y * kSizeOfMetaball)] = 255;
            }
            else
            {
                metaball_energies[x + (y * kSizeOfMetaball)] = (unsigned char)dist;
            }
        }
    }
}

static void AddMetaball(unsigned char* canvas_energies, Metaball* metaball, unsigned char* metaball_energies, const t_screen* scr)
{
    int radius = kSizeOfMetaball >> 1;

    unsigned char* energy_copy = metaball_energies;
    unsigned char* top_canvas_energies_copy = canvas_energies;
    unsigned char* bot_canvas_energies_copy = canvas_energies;

    for(int i = 0; i < kNumberOfMetaballs; ++i)
    {
        int screen_position = metaball[i].posx + metaball[i].posy * scr->w;

        for(int y = 0; y <= radius; ++y)
        {
            energy_copy = &metaball_energies[y * kSizeOfMetaball];

            top_canvas_energies_copy = &canvas_energies[screen_position + (y * scr->w)];
            bot_canvas_energies_copy = &canvas_energies[(screen_position + (kSizeOfMetaball * scr->w)) - (y * scr->w)];     

            for(int x = 0; x < kSizeOfMetaball; ++x)
            {
                int color_result_top = *top_canvas_energies_copy + *energy_copy;
                int color_result_bot = *bot_canvas_energies_copy + *energy_copy;
                energy_copy++;

                *top_canvas_energies_copy = color_result_top > 255 ? 255 : color_result_top;
                *bot_canvas_energies_copy = color_result_bot > 255 ? 255 : color_result_bot;
                top_canvas_energies_copy++;
                bot_canvas_energies_copy++;
            }
        }
    }
}


static void MoveMetaball(Metaball* metaball, float delta)
{
    for(int i = 0; i < kNumberOfMetaballs; ++i)
    {
        int result_x = metaball[i].init_posx + (int)(cosf((metaball[i].speedx * delta)) * 100.0f);
        int result_y = metaball[i].init_posy + (int)(sinf((metaball[i].speedy * delta)) * 100.0f);

        if(result_x < 0.0f)
        {
            result_x = 0.0f;
        }
        else if(result_x + kSizeOfMetaball > 640)
        {
            result_x = (float)(640 - kSizeOfMetaball);
        }

        if(result_y < 0.0f)
        {
            result_y = 0.0f;
        }
        else if(result_y + kSizeOfMetaball > 480)
        {
            result_y = (float)(480 - kSizeOfMetaball);
        }

        metaball[i].posx = result_x;
        metaball[i].posy = result_y;
    }
}



int main ( int argc, char** argv) 
{
    srand(time(NULL));
    int end = 0;
    SDL_Surface  *g_SDLSrf;
    int req_w = 640;
    int req_h = 640;

    if ( argc < 2) { fprintf ( stderr, "I need the cpu speed in Mhz!\n"); exit(0);}
    cpu_mhz = atoi( argv[1]);
    assert(cpu_mhz > 0);
    fprintf ( stdout, "Cycle times for a %d Mhz cpu\n", cpu_mhz);

    // Init SDL and screen
    if (SDL_Init( SDL_INIT_VIDEO) < 0)  {
        fprintf(stderr, "Can't Initialise SDL: %s\n", SDL_GetError());
        exit(1);
    }
    if (0 == SDL_SetVideoMode( req_w, req_h, 32,  SDL_HWSURFACE | SDL_DOUBLEBUF)) {
        printf("Couldn't set %dx%dx32 video mode: %s\n", req_w, req_h, SDL_GetError());
        return 0;
    }

    g_SDLSrf = SDL_GetVideoSurface();

    SDL_Surface *pattern;
    pattern = SDL_LoadBMP("pattern.bmp");
    if(!pattern){
        printf("%s\n", SDL_GetError());
    return 1;
    }


    SDL_LockSurface(pattern);
    unsigned int* color_palette = (unsigned int*)pattern->pixels; 
    SDL_UnlockSurface(pattern);

    unsigned char* metaball_energies = (unsigned char*) malloc(kSizeOfMetaball * kSizeOfMetaball * sizeof(unsigned char));
    unsigned char* canvas_energies = (unsigned char*) malloc(req_w * req_h * sizeof(unsigned char));
    
    Metaball metaballs[kNumberOfMetaballs];
    InitMetaballs(metaballs);
    PrecalculateMetaball(metaball_energies);

    float delta = 0.0f;

    while ( !end) {
        SDL_Event event;

        // Lock screen to get access to the memory array
        SDL_LockSurface( g_SDLSrf);
        unsigned int* screen_pixels = (unsigned int *) g_SDLSrf->pixels;

        ChronoWatchReset();

        t_screen scr = { screen_pixels, g_SDLSrf->pitch >> 2, g_SDLSrf->w, g_SDLSrf->h};

        ClearEnergyCanvas(&scr, canvas_energies);

        ChronoShow ( "Clean", g_SDLSrf->w * g_SDLSrf->h);

        ChronoWatchReset();
        AddMetaball(canvas_energies, metaballs, metaball_energies, &scr);
        ChronoShow ( "Add balls", kSizeOfMetaball * kSizeOfMetaball * kNumberOfMetaballs);

        ChronoWatchReset(); 
        delta += 0.1f;
        MoveMetaball(metaballs, delta);

        ChronoWatchReset();
        EnergyToColor(&scr, canvas_energies, color_palette);  
        ChronoShow ( "Luma2color", g_SDLSrf->w * g_SDLSrf->h);

        // Liberar y volcado a la pantalla real
        ChronoWatchReset();
        SDL_UnlockSurface( g_SDLSrf);
        SDL_Flip( g_SDLSrf);
        ChronoShow ( "Screen dump", g_SDLSrf->w * g_SDLSrf->h);

        // Limitar el framerate
        // (evitar que el refresco de pantalla sea mas rapido que 60 frames/segundo)
        LimitFramerate (60);

        // Recoger eventos de la ventana
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                end = 1;
                break;
                }
            }
        }

  return 1;
}
