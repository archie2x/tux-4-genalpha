/* sdl3_compat.h — declarations for the SDL3 port shims (sdl3_compat.c).
 * Include from globals.h so every TU sees these. */

#ifndef SDL3_COMPAT_H
#define SDL3_COMPAT_H

#include <SDL3/SDL.h>

/* SDL_gfx rotozoom replacement (no-op rotation, just duplicates). */
SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom,
                             int smooth);

/* LAN/network stubs — networking play disabled in this port.
 * Provide lan_player_type / LAN_* prototypes here so call sites compile
 * even though network.h gates the real type behind HAVE_LIBSDL_NET. */
#include "transtruct.h"
#include <stdbool.h>
#ifndef LAN_PLAYER_TYPE_DEFINED
#define LAN_PLAYER_TYPE_DEFINED
typedef struct lan_player_type
{
    bool connected;
    char name[NAME_SIZE];
    bool mine;
    bool ready;
    int  score;
} lan_player_type;
#endif
const char* LAN_PlayerName(int i);
int         LAN_PlayerScore(int i);
int         LAN_PlayerMine(int i);
int         LAN_PlayerConnected(int i);
int         LAN_MyIndex(void);
int         LAN_RequestIndex(void);

/* Server stubs — multiplayer host disabled in this port. */
int  OurServerRunning(void);
int  SrvrGameInProgress(void);
void StopServer(void);

#endif /* SDL3_COMPAT_H */
