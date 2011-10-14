#include <stdio.h>
#include <stdlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
//#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <time.h>

#define APP_NAME "Lines";
#define STR_ERR_FATAL "app:Lines. error is: %s"

#if defined TRUE
	#error "TRUE already defined"
#else
	#define TRUE ((unsigned char)0x01)
#endif

#if defined FALSE
	#error "FALSE already defined"
#else
	#define FALSE ((unsigned char)0x00)
#endif

#define max(a,b) ( (a) > (b) ? (a) : (b))
#define min(a,b) ( (a) > (b) ? (b) : (a))

Display *g_dpy = NULL;
Window g_wnd;

// Game logic begins
#define PLAY_FIELD_SZ_X 20
#define PLAY_FIELD_SZ_Y 20

#define PLAY_FIELD_MAX_BALLS (PLAY_FIELD_SZ_X * PLAY_FIELD_SZ_Y)

#define PLAY_BALL_COLOR_RED 0
#define PLAY_BALL_COLOR_GREEN 1
#define PLAY_BALL_COLOR_BLUE 2

// Quantity of balls spawned each time
#define PLAY_BALLS_SPAWN_Q 3
// Destroyable quantity of balls in row horz. or vert.
#define PLAY_BALLS_BOOM_MIN_Q 5
// Maximum quantity of destroyable balls.
//00100
//00100
//11X11
//00100
//00100
// So when adding new ball into that collection this will
// neven exceed max quantity and flows from min quantity
#define PLAY_BALLS_BOOM_MAX_Q ((PLAY_BALLS_BOOM_MIN_Q) * 4 - 3)

typedef struct {
	// 0x00 = in process
  // 0x01 = win
  // 0x02 = game over
	unsigned char state;
} game_state;

// Due to problems with storing and working with balls
// as set of pointers to global set, and Complexity that
// introduced to system, I decided to try another aproach
// where each cell will just know if it is occupied with a ball.
typedef struct {
	//ball color
	int col;
} field_ball;

typedef struct {
	unsigned char tag; // is cell occupied with a ball
	unsigned char color;
} field_cell;

typedef struct {
	field_cell fld[PLAY_FIELD_SZ_X][PLAY_FIELD_SZ_Y];
} play_field;

field_ball g_field_balls[PLAY_FIELD_MAX_BALLS];
int g_game_balls_counter;
play_field g_gamefield;
field_cell *g_spawncells[PLAY_BALLS_SPAWN_Q];
field_cell *g_selectedcell;

// This will add couple of random balls into field
void add_balls_onto_field(void) {
	int cl_spwn = 0;
	field_ball *bl;
	for(cl_spwn=0; cl_spwn < PLAY_BALLS_SPAWN_Q; ++cl_spwn) {
		bl = &g_field_balls[g_game_balls_counter++];

		// Create a random ball
		bl->col = rand() % 3;

		printf("adding ball with color: %i\n", bl->col);

		//g_gamefield.fld[rand() % 20][rand() % 20].ball = bl;
		g_spawncells[cl_spwn]->ball = bl;
	}
}

// This will update play field//ball color
void update_play_field(void) {
	int fld_x;
	int fld_y;
	field_cell *fcl = NULL;
	//counter for cells that will spawn
	int cl_spwn;

	//srand(time(NULL));

	// Parse through field
	for(fld_y=0; fld_y < PLAY_FIELD_SZ_Y; ++fld_y) {
		for(fld_x=0; fld_x < PLAY_FIELD_SZ_X; ++fld_x) {
			int ccx = rand() % 20;
			int ccy = rand() % 20;
			fcl = &g_gamefield.fld[ccx][ccy];
			// Check if current cell is not occupied.
			if(fcl->ball == NULL) {
				if(cl_spwn < PLAY_BALLS_SPAWN_Q) {
					printf("Will spawn in: x(%i) y(%i)\n", ccx, ccy);
					//and if not add it to free cells list, those will be cells where balls will spawn.
					g_spawncells[cl_spwn] = fcl;
					++cl_spwn;
				}
			}
		}
	}

	// now just add balls on random cells
	add_balls_onto_field();
}

// This will init all logics
void init_logics(void) {
	int cx, cy;
	g_game_balls_counter = 0;
	g_selectedcell = NULL;

	// traverse field and remove all balls from field
	for(cy=0; cy<PLAY_FIELD_SZ_Y; ++cy) {
		for(cx=0; cx<PLAY_FIELD_SZ_X; ++cx) {
			g_gamefield.fld[cx][cy].ball = NULL;
		}
	}

	// traverse complete balls list and init them with random parameters
	for(cx=0; cx<PLAY_FIELD_MAX_BALLS; ++cx) {
		g_field_balls[cx].col = 0 + rand() % 3;
	}

	// traverse spawn list and initialise it
	for(cx=0; cx < PLAY_BALLS_SPAWN_Q; ++cx) {
		g_spawncells[cx] = NULL;
	}

	// now spawn some balls
	update_play_field();
	add_balls_onto_field();
}

// This will select a ball on field
void select_ball_on_field(int clx, int cly) {
	g_selectedcell = &g_gamefield.fld[clx][cly];
}

// This will trace and collect all cells with the same color balls
// for target cell
void trace_and_destroy_cells_on_field(int clx, int cly) {
	// list of cells that are to destroy if their
	// quantity is more than PLAY_BALLS_INROW_Q
	field_cell *boomlist[PLAY_BALLS_BOOM_MAX_Q] = {NULL};
	int boom_count;

	boom_count = 0;

	// 1. starting from target cell we traverse up, down, left and right
	//    in the field checking if balls on those cells are the same color
	//    as the one on selected cell.
	// 2. add those cells into our boom list
	int cli_x = clx;
	int cli_y = cly;
	int cl_x_nmax = clx-min(clx, PLAY_BALLS_BOOM_MIN_Q-1);
	int cl_x_pmax = clx+min(PLAY_FIELD_SZ_X-1-clx, PLAY_BALLS_BOOM_MIN_Q-1);
	int cl_y_nmax = cly-min(cly, PLAY_BALLS_BOOM_MIN_Q-1);
	int cl_y_pmax = cly+min(PLAY_FIELD_SZ_Y-1-cly, PLAY_BALLS_BOOM_MIN_Q-1);
	field_cell *cl_test;

	// add target cell to list
	boomlist[boom_count++] = &g_gamefield.fld[clx][cly];

	// For negative X
	for(cli_x=clx; cli_x > cl_x_nmax; --cli_x) {
		cl_test = &g_gamefield.fld[cli_x][cly];
		// if there is no ball, then break
		if(cl_test->ball == NULL)
			break;
		//Stop if found cell with differently colored ball
		if(cl_test->ball->col != g_selectedcell->ball->col)
			break;
		// do not add selected cell
		if(cl_test == g_selectedcell)
			continue;
		boomlist[boom_count++]=cl_test;	// add to list
	}
	// For positive X
	for(cli_x=clx; cli_x < cl_x_pmax; ++cli_x) {
		cl_test = &g_gamefield.fld[cli_x][cly];
		// if there is no ball, then break
		if(cl_test->ball == NULL)
			break;
		//Stop if found cell with differently colored ball
		if(cl_test->ball->col != g_selectedcell->ball->col)
			break;
		// do not add selected cell
		if(cl_test == g_selectedcell)
			continue;
		boomlist[boom_count++]=cl_test;	// add to list
	}
	// For negative Y// add to list
	for(cli_y=cly; cli_y > cl_y_nmax; --cli_y) {
		cl_test = &g_gamefield.fld[clx][cli_y];
		// if there is no ball, then break
		if(cl_test->ball == NULL)
			break;
		//Stop if found cell with differently colored ball
		if(cl_test->ball->col != g_selectedcell->ball->col)
			break;
		// do not add selected cell
		if(cl_test == g_selectedcell)
			continue;
		boomlist[boom_count++]=cl_test;	// add to list
	}
	// For positive Y
	for(cli_y=cly; cli_y < cl_y_pmax; ++cli_y) {
		cl_test = &g_gamefield.fld[clx][cli_y];
		// if there is no ball, then break
		if(cl_test->ball == NULL)
			break;
		//Stop if found cell with differently colored ball
		if(cl_test->ball->col != g_selectedcell->ball->col)
			break;
		// do not add selected cell
		if(cl_test == g_selectedcell)
			continue;
		boomlist[boom_count++]=cl_test;	// add to list
	}

	// Now check if we reached quantity that exceedes minimum needed for BA-DA-BOOM!!!
	if(boom_count >= PLAY_BALLS_BOOM_MIN_Q) {
    //inflate global ball counter
		//g_game_balls_counter -= boom_count;
		//Now traverse boom list
		//CHEAT #1: to index boom list with boom_count var we need to decrease it by 1
		while(--boom_count) {
			//clear ball from cell
			boomlist[boom_count--]->ball = NULL;
			--g_game_balls_counter;
		}

		//if selected cell booms it's ball then no
		//need to leave it selected.
		g_selectedcell = NULL;
	}
}

// This will:
// 1. move selected ball onto cell
// 2. check if player collected a row
// 3. destroy a row if (2) is satisfied
// 4. spawn new balls onto field
void move_ball_on_field(int clx, int cly) {
	field_cell *cl;

	if(g_selectedcell == NULL)
		return;

	cl = &g_gamefield.fld[clx][cly];

	// Move ball from selected cell to cell that was hit by click
	// and choose new cell as celected
	cl->ball = g_selectedcell->ball;
	g_selectedcell->ball = NULL;
	g_selectedcell = cl;

	//Try to destroy nearby cells
	trace_and_destroy_cells_on_field(clx, cly);

	//now update field
	update_play_field();
}

// Next goes interesting piece of code
// first of all we going to check if there
// is a ball on selected cell logicaly, then
// if there is a ball we will check if we already selected
// any ball logically, if that is so
// we will change our chosen ball to new one
// if there was no ball on cell we going to check if we have
// any ball selected, if that is so then
// we going to move selected ball onto clicked cell
// and at last if there was no ball selected then we just ignore
// this click
void hit_cell_on_field(int clx, int cly) {
	// Check cell if it has a ball
	if(g_gamefield.fld[clx][cly].ball != NULL) {
			select_ball_on_field(clx, cly);
	} else {
		move_ball_on_field(clx, cly);
	}
}
// Game logic ends here

// Game graphics starts here
#define PLAY_GFX_CELL_WIDTH 20.0f
#define PLAY_GFX_CELL_HEIGHT 20.0f

#define PLAY_GFX_TO_LOGIC_CL_X 1.0f/PLAY_GFX_CELL_WIDTH
#define PLAY_GFX_TO_LOGIC_CL_Y 1.0f/PLAY_GFX_CELL_HEIGHT

#define GLLIST_DRAW_FIELD 1

typedef struct {
	GLfloat x, y;
	GLfloat width, height;
	// color
	GLfloat r, g, b, a;
	//logical cell
	field_cell *lcl;
} play_gfx_cell;

typedef struct {
	GLfloat x, y;
	GLfloat width, height;
	// color
	GLfloat r, g, b, a;
	// cells
	play_gfx_cell cells[PLAY_FIELD_SZ_X][PLAY_FIELD_SZ_Y];
} play_gfx_field;

play_gfx_field g_game_gfx_field;

// This will initialise whole game gfx sub-system
void init_game_gfx(void) {
	int cx, cy;
	play_gfx_cell *cl;

	// initialise field layer sizes and color
	g_game_gfx_field.x = 0;
	g_game_gfx_field.y = 0;
	g_game_gfx_field.width = g_game_gfx_field.x + PLAY_FIELD_SZ_X * PLAY_GFX_CELL_WIDTH;
	g_game_gfx_field.height = g_game_gfx_field.y + PLAY_FIELD_SZ_Y * PLAY_GFX_CELL_HEIGHT;
	g_game_gfx_field.r = 0.3f;
	g_game_gfx_field.g = 0.1f;
	g_game_gfx_field.b = 0.33f;
	g_game_gfx_field.a = 0.43f;

	// traverse gfx field cells and set their sizes and positions
	for(cy=0; cy < PLAY_FIELD_SZ_Y; ++cy) {
		for(cx=0; cx < PLAY_FIELD_SZ_X; ++cx) {
			cl = &g_game_gfx_field.cells[cx][cy];

			cl->width = PLAY_GFX_CELL_WIDTH;
			cl->height = PLAY_GFX_CELL_HEIGHT;
			cl->x = g_game_gfx_field.x + cx * PLAY_GFX_CELL_WIDTH;
			cl->y = g_game_gfx_field.y + cy * PLAY_GFX_CELL_HEIGHT;

			cl->r = 0.4f;
			cl->g = 0.4f;
			cl->b = 0.4f;
			cl->a = 0.43f;

			cl->lcl = &g_gamefield.fld[cx][cy];
		}
	}
}

// This will traverse whole field and:
// 1. determine if player selected ball or selected cell where ball should move
void click_on_field(GLfloat xpos, GLfloat ypos) {
	// logic coordinates of cell in the field that was hit by click
	int cl_logic_x, cl_logic_y;

	//check if click was on field
	if((xpos < g_game_gfx_field.x) || (xpos > (g_game_gfx_field.x+g_game_gfx_field.width)))
		return;
	if((ypos < g_game_gfx_field.y) || (ypos > (g_game_gfx_field.y+g_game_gfx_field.height)))
		return;

	// determine logic cell coordinates
	cl_logic_x = (int)(xpos*PLAY_GFX_TO_LOGIC_CL_X-g_game_gfx_field.x);
	cl_logic_y = (int)(ypos*PLAY_GFX_TO_LOGIC_CL_Y-g_game_gfx_field.y);

	// run logic part
	hit_cell_on_field(cl_logic_x, cl_logic_y);
}

// This will re-draw whole game gfx field
void draw_field(void) {
	int cx, cy;
	play_gfx_cell *cl;

// 	glCallList(GLLIST_DRAW_FIELD);

// 	glNewList(GLLIST_DRAW_FIELD, GL_COMPILE);
	glBegin(GL_QUADS);
		glColor4f(g_game_gfx_field.r, g_game_gfx_field.g, g_game_gfx_field.b, g_game_gfx_field.a);
		glVertex3f(g_game_gfx_field.x, g_game_gfx_field.y, 0.0f);
		glVertex3f(g_game_gfx_field.x+g_game_gfx_field.width, g_game_gfx_field.y, 0.0f);
		glVertex3f(g_game_gfx_field.x+g_game_gfx_field.width, g_game_gfx_field.y+g_game_gfx_field.height, 0.0f);
		glVertex3f(g_game_gfx_field.x, g_game_gfx_field.y+g_game_gfx_field.height, 0.0f);
	glEnd();
	for(cy=0; cy < PLAY_FIELD_SZ_Y; ++cy) {
		for(cx=0; cx < PLAY_FIELD_SZ_X; ++cx) {
			cl = &g_game_gfx_field.cells[cx][cy];
			glBegin(GL_QUADS);
				glColor4f(cl->r, cl->g, cl->b, cl->a);
				glVertex3f(cl->x, cl->y, 0.1f);
				glVertex3f(cl->x+cl->width, cl->y, 0.1f);
				glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.1f);
				glVertex3f(cl->x, cl->y+cl->height, 0.1f);
			glEnd();
			glBegin(GL_LINES);
				glColor4f(0.5f, 0.3f, 0.0f, 1.0f);
				glVertex3f(cl->x, cl->y, 0.3f);
				glVertex3f(cl->x+cl->width, cl->y, 0.3f);
				glVertex3f(cl->x+cl->width, cl->y, 0.3f);
				glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.3f);
				glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.3f);
				glVertex3f(cl->x, cl->y+cl->height, 0.3f);
				glVertex3f(cl->x, cl->y+cl->height, 0.3f);
				glVertex3f(cl->x, cl->y, 0.3f);

			if(cl->lcl->ball) {
				// a verrrryyy expensive and fat code for color
				switch(cl->lcl->ball->col)
				{
					case PLAY_BALL_COLOR_RED:
						glColor4f(1.0f, 0.0f, 0.0f, 0.0f); break;
					case PLAY_BALL_COLOR_GREEN:
						glColor4f(0.0f, 1.0f, 0.0f, 0.0f); break;
					case PLAY_BALL_COLOR_BLUE:
						glColor4f(0.0f, 0.0f, 1.0f, 0.0f); break;
				}
				glVertex3f(cl->x, cl->y, 0.5f);
				glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.5f);
				glVertex3f(cl->x+cl->width, cl->y, 0.5f);
				glVertex3f(cl->x, cl->y+cl->height, 0.5f);
			}
			glEnd();
		}
	}
// 	glEndList();
}

void draw_tools(void) {
	glBegin(GL_LINES);
		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3f(-10.0f, 0.0f, 0.0f);
		glVertex3f(10.0f, 0.0f, 0.0f);
		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3f(0.0f, -10.0f, 0.0f);
		glVertex3f(0.0f, 10.0f, 0.0f);
		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, -10.0f);
		glVertex3f(0.0f, 0.0f, 10.0f);
	glEnd();
}
// Game graphics ends here

typedef struct {
	unsigned char run;
} rs_app_settings;

typedef struct {
	GLboolean dbufferred;
	GLint wnd_width;
	GLint wnd_height;
	GLclampf clear_r, clear_g, clear_b, clear_a;
} game_options;

typedef struct {
	GLfloat x, y, z;
	GLfloat rx, ry, rz;
} field_location;

rs_app_settings g_app_opts;
field_location g_playereye;
game_options g_gameopts;

static int g_dblbuff[] = {GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None};

void fatal_err(const char *s_msg) {
	fprintf(stderr, STR_ERR_FATAL, s_msg);
	exit(1);
}

void init_options(void) {
	g_gameopts.clear_r = 1.0f;
	g_gameopts.clear_g = 1.0f;
	g_gameopts.clear_b = 1.0f;
	g_gameopts.clear_a = 1.0f;

	g_gameopts.wnd_width = 800;
	g_gameopts.wnd_height = 600;

	g_gameopts.dbufferred = GL_TRUE;

	//set player position
	g_playereye.x = 0.0f;
	g_playereye.y = 0.0f;
	g_playereye.z = -0.5f;

	g_playereye.rx = 0.0f;
	g_playereye.ry = 0.0f;
	g_playereye.rz = 0.0f;
}

void setup_projection(void) {
	glViewport(0, 0, g_gameopts.wnd_width, g_gameopts.wnd_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0.0, g_gameopts.wnd_width, g_gameopts.wnd_height, 0.0, -1.0, 1.0);
	//glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 1000.0);
}

void update_player_eye(void) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0.0f, 0.0f, 0.0f);
	glRotatef(0.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(0.0f, 0.0f, 1.0f, 0.0f);
	glRotatef(0.0f, 0.0f, 0.0f, 1.0f);

// 	glTranslatef(g_playereye.x, g_playereye.y, g_playereye.z);
// 	glRotatef(g_playereye.rx, 1.0f, 0.0f, 0.0f);
// 	glRotatef(g_playereye.ry, 0.0f, 1.0f, 0.0f);
// 	glRotatef(g_playereye.rz, 0.0f, 0.0f, 1.0f);
}

void redraw_scene(void) {
	glClearColor(g_gameopts.clear_r, g_gameopts.clear_g, g_gameopts.clear_b, g_gameopts.clear_a);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	update_player_eye();

	draw_field();

	//draw all tools we need
	//draw_tools();

	glXSwapBuffers(g_dpy, g_wnd);
}

int main(int argc, char *argv[]) {
	XVisualInfo *xvi = NULL;
	XSetWindowAttributes xswa;
	Colormap cmp;
	GLXContext glx_ct;
	XEvent event;
	int dummy;

	//Initialise random seed
	srand(time(NULL));

	// Init basic options first
	init_options();

	// Connect to X display;
	g_dpy = XOpenDisplay(NULL);
	if(g_dpy == NULL) {
		fatal_err("can't open display :0");
	}
	// Find out if GLX is up and running on this X server
	if(!glXQueryExtension(g_dpy, &dummy, &dummy)) {
		fatal_err("this X server has no GLX extension supported.");
	}

	// Find an appropriate visual for us
	if(g_gameopts.dbufferred) {
		xvi = glXChooseVisual(g_dpy, DefaultScreen(g_dpy), g_dblbuff);
	}
	if(xvi == NULL) {
		fatal_err("failed to get appropriate visual.");
	}

	//Create glx context
	glx_ct = glXCreateContext(g_dpy, xvi, None, GL_TRUE);
	if(glx_ct == NULL) {
		fatal_err("failed to create a glx context.");
	}

	// Create X window with selected visual
	cmp = XCreateColormap(g_dpy, RootWindow(g_dpy, xvi->screen), xvi->visual, AllocNone);
	xswa.colormap = cmp;
	xswa.border_pixel = 0;
	xswa.event_mask = KeyPressMask | ExposureMask | ButtonPressMask | StructureNotifyMask | PointerMotionMask;

	g_wnd = XCreateWindow(g_dpy, RootWindow(g_dpy, xvi->screen), 0, 0, g_gameopts.wnd_width, g_gameopts.wnd_height, 0,
							xvi->depth, InputOutput, xvi->visual, CWBorderPixel | CWColormap | CWEventMask, &xswa);
	XSetStandardProperties(g_dpy, g_wnd, "lines_for_mom", "lines_ico", None, argv, argc, NULL);

	// Now bind glx context to window
	glXMakeCurrent(g_dpy, g_wnd, glx_ct);

	// Request window to be dsplayed on screen ("map" window)
	XMapWindow(g_dpy, g_wnd);

	// Set GL depth parameters
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClearDepth(1.0);


	setup_projection();

	//Initialise game logics
	init_logics();

	//Initialise game gfx
	init_game_gfx();

	g_app_opts.run = TRUE;

	// Now enter a main cycle of program
	while(g_app_opts.run) {
		do {
			XNextEvent(g_dpy, &event);

			switch(event.type) {
				case KeyPress:
				{
					KeySym keysym;
					XKeyEvent *key_event;
					char buffer[1];

					key_event = (XKeyEvent *)&event;
					if( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
						(keysym == (KeySym)XK_Escape) ) {

						// not gentle way to quit the app !
						//exit(0);
						// so we will rather use app settings
						g_app_opts.run = FALSE;
					}

					if( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
						(keysym == (KeySym)'w') ) {
						g_playereye.z += 10.0f;
					}

					if( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
						(keysym == (KeySym)'s') ) {
						g_playereye.z -= 10.0f;
					}
				} break;
				case MotionNotify:
				{
					XMotionEvent *m_ev = (XMotionEvent*)&event;
					g_playereye.rx = m_ev->y * 0.08f;
					g_playereye.ry = m_ev->x * 0.08f;
				} break;
				case ButtonPress:
				{
					switch(event.xbutton.button) {
						case 1: {
							// Means that player have selected a ball on field
							// or selected empty cell on field where selected
							// ball should move
							click_on_field(event.xmotion.x, event.xmotion.y);
						} break;
						case 2: {
						} break;
						case 3: {
						} break;
					}
				} break;
				case ConfigureNotify:
				{
					g_gameopts.wnd_width = event.xconfigure.width;
					g_gameopts.wnd_height = event.xconfigure.height;
					setup_projection();
				} break;
			}
		} while(XPending(g_dpy));

		// Redraw whole scene
		redraw_scene();
	}

	return 0;
}
