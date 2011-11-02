#include <stdio.h>
#include <stdlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
//#include <GL/glut.h>
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

#define PLAY_BALL_COLOR_RED 1
#define PLAY_BALL_COLOR_GREEN 2
#define PLAY_BALL_COLOR_BLUE 3

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

#define PLAY_BALLS_BOOM_MAX_RECURSIVE_Q ((PLAY_BALLS_BOOM_MIN_Q*2+1)*(PLAY_BALLS_BOOM_MIN_Q*2+1))

/** [v0.3] Some of states for logical cell are introduced
 * now instead storing cells that need to be destroyed in
 * arrays in trace_and_destroy functions, we simply set
 * cell state as "NEEDTOEXPLODE" and then destroy it in update
 * routine state tag now contains flags of states
 * i.e. state can be OCCUPIED|COLLECTED
 */
#define LGS_FIELDCELL_STATE_OCCUPIED ((unsigned char)0x01)
#define LGS_FIELDCELL_STATE_EXPLODING ((unsigned char)0x02)
#define LGS_FIELDCELL_STATE_COLLECTED ((unsigned char)0x04)

/** [v0.3] macro to set, clear and toggle flags
 */
#define SET_FLAG(m, flag) (m |= (flag))
#define CLEAR_FLAG(m, flag) (m &= ~(flag))
#define TOGGLE_FLAG(m, flag) (m ^= (flag))

/** [v0.2] Due to problems with storing and working with balls
 * as set of pointers to global set, and Complexity that
 * introduced to system, I decided to try another aproach
 * where each cell will just know if it is occupied with a ball.
 */

typedef struct {
	// since [v0.3] describes logical state of cell
    unsigned char tag_state;
    unsigned char color;
} lgs_field_cell;

typedef struct {
    lgs_field_cell fld[PLAY_FIELD_SZ_X][PLAY_FIELD_SZ_Y];
} lgs_field;

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
    // corresponding cell in logics
    lgs_field_cell *lcl;

#if defined (VISUAL_DEBUG)
	// marker for "boom" condition
    // to trace visually what is happening in trace
    // method, the finit result
    unsigned char scorchmark;
	
	//scorchmark alpha to make them go invisible and then remove
	GLfloat a_scorchmark;
#endif
} gfx_field_cell;

typedef struct {
    GLfloat x, y;
    GLfloat width, height;
    // color
    GLfloat r, g, b, a;
    // gfx cells
    gfx_field_cell cells[PLAY_FIELD_SZ_X][PLAY_FIELD_SZ_Y];
} gfx_field;

/** [v0.3] introduced game state flags
 */
#define GAME_STATE_RUNNING 0x01
#define GAME_STATE_WIN	0x02
#define GAME_STATE_SPAWNBALLS 0x04

typedef struct {
    lgs_field_cell *lcl;
    gfx_field_cell *gcl;
} game_field_cell;

typedef struct {
	game_field_cell cells[PLAY_FIELD_SZ_X][PLAY_FIELD_SZ_Y];
} game_field;

typedef struct {
    // 0x00 = in process
    // 0x01 = win
    // 0x02 = game over
    unsigned char state;
	// [v0.3] game level
	unsigned char level;
} game_state;

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

lgs_field g_lgs_field; // main logic collection to represent game field
gfx_field g_gfx_field;
game_field g_game_field;

game_field_cell *g_selectedcell;

field_location g_playereye;
game_options g_gameopts;
game_state g_gamestate;

void game_hit_play_field(GLfloat xpos, GLfloat ypos);
void game_set_cell_to_explode(game_field_cell *gcl);
void game_spawn_ball_on_field(game_field_cell *cl);

// This will be called only when spawning ball onto field
void lgs_spawn_ball(lgs_field_cell *cl) {
	if(cl == NULL) {
		printf("Can't occupy cell (%X), it is NULL.\n", cl);
	}
	
	if(!(cl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED)) {
		SET_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_OCCUPIED);
		cl->color = 1+rand()%3;
		printf("cell (%X) is now occupied (%02X): %i\n", cl, cl->tag_state, cl->color);
	}
}

// This will free ball on field
void lgs_set_cell_free(lgs_field_cell *cl) {
	if(cl == NULL) {
		printf("Can't free cell (%X), it is NULL.\n", cl);
		return;
	}
	CLEAR_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_OCCUPIED);
	CLEAR_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_COLLECTED);
	cl->color = 0x00;
	printf("cell (%X) freed (%02X): %i\n", cl, cl->tag_state, cl->color);
}

// This will exchange ball between two cells
void lgs_move_ball(lgs_field_cell *cl_src, lgs_field_cell *cl_dest) {
	if((cl_src != NULL) && (cl_dest != NULL)) {
		cl_dest->tag_state = cl_src->tag_state;
		cl_dest->color = cl_src->color;
		
		// set src cell free
		lgs_set_cell_free(cl_src);
	}
}

// This will boom supplied ball
void lgs_explode_cell(lgs_field_cell *cl) {
	if(cl == NULL) {
		printf("can't remove cell (%X), it is NULL.\n", cl);
	}

	CLEAR_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_OCCUPIED);
	CLEAR_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_COLLECTED);
	cl->color = 0x00;
	
	CLEAR_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_EXPLODING);
	
	printf("cell (%X) freed (%02X): %i\n", cl, cl->tag_state, cl->color);
}

// This will init all logics
void init_logics(void) {
    //g_selectedcell = NULL;
}

// Special function that will handle adding cells to
// "destroy" lists
void put_cell_to_boom_list(lgs_field_cell *l[], int* counter, lgs_field_cell *cl) {
    l[(*counter)++]=cl;
	// mark cell as collected one, so can't add it again into boom list
	SET_FLAG(cl->tag_state, LGS_FIELDCELL_STATE_COLLECTED);
	
	printf("collecting cell (%X), collected %i.\n", cl, *counter);
}

// This will remove cell from boom list
void remove_cell_from_boom_list(lgs_field_cell *l[], int counter) {
	CLEAR_FLAG(l[counter]->tag_state, LGS_FIELDCELL_STATE_COLLECTED);
}

/**
 * This will check a target cell neighbours
 * [x-1, y]
 * [x, y-1]
 * [x, y+1]
 * [x+1, y]
 * if found the same colored ball, then add to list and jump into cell
 */
void trace_and_destroy_knl(int cx, int cy, lgs_field_cell *l[], int* counter) {
	if(cx == PLAY_FIELD_SZ_X || cx < 0)
		return;
	if(cy == PLAY_FIELD_SZ_Y || cy < 0)
		return;

	lgs_field_cell *cl = &g_lgs_field.fld[cx][cy];
	lgs_field_cell *t_cl; // tentative cell;

	// [x+1, y] case
	if(cx+1 < PLAY_FIELD_SZ_X) {
		t_cl = &g_lgs_field.fld[cx+1][cy];
		if((t_cl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED) && t_cl->color == cl->color && !(t_cl->tag_state & LGS_FIELDCELL_STATE_COLLECTED)) {
			put_cell_to_boom_list(l, counter, t_cl);
			trace_and_destroy_knl(cx+1, cy, l, counter);
		}
	}
	
	// [x-1, y] case
	if(cx-1 >= 0) {
		t_cl = &g_lgs_field.fld[cx-1][cy];
		if((t_cl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED) && t_cl->color == cl->color && !(t_cl->tag_state & LGS_FIELDCELL_STATE_COLLECTED)) {
			put_cell_to_boom_list(l, counter, t_cl);
			trace_and_destroy_knl(cx-1, cy, l, counter);
		}
	}
}

/**
 * method: It will trace all adjacent cells on field with same color
 * and add them into destroy list.
 * This method works recursively traveling each cell and checking it
 * and it's neighbours
 */
void trace_and_destroy_recursive_collection(int clx, int cly) {
	lgs_field_cell *boomlist[PLAY_BALLS_BOOM_MAX_RECURSIVE_Q] = {NULL};
	int boomcount = 0;
	
	lgs_field_cell *t_cl = &g_lgs_field.fld[clx][cly];
	
	// add initial target cell into boom list
	put_cell_to_boom_list(boomlist, &boomcount, t_cl);
	
	// call trace_and_destroy recursive kernel
	trace_and_destroy_knl(clx, cly, boomlist, &boomcount);
	
	// Check if it is worth to add final target cell to boom list
	// destroy collected cells
	if (boomcount >= PLAY_BALLS_BOOM_MIN_Q) {
		printf("boom: %i balls.\n", boomcount);
        
		//Now traverse boom list
        //CHEAT #1: to index boom list with boom_count var we need to decrease it by 1
        while (--boomcount >= 0) {
            SET_FLAG(boomlist[boomcount]->tag_state, LGS_FIELDCELL_STATE_EXPLODING);
        }

        //if selected cell booms it's ball then no
        //need to leave it selected.
    } else {
		while (--boomcount >= 0) {
            remove_cell_from_boom_list(boomlist, boomcount);
        }
	}
}

// This will trace and collect all cells with the same color balls
// for target cell
void trace_and_destroy_cross_collection(int clx, int cly) {
    // list of cells that are to destroy if their
    // quantity is more than PLAY_BALLS_INROW_Q
    lgs_field_cell *boomlist[PLAY_BALLS_BOOM_MAX_Q] = {NULL};
    int boom_count = 0;

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
    lgs_field_cell *cl_test;

    // add target cell to list
    put_cell_to_boom_list(boomlist, &boom_count, &g_lgs_field.fld[clx][cly]);
    //boomlist[boom_count++] = &g_gamefield.fld[clx][cly];

    // For negative X
    for (cli_x=clx-1, cli_y=cly; cli_x >= cl_x_nmax; --cli_x) {
        cl_test = &g_lgs_field.fld[cli_x][cly];
        // if there is no ball, then break
        if (!(cl_test->tag_state & LGS_FIELDCELL_STATE_OCCUPIED))
            break;
        //Stop if found cell with differently colored ball
        if (cl_test->color != g_selectedcell->lcl->color)
            break;
        // do not add selected cell
        //if(cl_test == g_selectedcell)
        //continue;
        put_cell_to_boom_list(boomlist, &boom_count, cl_test);	// add to list
    }
    // For positive X
    for (cli_x=clx+1, cli_y=cly; cli_x <= cl_x_pmax; ++cli_x) {
        cl_test = &g_lgs_field.fld[cli_x][cly];
        // if there is no ball, then break
        if (!(cl_test->tag_state & LGS_FIELDCELL_STATE_OCCUPIED))
            break;
        //Stop if found cell with differently colored ball
        if (cl_test->color != g_selectedcell->lcl->color)
            break;
        // do not add selected cell
        //if(cl_test == g_selectedcell)
        //continue;
        put_cell_to_boom_list(boomlist, &boom_count, cl_test);	// add to list
    }
    // For negative Y// add to list
    for (cli_y=cly-1, cli_x=clx; cli_y >= cl_y_nmax; --cli_y) {
        cl_test = &g_lgs_field.fld[clx][cli_y];
        // if there is no ball, then break
        if (!(cl_test->tag_state & LGS_FIELDCELL_STATE_OCCUPIED))
            break;
        //Stop if found cell with differently colored ball
        if (cl_test->color != g_selectedcell->lcl->color)
            break;
        // do not add selected cell
        //if(cl_test == g_selectedcell)
        //continue;
        put_cell_to_boom_list(boomlist, &boom_count, cl_test);	// add to list
    }
    // For positive Y
    for (cli_y=cly+1, cli_x=clx; cli_y <= cl_y_pmax; ++cli_y) {
        cl_test = &g_lgs_field.fld[clx][cli_y];
        // if there is no ball, then break
        if (!(cl_test->tag_state & LGS_FIELDCELL_STATE_OCCUPIED))
            break;
        //Stop if found cell with differently colored ball
        if (cl_test->color != g_selectedcell->lcl->color)
            break;
        // do not add selected cell
        //if(cl_test == g_selectedcell)
        //continue;
        put_cell_to_boom_list(boomlist, &boom_count, cl_test);	// add to list
    }

    // Now check if we reached quantity that exceedes minimum needed for BA-DA-BOOM!!!
    if (boom_count >= PLAY_BALLS_BOOM_MIN_Q) {
        printf("boom: %i balls.\n", boom_count);
        //g_game_balls_counter -= boom_count;
        //Now traverse boom list
        //CHEAT #1: to index boom list with boom_count var we need to decrease it by 1
        while (--boom_count >= 0) {
            lgs_set_cell_free(boomlist[boom_count]);
        }

        //if selected cell booms it's ball then no
        //need to leave it selected.
    }
}
// Game logic ends here

// Game graphics starts here
// This will prepare everything graphical
// for ball to appear on field
void gfx_spawn_ball(gfx_field_cell *cl) {
	// set ball gfx color
	switch (cl->lcl->color) {
		case PLAY_BALL_COLOR_RED:
			cl->r = 1.0f; cl->g = 0.0f; cl->b = 0.0f; cl->a = 1.0f;
		break;
		case PLAY_BALL_COLOR_GREEN:
			cl->r = 0.0f; cl->g = 1.0f; cl->b = 0.0f; cl->a = 1.0f;
		break;
		case PLAY_BALL_COLOR_BLUE:
			cl->r = 0.0f; cl->g = 0.0f; cl->b = 1.0f; cl->a = 1.0f;
		break;
	}
	
#if defined (VISUAL_DEBUG)
	cl->scorchmark = FALSE;
	cl->a_scorchmark = 0.0f;
#endif
}

/**
 * This method will move and update gfx contents of cells
 */
void gfx_move_cell(gfx_field_cell *cl_src, gfx_field_cell* cl_dst) {
	cl_dst->r = cl_src->r;
	cl_dst->g = cl_src->g;
	cl_dst->b = cl_src->b;
	cl_dst->a = cl_src->a;
}

// This will initialise whole game gfx sub-system
void init_game_gfx(void) {
    int cx, cy;
    gfx_field_cell *cl;

    // initialise field layer sizes and color
    g_gfx_field.x = 0;
    g_gfx_field.y = 0;
    g_gfx_field.width = g_gfx_field.x + PLAY_FIELD_SZ_X * PLAY_GFX_CELL_WIDTH;
    g_gfx_field.height = g_gfx_field.y + PLAY_FIELD_SZ_Y * PLAY_GFX_CELL_HEIGHT;
    g_gfx_field.r = 0.3f;
    g_gfx_field.g = 0.1f;
    g_gfx_field.b = 0.33f;
    g_gfx_field.a = 0.43f;

    // traverse gfx field cells and set their sizes and positions
    for (cy=0; cy < PLAY_FIELD_SZ_Y; ++cy) {
        for (cx=0; cx < PLAY_FIELD_SZ_X; ++cx) {
            cl = &g_gfx_field.cells[cx][cy];

            cl->width = PLAY_GFX_CELL_WIDTH;
            cl->height = PLAY_GFX_CELL_HEIGHT;
            cl->x = g_gfx_field.x + cx * PLAY_GFX_CELL_WIDTH;
            cl->y = g_gfx_field.y + cy * PLAY_GFX_CELL_HEIGHT;

            cl->lcl = &g_lgs_field.fld[cx][cy];
        }
    }
}

// This will re-draw whole game gfx field
void draw_field(void) {
    int cx, cy;
    gfx_field_cell *cl;

// 	glNewList(GLLIST_DRAW_FIELD, GL_COMPILE);
    glBegin(GL_QUADS);
    glColor4f(g_gfx_field.r, g_gfx_field.g, g_gfx_field.b, g_gfx_field.a);
    glVertex3f(g_gfx_field.x, g_gfx_field.y, 0.0f);
    glVertex3f(g_gfx_field.x+g_gfx_field.width, g_gfx_field.y, 0.0f);
    glVertex3f(g_gfx_field.x+g_gfx_field.width, g_gfx_field.y+g_gfx_field.height, 0.0f);
    glVertex3f(g_gfx_field.x, g_gfx_field.y+g_gfx_field.height, 0.0f);
    glEnd();
    for (cy=0; cy < PLAY_FIELD_SZ_Y; ++cy) {
        for (cx=0; cx < PLAY_FIELD_SZ_X; ++cx) {
            cl = &g_gfx_field.cells[cx][cy];
//             glBegin(GL_QUADS);
//             glColor4f(cl->r, cl->g, cl->b, cl->a);
//             glVertex3f(cl->x, cl->y, 0.1f);
//             glVertex3f(cl->x+cl->width, cl->y, 0.1f);
//             glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.1f);
//             glVertex3f(cl->x, cl->y+cl->height, 0.1f);
//             glEnd();
			
			// draw cell frame
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

			// draw ball
            if (cl->lcl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED) {
                glColor4f(cl->r, cl->g, cl->b, cl->a);
                glVertex3f(cl->x, cl->y, 0.5f);
                glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.5f);
                glVertex3f(cl->x+cl->width, cl->y, 0.5f);
                glVertex3f(cl->x, cl->y+cl->height, 0.5f);
            }

#if defined (VISUAL_DEBUG)
            // draw collected cell to debug algorithm work
            if (cl->lcl->tag_state & LGS_FIELDCELL_STATE_COLLECTED) {
                glColor4f(0.9f, 0.5f, 0.9f, 1.0f);
                glVertex3f(cl->x+cl->width*0.5f, cl->y, 0.7f);
                glVertex3f(cl->x+cl->width*0.5f, cl->y+cl->height, 0.7f);
                glVertex3f(cl->x, cl->y+cl->height*0.5f, 0.7f);
                glVertex3f(cl->x+cl->width, cl->y+cl->height*0.5f, 0.7f);
            }
            
            if (cl->scorchmark == TRUE) {
                glColor4f(0.8f, 0.8f, 0.0f, cl->a_scorchmark);
                glVertex3f(cl->x, cl->y, 0.4f);
                glVertex3f(cl->x+cl->width, cl->y, 0.4f);
                glVertex3f(cl->x+cl->width, cl->y, 0.4f);
                glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.4f);
                glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.4f);
                glVertex3f(cl->x, cl->y+cl->height, 0.4f);
                glVertex3f(cl->x, cl->y+cl->height, 0.4f);
                glVertex3f(cl->x, cl->y, 0.4f);
            }
#endif
            glEnd();
        }
    }

    //Draw selected cell
    if (g_selectedcell) {
        glColor4f(0.0f, 0.8f, 0.8f, 1.0f);
        glVertex3f(cl->x, cl->y, 0.55f);
        glVertex3f(cl->x+cl->width, cl->y, 0.55f);
        glVertex3f(cl->x+cl->width, cl->y, 0.55f);
        glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.55f);
        glVertex3f(cl->x+cl->width, cl->y+cl->height, 0.55f);
        glVertex3f(cl->x, cl->y+cl->height, 0.55f);
        glVertex3f(cl->x, cl->y+cl->height, 0.55f);
        glVertex3f(cl->x, cl->y, 0.55f);
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

// Game part starts here
/**
 * This method will initialise all game components
 */
void game_init(void) {
	//Initialise game logics
    init_logics();

    //Initialise game gfx
    init_game_gfx();
	
	int cx, cy;
    // traverse field init all game representations
    for (cy=0; cy<PLAY_FIELD_SZ_Y; ++cy) {
        for (cx=0; cx<PLAY_FIELD_SZ_X; ++cx) {
			// connect logical representation
			g_game_field.cells[cx][cy].lcl = &g_lgs_field.fld[cx][cy];
			// connect gfx representation
			g_game_field.cells[cx][cy].gcl = &g_gfx_field.cells[cx][cy];
			// set cell empty
            lgs_set_cell_free(g_game_field.cells[cx][cy].lcl);
        }
	}
	
	g_selectedcell = NULL;
	
	g_gamestate.state = 0x00;
	g_gamestate.level = 0x00;
	
	SET_FLAG(g_gamestate.state, GAME_STATE_RUNNING);
	SET_FLAG(g_gamestate.state, GAME_STATE_SPAWNBALLS);
}

/**
 * This function will determine wether player hit cell with a ball and selected it,
 * or hit emty cell and had moved ball from selected cell there.
 * 1. move selected ball onto cell
 * 2. check if player collected a row
 * 3. destroy a row if (2) is satisfied
 * 4. spawn new balls onto field
 */
void game_hit_play_field(GLfloat xpos, GLfloat ypos) {
    // logic coordinates of cell in the field that was hit by click
    int cl_logic_x, cl_logic_y;
	game_field_cell *cl;

    //check if click was on field
    if ((xpos < g_gfx_field.x) || (xpos > (g_gfx_field.x+g_gfx_field.width)))
        return;
    if ((ypos < g_gfx_field.y) || (ypos > (g_gfx_field.y+g_gfx_field.height)))
        return;

    // determine logic cell coordinates
    cl_logic_x = (int)(xpos*PLAY_GFX_TO_LOGIC_CL_X-g_gfx_field.x);
    cl_logic_y = (int)(ypos*PLAY_GFX_TO_LOGIC_CL_Y-g_gfx_field.y);
	
	cl = &g_game_field.cells[cl_logic_x][cl_logic_y];

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
	// Check cell if it has a ball
    if ((cl->lcl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED)) {
        g_selectedcell = cl;
    } else {
		if(g_selectedcell != NULL) {
			// Update gfx part accordingly
			gfx_move_cell(g_selectedcell->gcl, cl->gcl);
			// Update logics part accordingly
			lgs_move_ball(g_selectedcell->lcl, cl->lcl);
			// now destination cell becomes selected cell because we need
			// to calculate some logics with it.
			g_selectedcell = cl;

			//check nearby cells if we need to destroy some of them
			trace_and_destroy_recursive_collection(cl_logic_x, cl_logic_y);
			
			// spawn balls onto field on update
			SET_FLAG(g_gamestate.state, GAME_STATE_SPAWNBALLS);
			
			// unselect current cell
			g_selectedcell = NULL;
		}
    }
}

void game_set_cell_to_explode(game_field_cell *gcl) {
#if defined (VISUAL_DEBUG)
	// check tag to visualize collection
	gcl->gcl->scorchmark = TRUE;
	gcl->gcl->a_scorchmark = 1.0f;
#endif
}

void game_spawn_ball_on_field(game_field_cell *cl) {
	lgs_spawn_ball(cl->lcl);
	gfx_spawn_ball(cl->gcl);
}

/**
 * This method is simply top-level render call
 */
void game_render(void) {
	glClearColor(g_gameopts.clear_r, g_gameopts.clear_g, g_gameopts.clear_b, g_gameopts.clear_a);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    update_player_eye();

    // 	glCallList(GLLIST_DRAW_FIELD);
    draw_field();

    //draw all tools we need
    //draw_tools();

    glXSwapBuffers(g_dpy, g_wnd);
}

/**
 * This method will update main game gfx and logics
 */
void game_update(time_t tm) {
	int cx, cy;
    game_field_cell *cl;
	
	if(g_gamestate.state & GAME_STATE_RUNNING) {
	
		// Traverse cells and update their state
		for (cy=0; cy < PLAY_FIELD_SZ_Y; ++cy) {
			for (cx=0; cx < PLAY_FIELD_SZ_X; ++cx) {
				cl = &g_game_field.cells[cx][cy];
				
				// Check if cell will explode
				if(cl->lcl->tag_state & LGS_FIELDCELL_STATE_EXPLODING) {
#if defined (VISUAL_DEBUG)
					cl->gcl->scorchmark = TRUE;
					cl->gcl->a_scorchmark = 1.0f;
#endif
					lgs_explode_cell(cl->lcl);
				}
				
#if defined (VISUAL_DEBUG)
				// handle scorch marks
				if(cl->gcl->scorchmark == TRUE) {
					cl->gcl->a_scorchmark -= 0.01f;
					if(cl->gcl->a_scorchmark <= 0.01f) {
						cl->gcl->scorchmark = FALSE;
					}
				}
#endif
			}
		}
		
		if(g_gamestate.state & GAME_STATE_SPAWNBALLS) {
			int spwn_count;
			int tries = 0;
			game_field_cell *cl;
			for (spwn_count=0; spwn_count < PLAY_BALLS_SPAWN_Q;) {
				int cx=rand() % 20;
				int cy=rand() % 20;
				cl = &g_game_field.cells[cx][cy];
				++tries;
				// if cell is free
				if (!(cl->lcl->tag_state & LGS_FIELDCELL_STATE_OCCUPIED)) {
					// occupy cell and set a ball with a random ball
					lgs_spawn_ball(cl->lcl);
					gfx_spawn_ball(cl->gcl);
					++spwn_count;
				}
			}
			printf("spended (%i) tries.\n", tries);
			CLEAR_FLAG(g_gamestate.state, GAME_STATE_SPAWNBALLS);
		}
		
		// Check if we win
		if(g_gamestate.state & GAME_STATE_WIN) {
			// Check for epic WIN!!!
			if(g_gamestate.level == 0xFE) {
				printf("Congratulations !!! EPIC WIN :)\n");
			}
			g_gamestate.level += 0x01;
		}
	}
}
// Game part ends here

typedef struct {
    unsigned char run;
} rs_app_settings;

rs_app_settings g_app_opts;

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
    if (g_dpy == NULL) {
        fatal_err("can't open display :0");
    }
    // Find out if GLX is up and running on this X server
    if (!glXQueryExtension(g_dpy, &dummy, &dummy)) {
        fatal_err("this X server has no GLX extension supported.");
    }

    // Find an appropriate visual for us
    if (g_gameopts.dbufferred) {
        xvi = glXChooseVisual(g_dpy, DefaultScreen(g_dpy), g_dblbuff);
    }
    if (xvi == NULL) {
        fatal_err("failed to get appropriate visual.");
    }

    //Create glx context
    glx_ct = glXCreateContext(g_dpy, xvi, None, GL_TRUE);
    if (glx_ct == NULL) {
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
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    setup_projection();

	game_init();

    // CHEAT: code to set window size exactly as gfx field size
    g_gameopts.wnd_width = g_gfx_field.width;
    g_gameopts.wnd_height = g_gfx_field.height;
    XResizeWindow(g_dpy, g_wnd, g_gameopts.wnd_width, g_gameopts.wnd_height);
    setup_projection();

    g_app_opts.run = TRUE;

    // Now enter a main cycle of program
    while (g_app_opts.run) {
        do {
            XNextEvent(g_dpy, &event);

            switch (event.type) {
            case KeyPress:
            {
                KeySym keysym;
                XKeyEvent *key_event;
                char buffer[1];

                key_event = (XKeyEvent *)&event;
                if ( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
                        (keysym == (KeySym)XK_Escape) ) {

                    // not gentle way to quit the app !
                    //exit(0);
                    // so we will rather use app settings
                    g_app_opts.run = FALSE;
                }

                if ( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
                        (keysym == (KeySym)'w') ) {
                    g_playereye.z += 10.0f;
                }

                if ( (XLookupString(key_event, buffer, 1, &keysym, NULL) == 1) &&
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
                switch (event.xbutton.button) {
                case 1: {
                    // Means that player have selected a ball on field
                    // or selected empty cell on field where selected
                    // ball should move
                    game_hit_play_field(event.xmotion.x, event.xmotion.y);
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
        } while (XPending(g_dpy));
		
		// Update game
		game_update(time(NULL));

        // Redraw whole scene
        game_render();
    }

    return 0;
}
