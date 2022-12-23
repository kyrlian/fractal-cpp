// Kyrlian, 20093010 - 20091120
//
//
// left click zoom in on cursor
// right click zoom out
// middle click center on cursor
//
// f toggle fullscreen mode (will try several resolutions)
//
// j switch to julia set, getting x0/y0 from cursor
// m switch to mandelbrot set
// p change power in mandelbrot or julian equation (p in z' = z^p + z0) (cycle from 2 to 8)
//
// d switch depth (8bits/32bits)
// c shows palette, and shows color curves if in 32 bits mode
// if in 8 bits mode
//  a toggle palette cycling
//  s toggle each color channel
//  w cycle palette with from 1 to 1/10
//
// r reset
// q quit
//
#include <stdio.h>
#include <math.h>
#include <SDL/SDL.h>

#define WIN_WIDTH 640
#define WIN_HEIGHT 480
#define FS_WIDTH 1680
#define FS_HEIGHT 1050

#define ZOOMSPEED 10

#define INIXMIN -2
#define INIXMAX 1
#define INIYMIN -1
#define INIYMAX 1

#define MANDEL 0
#define JULIA 1
#define PALETTE 2
#define RGB 3

#define MAXITER32 2048
#define NBCOLORS32 2048
#define MAXITER8 256
#define NBCOLORS8 512

using namespace std;

// global variables
int g_bitsperpixel = 8; // can be 8 or 32, changed with d key
int g_nbcolors = NBCOLORS8;
int g_maxiterations = MAXITER8; // changed when in 32bits mode
int g_decal_palette = 0;
int g_switch_palette = 7;
int g_width_palette = 1;
int g_pow = 2;

// the three functions to convert number of iterations to r,g,b
// sw is 1 to 7, so %2 cycles in 1010101, /2%2 cycles in 0110011 and /4%2 in 0001111, thus all combinations of r/g/b on/off are cycled through
Uint8 setred(int i) { return (Uint8)(255 * (int)(g_switch_palette % 2) * fabs(sin((i + g_decal_palette) * M_PI / g_maxiterations))); };
Uint8 setgreen(int i) { return (Uint8)(255 * (int)((g_switch_palette / 2) % 2) * fabs(sin((i + g_decal_palette) * M_PI / g_maxiterations + M_PI / 3 / g_width_palette))); }
Uint8 setblue(int i) { return (Uint8)(255 * (int)((g_switch_palette / 4) % 2) * fabs(sin((i + g_decal_palette) * M_PI / g_maxiterations + M_PI * 2 / 3 / g_width_palette))); }

// convert iteration to colour
Uint32 setcolor32(SDL_Surface *screen, int iteration)
{
  Uint32 colour;
  Uint8 r, g, b;
  if (iteration == g_maxiterations)
  {
    r = g = b = 0;
  }
  else
  {
    r = setred(iteration % g_nbcolors);
    g = setgreen(iteration % g_nbcolors);
    b = setblue(iteration % g_nbcolors);
  }
  colour = SDL_MapRGB(screen->format, r, g, b);
  return colour;
}

void setpalette(SDL_Surface *screen)
{
  SDL_Color colors[g_nbcolors];
  for (int i = 0; i < g_nbcolors; i++)
  {
    colors[i].r = setred(i);
    colors[i].g = setgreen(i);
    colors[i].b = setblue(i);
  }
  // on ecrase la palette � partir de 1, le 0 reste le noir
  SDL_SetPalette(screen, SDL_PHYSPAL, &colors[0], 1, g_nbcolors);
  // SDL_SetColors(screen, &colors[0], 1, NBCOLORS);
  SDL_Flip(screen);
}

// draw pixel of given colour at given position
void setpixel32(SDL_Surface *screen, int x, int y, Uint32 colour)
{
  Uint32 *pixmem;
  pixmem = (Uint32 *)screen->pixels + y * screen->pitch / screen->format->BytesPerPixel + x;
  *pixmem = colour;
}

// draw pixel of given iteration at given position
void drawpixel32(SDL_Surface *screen, int x, int y, int iteration)
{
  setpixel32(screen, x, y, setcolor32(screen, iteration));
}

// draw pixel of given iteration at given position
void drawpixel8(SDL_Surface *screen, int x, int y, int iteration)
{
  Uint8 *pixmem;
  pixmem = (Uint8 *)screen->pixels + y * screen->pitch / screen->format->BytesPerPixel + x;
  *pixmem = iteration % g_nbcolors;
}

void drawpixel(SDL_Surface *screen, int x, int y, int iteration)
{
  if (g_bitsperpixel == 32)
  {
    drawpixel32(screen, x, y, iteration);
  }
  else
  {
    drawpixel8(screen, x, y, iteration);
  }
}

// tool : draw the palette
void DrawPalette(SDL_Surface *screen)
{
  int scx, scy;
  for (scy = 0; scy < screen->h; scy++)
  {
    for (scx = 0; scx < screen->w; scx++)
    {
      drawpixel(screen, scx, scy, scx);
    }
  }
}

// tool : draw r,g,b curves
void DrawRGB(SDL_Surface *screen)
{
  int scx, scy, it;
  Uint8 r, g, b;
  for (scx = 0; scx < screen->w; scx++)
  {
    for (scy = 0; scy < screen->h; scy++)
    {
      setpixel32(screen, scx, scy, 0);
    }
    it = scx * g_maxiterations / screen->w;
    r = setred(it);
    g = setgreen(it);
    b = setblue(it);
    setpixel32(screen, scx, r * screen->h / 256, SDL_MapRGB(screen->format, r, 0, 0));
    setpixel32(screen, scx, g * screen->h / 256, SDL_MapRGB(screen->format, 0, g, 0));
    setpixel32(screen, scx, b * screen->h / 256, SDL_MapRGB(screen->format, 0, 0, b));
    setpixel32(screen, scx, (r + g + b) * screen->h / 3 / 256,
               SDL_MapRGB(screen->format, r, g, b));
  }
}

// draw mandelbrot set
void DrawMandel(SDL_Surface *screen, double xmin, double xmax, double ymin, double ymax)
{
  double scalex = (xmax - xmin) / screen->w;
  double scaley = (ymax - ymin) / screen->h;
  double x0, y0, x, xtemp, y, px, py;
  int scx, scy, iteration, ipow;
  // pow should be modifiable
  for (scy = 0; scy < screen->h; scy++)
  {
    for (scx = 0; scx < screen->w; scx++)
    {
      x0 = xmin + scx * scalex;
      y0 = ymin + scy * scaley;
      x = y = iteration = 0;
      while (x * x + y * y <= (2 * 2) && iteration < g_maxiterations)
      {
        // variable pow
        px = x;
        py = y;
        for (ipow = 1; ipow < g_pow; ipow++)
        {
          xtemp = px * x - py * y;
          py = py * x + px * y;
          px = xtemp;
        }
        x = px + x0;
        y = py + y0;
        // fixed pows
        // xtemp = x*x - y*y + x0;
        // y = 2*x*y + y0;
        // x = xtemp;
        iteration++;
      }
      drawpixel(screen, scx, scy, iteration);
    }
  }
}

// draw julia set of given x0,y0
void DrawJulia(SDL_Surface *screen, double xmin, double xmax, double ymin, double ymax, double x0, double y0)
{
  double scalex = (xmax - xmin) / screen->w;
  double scaley = (ymax - ymin) / screen->h;
  double x, xtemp, y, px, py;
  int scx, scy, iteration, ipow;
  for (scy = 0; scy < screen->h; scy++)
  {
    for (scx = 0; scx < screen->w; scx++)
    {
      x = xmin + scx * scalex;
      y = ymin + scy * scaley;
      iteration = 0;
      while (x * x + y * y <= (2 * 2) && iteration < g_maxiterations)
      {
        // variable pow
        px = x;
        py = y;
        for (ipow = 1; ipow < g_pow; ipow++)
        {
          xtemp = px * x - py * y;
          py = py * x + px * y;
          px = xtemp;
        }
        x = px + x0;
        y = py + y0;
        // fixed pows
        // xtemp = x*x - y*y + x0;
        // y = 2*x*y + y0;
        // x = xtemp;
        iteration++;
      }
      drawpixel(screen, scx, scy, iteration);
    }
  }
}

// draw palette, julia or mandel, depending of 'mode'
void Draw(SDL_Surface *screen, int mode, double xmin, double xmax,
          double ymin, double ymax, double x0, double y0)
{
  // if(SDL_MUSTLOCK(screen)) {
  //  if(SDL_LockSurface(screen) < 0) return;
  // }
  switch (mode)
  {
  case MANDEL:
    DrawMandel(screen, xmin, xmax, ymin, ymax);
    break;
  case JULIA:
    DrawJulia(screen, xmin, xmax, ymin, ymax, x0, y0);
    break;
  case PALETTE:
    DrawPalette(screen);
    break;
  case RGB:
    DrawRGB(screen);
    break;
  }
  SDL_Flip(screen);
}

SDL_Surface *createscreen(int fullscreen, int bitsperpixel, int w = WIN_WIDTH, int h = WIN_HEIGHT)
{
  SDL_Surface *screen;
  Uint32 flags = 0x00;
  SDL_Quit();
  SDL_Init(SDL_INIT_VIDEO);
  flags = SDL_HWSURFACE | SDL_DOUBLEBUF;
  if (fullscreen)
  {
    if (!(screen = SDL_SetVideoMode(FS_WIDTH, FS_HEIGHT, bitsperpixel,
                                    flags | SDL_FULLSCREEN)))
      if (!(screen = SDL_SetVideoMode(1600, 1200, bitsperpixel, flags)))
        if (!(screen = SDL_SetVideoMode(800, 600, bitsperpixel, flags)))
          if (!(screen = SDL_SetVideoMode(640, 450, bitsperpixel, flags)))
            SDL_Quit();
  }
  else
  {
    if (!(screen = SDL_SetVideoMode(w, h, bitsperpixel, flags | SDL_RESIZABLE)))
      SDL_Quit();
  }
  return screen;
}

// init screen, manages inputs
int main(int argc, char *argv[])
{
  SDL_Surface *screen;
  SDL_Event event;
  int finished = 0;
  int mode = MANDEL;
  int autodecal = 0;
  bool fullscreen = false;
  double xmin, xmax, ymin, ymax, xwidth, ywidth, xcenter, ycenter, mousecurx, mousecury;
  double x0 = 0;
  double y0 = 0;
  if (!(screen = createscreen(fullscreen, g_bitsperpixel)))
  {
    return 1;
  }
  xmin = INIXMIN;
  xmax = INIXMAX;
  ymin = INIYMIN;
  ymax = INIYMAX;
  setpalette(screen);
  Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
  while (!finished)
  {
    if (g_bitsperpixel == 8 && autodecal)
    {
      g_decal_palette = (g_decal_palette + 1) % g_nbcolors;
      setpalette(screen);
    }
    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_VIDEORESIZE:
        if (!(screen = createscreen(fullscreen, g_bitsperpixel, event.resize.w, event.resize.h)))
        {
          return 1;
        }
        if (g_bitsperpixel == 8)
          setpalette(screen);
        Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
        break;
      case SDL_MOUSEMOTION:
        mousecurx = (double)event.motion.x;
        mousecury = (double)event.motion.y;
        break;
      case SDL_MOUSEBUTTONDOWN: // move
        xwidth = (xmax - xmin);
        ywidth = (ymax - ymin);
        xcenter = (double)event.button.x / screen->w * xwidth + xmin;
        ycenter = (double)event.button.y / screen->h * ywidth + ymin;
        switch (event.button.button)
        {
          // 1:left, 2:middle, 3:right
        case 1: // zoomin
          xmin = xcenter - xwidth / 4;
          xmax = xcenter + xwidth / 4;
          ymin = ycenter - ywidth / 4;
          ymax = ycenter + ywidth / 4;
          break;
        case 2: // move
          xmin = xcenter - xwidth / 2;
          xmax = xcenter + xwidth / 2;
          ymin = ycenter - ywidth / 2;
          ymax = ycenter + ywidth / 2;
          break;
        case 3: // zoomout
          xmin = xcenter - xwidth;
          xmax = xcenter + xwidth;
          ymin = ycenter - ywidth;
          ymax = ycenter + ywidth;
          break;
        }
        Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
        break;
      case SDL_QUIT:
        finished = 1;
        break;
      case SDL_KEYDOWN:
        xwidth = (xmax - xmin);
        ywidth = (ymax - ymin);
        xcenter = mousecurx / screen->w * xwidth + xmin;
        ycenter = mousecury / screen->h * ywidth + ymin;
        switch (event.key.keysym.sym)
        {
        case SDLK_c: // switch Palette demo
          if (mode == PALETTE && g_bitsperpixel == 32)
          {
            mode = RGB;
          }
          else
          {
            mode = PALETTE;
          }
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_d: // change depth (8 or 32)
          if (g_bitsperpixel == 8)
          {
            g_bitsperpixel = 32;
            g_maxiterations = MAXITER32;
            g_nbcolors = NBCOLORS32;
            g_decal_palette = g_decal_palette * MAXITER32 / MAXITER8;
          }
          else
          {
            g_bitsperpixel = 8;
            g_maxiterations = MAXITER8;
            g_nbcolors = NBCOLORS8;
            g_decal_palette = g_decal_palette * MAXITER8 / MAXITER32;
            if (mode == RGB)
              mode = PALETTE; // RGB mode doesnt exist in 8 bits mode
          }
          if (!(screen = createscreen(fullscreen, g_bitsperpixel)))
          {
            return 1;
          }
          if (g_bitsperpixel == 8)
            setpalette(screen);
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_f: // toggle full screen
          fullscreen = !fullscreen;
          if (!(screen = createscreen(fullscreen, g_bitsperpixel)))
          {
            return 1;
          }
          if (g_bitsperpixel == 8)
            setpalette(screen);
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_j: // switch julia
          x0 = mousecurx / screen->w * xwidth + xmin;
          y0 = mousecury / screen->h * ywidth + ymin;
          mode = JULIA;
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_SEMICOLON: // switch Mandel
          mode = MANDEL;
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_p: // change power
          if (g_pow++ == 8)
            g_pow = 2;
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_q: // a autodecal (qwerty)
          if (g_bitsperpixel == 8)
            autodecal = !autodecal;
          break;
        case SDLK_r: // reset to standard mandel
          g_pow = 2;
          xmin = INIXMIN;
          xmax = INIXMAX;
          ymin = INIYMIN;
          ymax = INIYMAX;
          mode = MANDEL;
          Draw(screen, mode, xmin, xmax, ymin, ymax, x0, y0);
          break;
        case SDLK_s: // switch palette colors
          if (g_bitsperpixel == 8)
          {
            g_switch_palette = g_switch_palette % 7 + 1;
            g_width_palette = 1;
            setpalette(screen);
          }
          break;
        case SDLK_z: // w change sin decal
          if (g_bitsperpixel == 8)
          {
            g_width_palette = g_width_palette % 10 + 1;
            setpalette(screen);
          }
          break;
        case SDLK_a: // q quit  (qwerty)
          finished = 1;
          break;
        }
        break;
      }
    }
  }
  SDL_Quit();
  return 0;
}
