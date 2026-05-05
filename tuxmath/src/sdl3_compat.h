/* sdl3_compat.h — declarations for the SDL3 port shims (sdl3_compat.c).
 * Include from globals.h so every TU sees these. */

#ifndef SDL3_COMPAT_H
#define SDL3_COMPAT_H

#include <SDL3/SDL.h>

/* SDL_gfx rotozoom replacement (no-op rotation, just duplicates). */
SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom, int smooth);

/* LAN/network stubs — networking play disabled in this port. */
#ifndef NAME_SIZE
#define NAME_SIZE 32
#endif
#ifndef MAX_CLIENTS
#define MAX_CLIENTS 16
#endif
typedef struct {
    char name[NAME_SIZE];
    int  mine;
    int  score;
    int  connected;
} lan_player_type;
const char* LAN_PlayerName(int i);
int         LAN_PlayerScore(int i);
int         LAN_PlayerMine(int i);
int         LAN_PlayerConnected(int i);
int         LAN_MyIndex(void);
int         LAN_RequestIndex(void);

#endif /* SDL3_COMPAT_H */
