#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

static uint32_t cell_w_px,cell_w_inner_px,cell_w_margin_px,cell_w_px_adj;
static uint32_t window_w,window_h;
static uint16_t cell_w,cell_h,cell_n;
static uint16_t snake_append = 0;
static uint32_t render_l,render_t;
static SDL_Renderer* renderer;
static SDL_Window* window;
static const uint32_t color_main = 0xffffffff,color_bg = 0x000000ff,color_food = 0xff0000ff,color_head = 0xff007fff,ymask = (1 << 16) - 1;
static uint32_t *snake,*snake_e,*snake_l,*snake_r;
static uint16_t food_x,food_y;
static uint8_t *tally;
static uint16_t dir,ldir;
static bool paused = false;
static bool fullscreen = false;
static bool recalc_queue = false;

static uint16_t snake_size()
{
	if(snake_l <= snake_r)
		return snake_r - snake_l + 1;
	return snake_e - snake_l + snake_r - snake + 1;
}

static void draw_cell(uint16_t x,uint16_t y)
{
	SDL_Rect rect =
	{
		.x = render_l + cell_w_px * x + cell_w_margin_px,
		.y = render_t + cell_w_px * y + cell_w_margin_px,
		.w = cell_w_inner_px,
		.h = cell_w_inner_px,
	};
	SDL_RenderFillRect(renderer,&rect);
}

static void draw_borders()
{
	SDL_Rect rect;
	rect.x = render_l;
	rect.y = render_t;
	rect.w = cell_w * cell_w_px;
	rect.h = cell_w_margin_px;
	SDL_RenderFillRect(renderer,&rect);
	rect.y = render_t + cell_h * cell_w_px - cell_w_margin_px;
	SDL_RenderFillRect(renderer,&rect);
	rect.y = render_t;
	rect.w = cell_w_margin_px;
	rect.h = cell_h * cell_w_px;
	SDL_RenderFillRect(renderer,&rect);
	rect.x = render_l + cell_w * cell_w_px - cell_w_margin_px;
	SDL_RenderFillRect(renderer,&rect);
}

static Uint32 push_tick_event(Uint32 interval,void *param)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.type = SDL_USEREVENT;
	SDL_PushEvent(&e);
	return interval;
}

static void set_color(uint32_t color)
{
	SDL_SetRenderDrawColor(renderer,color >> 24,color >> 16 & 0xff,color >> 8 & 0xff,color & 0xff);
}

static void repaint()
{
	set_color(color_bg);
	SDL_RenderFillRect(renderer,NULL);
	set_color(color_main);
	draw_borders();
	for(uint32_t* i = snake_l;i != snake_r;i = i + 1 == snake_e ? snake : i + 1)
		draw_cell(*i >> 16,*i & ymask);
	set_color(color_head);
	draw_cell(*snake_r >> 16,*snake_r & ymask);
	set_color(color_food);
	draw_cell(food_x,food_y);
	SDL_RenderPresent(renderer);
}

static bool check_snake_collide(uint16_t x,uint16_t y)
{
	uint32_t cpos = x << 16 | y;
	for(uint32_t* i = snake_l;i != snake_r;i = i + 1 == snake_e ? snake : i + 1)
		if(*i == cpos)
			return true;
	if(*snake_r == cpos)
		return true;
	return false;
}

static uint16_t rand16(uint16_t min,uint16_t max)
{
	return (uint64_t)rand() * (max - min) / RAND_MAX + min;
}

static void rand_food()
{
	// printf("%d %d\n",cell_n,snake_size());
	uint16_t k = rand16(0,cell_n - snake_size()),i = 0;
	do
	{
		if(tally[i] == 0)
		{
			if(k == 0)
				break;
			--k;
		}
		++i;
	}
	while(i < cell_n);
	food_x = i % cell_w;
	food_y = i / cell_w;
	// printf("%d %d %d\n",i,food_x,food_y);
}

static void reset()
{
	uint16_t mx = cell_w / 2,my = cell_h / 2;
	snake_l = snake_r = snake;
	*snake_l = mx << 16 | my;
	dir = 4;
	ldir = 4;
	memset(tally,0,sizeof(*tally) * cell_n);
	tally[mx + my * cell_w] = 1;
	rand_food();
	snake_append = 2;
}

static void recalc_render()
{
	SDL_GetRendererOutputSize(renderer,&window_w,&window_h);
	uint32_t w_px = window_w / cell_w;
	uint32_t h_px = window_h / cell_h;
	cell_w_px = w_px > h_px ? h_px : w_px;
	cell_w_px += cell_w_px_adj;
	cell_w_inner_px = cell_w_px * 7 / 8;
	cell_w_margin_px = (cell_w_px - cell_w_inner_px) / 2;
	render_l = window_w / 2 - cell_w * cell_w_px / 2;
	render_t = window_h / 2 - cell_h * cell_w_px / 2;
}

int main(int argc,char **argv)
{
	srand(time(NULL));
	if(SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;
	window = SDL_CreateWindow("Snake",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,1000,900,SDL_WINDOW_SHOWN);
	if(!window)
		return 2;
	renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
	//SDL_SetWindowFullscreen(window,true);
	SDL_GetRendererOutputSize(renderer,&window_w,&window_h);
	// cell_w_px = 30;
	// cell_w = window_w / cell_w_px,cell_h = window_h / cell_w_px,cell_n = cell_w * cell_h;
	// printf("%d %d\n",cell_w,cell_h);
	cell_w = 50;
	cell_h = 30;
	cell_n = cell_w * cell_h;
	recalc_render();
	uint32_t tick_ms = 80,end_wait_ticks = 5,end_wait_cur = end_wait_ticks;
	SDL_AddTimer(tick_ms,&push_tick_event,NULL);
	snake = malloc(sizeof(*snake) * cell_n);
	snake_e = snake + cell_n;
	tally = malloc(sizeof(*tally) * cell_n);
	reset();
	repaint();
	SDL_Event e;
	while(SDL_WaitEvent(&e))
	{
		switch(e.type)
		{
			case SDL_QUIT:
				return 0;
			case SDL_KEYUP:
			{
				SDL_KeyboardEvent* ke = (SDL_KeyboardEvent*)&e;
				switch(ke->keysym.scancode)
				{
					case SDL_SCANCODE_UP:
					case SDL_SCANCODE_W:
						if(ldir != 1)
							dir = 0;
					break;
					case SDL_SCANCODE_DOWN:
					case SDL_SCANCODE_S:
						if(ldir != 0)
							dir = 1;
					break;
					case SDL_SCANCODE_LEFT:
					case SDL_SCANCODE_A:
						if(ldir != 3)
							dir = 2;
					break;
					case SDL_SCANCODE_RIGHT:
					case SDL_SCANCODE_D:
						if(ldir != 2)
							dir = 3;
					break;
					case SDL_SCANCODE_P:
						paused = !paused;
					break;
					case SDL_SCANCODE_Y:
						++snake_append;
					break;
					case SDL_SCANCODE_ESCAPE:
						return 0;
					case SDL_SCANCODE_U:
						++cell_w_px_adj;
						recalc_queue = true;
					break;
					case SDL_SCANCODE_I:
						--cell_w_px_adj;
						recalc_queue = true;
					break;
					case SDL_SCANCODE_N:
					break;
					case SDL_SCANCODE_F11:
						fullscreen = !fullscreen;
						SDL_SetWindowFullscreen(window,fullscreen);
						recalc_queue = true;
					break;
				}
			}
			break;
			case SDL_USEREVENT: /* tick */
			{
				if(recalc_queue)
				{
					recalc_queue = false;
					recalc_render();
				}
				if(++end_wait_cur < end_wait_ticks)
					goto tick_repaint;
				if(paused)
					goto tick_repaint;
				if(end_wait_cur == end_wait_ticks)
				{
					reset();
					goto tick_repaint;
				}
				uint16_t hx = *snake_r >> 16,
						 hy = *snake_r & ymask;
				ldir = dir;
				switch(dir)
				{
					case 0:
						--hy;
					break;
					case 1:
						++hy;
					break;
					case 2:
						--hx;
					break;
					case 3:
						++hx;
					break;
					case 4:
					goto tick_repaint;
				}
				if(!(hx < cell_w && hy < cell_h))
					goto game_over;
				uint32_t* snake_l_old = snake_l;
				bool need_rand_food = false;
				if(!(food_x == hx && food_y == hy || snake_append))
				{
					tally[(*snake_l >> 16) + (*snake_l & ymask) * cell_w] = 0;
					++snake_l;
					if(snake_l == snake_e) snake_l = snake;
				}
				else
				{
					if(food_x == hx && food_y == hy)
						need_rand_food = true;
					else
						--snake_append;
				}
				uint32_t cpos = hx << 16 | hy;
				tally[hx + hy * cell_w] = 1;
				++snake_r;
				if(snake_r == snake_e) snake_r = snake;
				*snake_r = cpos;
				for(uint32_t* i = snake_l;i != snake_r;i = i + 1 == snake_e ? snake : i + 1)
					if(*i == cpos)
						goto game_over;
				if(need_rand_food)
					rand_food();
				goto tick_repaint;
game_over:
				end_wait_cur = 0;
tick_repaint:
				repaint();
			}
			break;
		}
	}
}
