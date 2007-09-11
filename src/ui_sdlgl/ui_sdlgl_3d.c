/*  DreamChess
**
**  DreamChess is the legal property of its developers, whose names are too
**  numerous to list here. Please refer to the COPYRIGHT file distributed
**  with this source distribution.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef WITH_UI_SDLGL

#include <stdio.h>
#include <stdlib.h>
#include "debug.h"

/* Define our booleans */
#define TRUE  1
#define FALSE 0

#ifdef _arch_dreamcast
#include <dc/fmath.h>
#define sin fsin
#define cos fcos
#define sqrt fsqrt
#define magnitude_sqr(X, Y, Z) fipr_magnitude_sqr(X, Y, Z, 0)
#define dot_product(X, Y, Z, XX, YY, ZZ) fipr(X, Y, Z, 0, XX, YY, ZZ, 0)
#else
#include <math.h>
#define magnitude_sqr in_product_self
#define dot_product in_product
#endif

#include <SDL.h>
#include <SDL_opengl.h>

#include "board.h"
#include "ui_sdlgl.h"
#include "ui_sdlgl_3d.h"

typedef struct coord3
{
    float x;
    float y;
    float z;
}
coord3_t;

typedef struct coord3d
{
    GLdouble x;
    GLdouble y;
    GLdouble z;
}
coord3d_t;

typedef enum primitive_type
{
    PRIM_TRIANGLES,
    PRIM_STRIP
} primitive_type_t;

typedef struct group
{
    primitive_type_t type;
    int len;
    unsigned int *data;
}
group_t;

typedef struct bone
{
    char *name;
    float offset[3];
    int children;
    int *child;
}
bone_t;

typedef struct mesh
{
    int has_bones;
    float *vertex;
    float *normal;
    float *tex_coord;
    int *bone_w;
    int groups;
    group_t *group;
    bone_t *bone;
}
mesh_t;

typedef struct model
{
    mesh_t *mesh;
    texture_t *texture;
}
model_t;



typedef struct data_col_item
{
    char *name;
    void *data;
}
data_col_item_t;

typedef struct
{
    data_col_item_t *item;
    int size;
}
data_col_t;

static void data_col_init(data_col_t *data_col)
{
    data_col->item = NULL;
    data_col->size = 0;
}

static void data_col_free(data_col_t *data_col, void (* free_data) (void *))
{
    int i;
    for (i = 0; i < data_col->size; i++)
    {
        free(data_col->item[i].name);
        free_data(data_col->item[i].data);
    }

    free(data_col->item);
}

static void *data_col_find(data_col_t *data_col, char *name)
{
    int i = 0;

    for (i = 0; i < data_col->size; i++)
        if (!strcmp(data_col->item[i].name, name))
            return data_col->item[i].data;

    return NULL;
}

static void data_col_add(data_col_t *data_col, char *name, void *data)
{
    data_col->item = realloc(data_col->item, (data_col->size + 1)
                             * sizeof(data_col_item_t));

    data_col->item[data_col->size].name = strdup(name);
    data_col->item[data_col->size++].data = data;
}

static data_col_t textures;
static data_col_t meshes;

texture_t ground;

#define BUF_SIZE 256
#define FN_LEN 256

static inline float in_product_self(float x, float y, float z)
{
    return x * x + y * y + z * z;
}

static inline float in_product(float x, float y, float z, float xx, float yy,
                               float zz)
{
    return x * xx + y * yy + z * zz;
}

static model_t model[13];
static model_t board;

static int is_2d;

static int selector, selected;
static float x_rotation, z_rotation;

int piece_moving_done=1;
int piece_moving_start;
int piece_moving_dest;
int piece_moving_source;
int piece_moving_x_done;
int piece_moving_y_done;
float piece_moving_source_xpos;
float piece_moving_source_ypos;
float piece_moving_dest_xpos;
float piece_moving_dest_ypos;
float piece_moving_xpos;
float piece_moving_ypos;

int get_piece_moving_done()
{
    return piece_moving_done;
}

void start_piece_move( int source, int dest )
{
    piece_moving_start=SDL_GetTicks();

    piece_moving_done=0;

    piece_moving_dest=dest;
    piece_moving_source=source;

    piece_moving_source_xpos=(float)(source%8);
    piece_moving_source_ypos=(float)(source/8);

    piece_moving_dest_xpos=(float)(dest%8);
    piece_moving_dest_ypos=(float)(dest/8);

    piece_moving_xpos=piece_moving_source_xpos;
    piece_moving_ypos=piece_moving_source_ypos;

    piece_moving_x_done=0;
    piece_moving_y_done=0;
}

static texture_t *load_piece_texture(char *filename)
{
    texture_t *tex = data_col_find(&textures, filename);

    if (tex)
    {
        return tex;
    }

    tex = malloc(sizeof(texture_t));

    DBG_LOG("loading texture: %s", filename);
    load_texture_png(tex, filename, 1, 0);
    data_col_add(&textures, filename, tex);
    return tex;
}

mesh_t *dcm_load(char *filename)
{
    FILE *f;
    mesh_t *mesh;
    int version;
    char id[3];
    int vertices;
    int i;

    f = fopen(filename, "r");

    if (!f)
    {
        DBG_ERROR("couldn't open %s", filename);
        return NULL;
    }

    if ((fscanf(f, "%c%c%c %d\n", &id[0], &id[1], &id[2], &version) != 4)
            || ((id[0] != 'D') || (id[1] != 'C') || (id[2] != 'M')))
    {
        DBG_ERROR("invalid DCM file header");
        return NULL;
    }

    if (version != 100)
    {
        DBG_ERROR( "DCM version %i not supported", version);
        return NULL;
    }

    if (fscanf(f, "%d\n", &vertices) != 1)
    {
        DBG_ERROR("error reading DCM file");
        return NULL;
    }

    mesh = malloc(sizeof(mesh_t));

    mesh->has_bones = 0;
    mesh->vertex = malloc(sizeof(float) * vertices * 3);

    for (i = 0; i < vertices * 3; i++)
    {
        if (fscanf(f, "%f\n", &mesh->vertex[i]) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }
    }

    mesh->normal = malloc(sizeof(float) * vertices * 3);

    for (i = 0; i < vertices * 3; i++)
    {
        if (fscanf(f, "%f\n", &mesh->normal[i]) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }
    }

    mesh->tex_coord = malloc(sizeof(float) * vertices * 2);

    for (i = 0; i < vertices * 2; i++)
    {
        if (fscanf(f, "%f\n", &mesh->tex_coord[i]) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }
    }

    /* As we don't flip our images we flip our u coordinates instead. */
    for (i = 1; i < vertices * 2; i += 2)
        mesh->tex_coord[i] = 1.0f - mesh->tex_coord[i];

    if (fscanf(f, "%d\n", &mesh->groups) != 1)
    {
        DBG_ERROR("error reading DCM file");
        exit(1);
    }

    mesh->group = malloc(sizeof(group_t) * mesh->groups);

    for (i = 0; i < mesh->groups; i++)
    {
        char line[11];
        int group_len;
        int j;

        fgets(line, 11, f);

        if (!strcmp(line, "STRIP\n"))
            mesh->group[i].type = PRIM_STRIP;
        else if (!strcmp(line, "TRIANGLES\n"))
            mesh->group[i].type = PRIM_TRIANGLES;
        else
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        if (fscanf(f, "%d\n", &group_len) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        mesh->group[i].len = group_len;

        mesh->group[i].data = malloc(sizeof(unsigned int) * group_len);

        for (j = 0; j < group_len; j++)
        {
            if (fscanf(f, "%u\n", &mesh->group[i].data[j]) != 1)
            {
                DBG_ERROR("error reading DCM file");
                exit(1);
            }
        }
    }

    fclose(f);

    return mesh;
}

mesh_t *dcm_load_new(char *filename)
{
    FILE *f;
    mesh_t *mesh;
    int version;
    char id[3];
    int vertices;
    int bones;
    int i;

    f = fopen(filename, "r");

    if (!f)
    {
        DBG_ERROR("couldn't open %s", filename);
        return NULL;
    }

    if ((fscanf(f, "%c%c%c %d\n", &id[0], &id[1], &id[2], &version) != 4)
            || ((id[0] != 'D') || (id[1] != 'C') || (id[2] != 'M')))
    {
        DBG_ERROR("invalid DCM file header");
        return NULL;
    }

    if (version != 1)
    {
        DBG_ERROR( "DCM version %i not supported", version);
        return NULL;
    }

    if (fscanf(f, "VERTICES %d\n", &vertices) != 1)
    {
        DBG_ERROR("error reading DCM file");
        return NULL;
    }

    mesh = malloc(sizeof(mesh_t));
    mesh->has_bones = 1;

    mesh->vertex = malloc(sizeof(float) * vertices * 3);
    mesh->normal = malloc(sizeof(float) * vertices * 3);

    for (i = 0; i < vertices; i++)
    {
        if (fscanf(f, "%f %f %f %f %f %f\n", &mesh->vertex[i * 3], &mesh->vertex[i * 3 + 1], &mesh->vertex[i * 3 + 2],
            &mesh->normal[i * 3], &mesh->normal[i * 3 + 1], &mesh->normal[i * 3 + 2]) != 6)
        {
            DBG_ERROR("error reading DCM file: %i of %i", i, vertices);
            exit(1);
        }
    }

    mesh->bone_w = malloc(sizeof(int) * vertices);
    memset(mesh->bone_w, -1, sizeof(int) * vertices);

    mesh->tex_coord = malloc(sizeof(float) * vertices * 2);

    for (i = 0; i < vertices * 2; i++)
        mesh->tex_coord[i] = 0;

    /* As we don't flip our images we flip our u coordinates instead. */
    for (i = 1; i < vertices * 2; i += 2)
        mesh->tex_coord[i] = 1.0f - mesh->tex_coord[i];

    mesh->groups = 1;
    mesh->group = malloc(sizeof(group_t) * mesh->groups);

    for (i = 0; i < mesh->groups; i++)
    {
        char line[11];
        int group_len;
        float fl;
        int j;

        mesh->group[i].type = PRIM_TRIANGLES;

        if (fscanf(f, "TRIANGLES %d\n", &group_len) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        mesh->group[i].len = group_len * 3;

        mesh->group[i].data = malloc(sizeof(unsigned int) * group_len * 3);

        for (j = 0; j < group_len; j++)
        {
            if (fscanf(f, "%u %u %u %f %f %f %f %f %f\n", &mesh->group[i].data[j * 3], &mesh->group[i].data[j * 3 + 1],
                &mesh->group[i].data[j * 3 + 2], &fl, &fl, &fl, &fl, &fl, &fl) != 9)
            {
                DBG_ERROR("error reading DCM file");
                exit(1);
            }
        }
    }

    if (fscanf(f, "BONES %d\n", &bones) != 1)
    {
        DBG_ERROR("error reading DCM file");
        return NULL;
    }

    mesh->bone = malloc(sizeof(bone_t) * bones);

    for (i = 0; i < bones; i++)
    {
        int j;
        int vw;

        mesh->bone[i].name = malloc(256);
        if (fscanf(f, "BONE %255s\n", mesh->bone[i].name) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        if (fscanf(f, "%f %f %f\n", &mesh->bone[i].offset[0], &mesh->bone[i].offset[1],
            &mesh->bone[i].offset[2]) != 3)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        if (fscanf(f, "CHILDREN %d\n", &mesh->bone[i].children) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        if (mesh->bone[i].children)
            mesh->bone[i].child = malloc(sizeof(int) * mesh->bone[i].children);

        for (j = 0; j < mesh->bone[i].children; j++)
        {
            if (fscanf(f, "%d\n", &mesh->bone[i].child[j]) != 1)
            {
                DBG_ERROR("error reading DCM file");
                exit(1);
            }
        }

        if (fscanf(f, "VERTEXWEIGHTS %d\n", &vw) != 1)
        {
            DBG_ERROR("error reading DCM file");
            exit(1);
        }

        for (j = 0; j < vw; j++)
        {
            float weight;
            int vert_idx;
            if (fscanf(f, "%i %f\n", &vert_idx, &weight) != 2)
            {
                DBG_ERROR("error reading DCM file");
                exit(1);
            }
            mesh->bone_w[vert_idx] = i;
        }
    }

    fclose(f);

    return mesh;
}

static mesh_t *load_mesh(char *filename)
{
    mesh_t *mesh = data_col_find(&meshes, filename);

    if (mesh)
    {
        return mesh;
    }

    DBG_LOG("loading mesh: %s", filename);
    mesh = dcm_load(filename);
    data_col_add(&meshes, filename, mesh);
    return mesh;
}

static mesh_t *load_mesh_new(char *filename)
{
    mesh_t *mesh = data_col_find(&meshes, filename);

    if (mesh)
    {
        return mesh;
    }

    DBG_LOG("loading mesh: %s", filename);
    mesh = dcm_load_new(filename);
    data_col_add(&meshes, filename, mesh);
    return mesh;
}

coord3_t light = {0.0f, 0.0f, -1.0f};

#define DC_PI 3.14159265358979323846

inline float arccos(float f)
{
    return (2.193376378 + (-2.987042783 + (.5314426631 + .2990387380 * f)
                           * f) * f) / (1.396346817 + (-1.012703522 + (-.3056194995
                                                       + .1383216735 * f) * f) * f);
}

void model_render(model_t *model, float alpha, coord3_t *light, char tex_spin )
{
    mesh_t *mesh = model->mesh;
    int g;
    texture_t *texture = model->texture;
    int ticks = SDL_GetTicks();
    float tex_spin_pos=0.0f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture->id);

glPushMatrix();
glLoadIdentity();
glTranslatef(0, -0.5f, -12.0f );

// Somewhere in the initialization part of your program…
glEnable(GL_LIGHTING);
glEnable(GL_LIGHT0);

// Create light components
GLfloat ambientLight[] = { 0.0f, 0.0f, 0.0f, 1.0f };
GLfloat diffuseLight[] = { 0.5f, 0.5f, 0.5, 1.0f };
GLfloat specularLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat position[] = { 10.0f, -10.0f, 10.0f, 1.0f };

// Assign created components to GL_LIGHT0
glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
glLightfv(GL_LIGHT0, GL_POSITION, position);

float mcolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mcolor);

float specReflection[] = { 1.0f, 1.0f, 1.0f, 1.0f };
glMaterialfv(GL_FRONT, GL_SPECULAR, specReflection);
glMateriali(GL_FRONT, GL_SHININESS, 128);

glPopMatrix();

    if ( tex_spin )
        tex_spin_pos=(float)ticks / (float)(1000 * (1000/(float)get_tex_spin_speed()));

light = NULL;

    for (g = 0; g < mesh->groups; g++)
    {
        int i;

        switch (mesh->group[g].type)
        {
        case PRIM_TRIANGLES:
            glBegin(GL_TRIANGLES);
            break;
        case PRIM_STRIP:
            glBegin(GL_TRIANGLE_STRIP);
        }

        for (i = 0; i < mesh->group[g].len; i++)
        {
            unsigned int *data = mesh->group[g].data;
            float angle = 1.0f;

            if (light && use_lighting())
            {
                angle = arccos(dot_product(mesh->normal[data[i] * 3],
                                           mesh->normal[data[i] * 3 + 1],
                                           mesh->normal[data[i] * 3 + 2],
                                           light->x, light->y, light->z));

                angle /= 2.8;

                if (angle < 0.5f)
                    angle = 0.25f;

                else
                {
                    angle -= 0.5f;
                    angle *= 1.5;
                    angle += 0.25f;
                }
            }

            if (mesh->has_bones && (mesh->bone_w[data[i]] == 1))
            glColor4f(0, 0xff, 0, 1);
            else
            glColor4f(angle, angle, angle, alpha);

            glTexCoord2f(mesh->tex_coord[data[i] * 2] * texture->u2+tex_spin_pos,
                         mesh->tex_coord[data[i] * 2 + 1] * texture->v2);

            glNormal3f(mesh->normal[data[i] * 3],
                       mesh->normal[data[i] * 3 + 1],
                       mesh->normal[data[i] * 3 + 2]);

            glVertex3f(mesh->vertex[data[i] * 3],
                       mesh->vertex[data[i] * 3 + 1],
                       mesh->vertex[data[i] * 3 + 2]);
        }

        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
}

void loadmodels(char *filename)
{
    int i;
    FILE *f;
    char mesh[256];
    char texture[256];

    data_col_init(&meshes);
    data_col_init(&textures);

    f = fopen(filename, "r");

    if (!f)
    {
        DBG_ERROR("couldn't open %s\n", filename);
        exit(-1);
    }

    for (i = 0; i < 12; i++)
    {
        if (!fgets(mesh, 256, f) || (mesh[strlen(mesh) - 1] != '\n')
                || !fgets(texture, 256, f)
                || (texture[strlen(texture) - 1] != '\n'))
        {
            DBG_ERROR("error reading set file\n");
            exit(1);
        }

        mesh[strlen(mesh) - 1] = '\0';
        texture[strlen(texture) - 1] = '\0';

        model[i].mesh = load_mesh(mesh);
        model[i].texture = load_piece_texture(texture);
    }
/*
    model[12].mesh = load_mesh_new("/home/walter/devel/ginger/ginger.dcm");
    model[12].texture = load_piece_texture(texture);
    model[11].mesh = model[12].mesh;
*/
    if ((model[0].mesh->groups) == 1 && (model[0].mesh->group[0].len == 4))
        is_2d = 1;
    else
        is_2d = 0;

    fclose(f);
}

void load_board(char *dcm_name, char *texture_name)
{
    board.mesh = load_mesh(dcm_name);
    board.texture = load_piece_texture(texture_name);
}

void free_mesh(void *data)
{
    mesh_t *mesh = data;
    int i;

    free(mesh->vertex);
    free(mesh->normal);
    free(mesh->tex_coord);

    for (i = 0; i < mesh->groups; i++)
        free(mesh->group[i].data);

    free(mesh->group);
    free(mesh);
}

void free_texture(void *data)
{
    texture_t *tex = data;

    glDeleteTextures(1, &tex->id);
    free(tex);
}

void freemodels()
{
    data_col_free(&meshes, free_mesh);
    data_col_free(&textures, free_texture);
}

/* How many squares per second? */
#define PIECE_MOVE_SPEED 3.0

extern float piece_moving_source_xpos;
extern float piece_moving_source_ypos;
extern float piece_moving_dest_xpos;
extern float piece_moving_dest_ypos;
extern float piece_moving_xpos;
extern float piece_moving_ypos;
extern int piece_moving_dest;
extern int piece_moving_source;
extern int piece_moving_start;
extern int piece_moving_x_done;
extern int piece_moving_y_done;
extern int piece_moving_done;

static void draw_pieces(board_t *board, float rot_x, float rot_z, int flip)
{
    int i,j,k;
    float moved=0;
    coord3_t light_inv;

    light_inv.x = -light.x;
    light_inv.y = -light.y;
    light_inv.z = light.z;

    moved=(float)((SDL_GetTicks()-piece_moving_start)/(1000/PIECE_MOVE_SPEED));

    /* Draw the pieces.. */
    for (i = 7; i >= 0; i--)
        for (j = 0; j < 8; j++)
        {
            if ((k = board->square[(0/*flip_board*/ ? 63 - (i*8+j) : i*8+j)]) != NONE)
            {
                coord3_t *l = &light;
                glLoadIdentity();
                glTranslatef(0, -0.5f, -12.0f );
                glRotatef(rot_x, 1, 0, 0);
                glRotatef(rot_z, 0, 0, 1);

                if ( (i*8+j) == piece_moving_dest )
                {
                    if ( piece_moving_dest_xpos > piece_moving_source_xpos )
                    {
                        piece_moving_xpos = piece_moving_source_xpos + moved;
                        if ( piece_moving_xpos >= piece_moving_dest_xpos )
                            piece_moving_xpos = piece_moving_dest_xpos;
                    }
                    else if ( piece_moving_dest_xpos < piece_moving_source_xpos )
                    {
                        piece_moving_xpos = piece_moving_source_xpos - moved;
                        if ( piece_moving_xpos <= piece_moving_dest_xpos )
                            piece_moving_xpos = piece_moving_dest_xpos;
                    }

                    if ( piece_moving_dest_ypos > piece_moving_source_ypos )
                    {
                        piece_moving_ypos = piece_moving_source_ypos + moved;
                        if ( piece_moving_ypos >= piece_moving_dest_ypos )
                            piece_moving_ypos = piece_moving_dest_ypos;
                    }
                    else if ( piece_moving_dest_ypos < piece_moving_source_ypos )
                    {
                        piece_moving_ypos = piece_moving_source_ypos - moved;
                        if ( piece_moving_ypos <= piece_moving_dest_ypos )
                            piece_moving_ypos = piece_moving_dest_ypos;
                    }

                    if ( piece_moving_xpos == piece_moving_dest_xpos &&
                            piece_moving_ypos == piece_moving_dest_ypos )
                    {
                        piece_moving_done = 1;
                    }
                    if (is_2d)
                        glTranslatef(-3.5f + piece_moving_xpos, -3.5f +
                                     piece_moving_ypos, 0.04);
                    else
                        glTranslatef(-3.5f + piece_moving_xpos, -3.5f +
                                     piece_moving_ypos, 0.02);
                }
                else
                    glTranslatef(-3.5f + j, -3.5f + i, 0.02);

                if (is_2d)
                {
                    int steps = (z_rotation + 45.0f) / 90.0f;

                    glRotatef(-steps * 90.0f, 0, 0, 1);
                }
                else if (k < 12 && IS_BLACK(k))
                {
                    glRotatef(180, 0, 0, 1);
                    l = &light_inv;
                }

                if (!is_2d && flip) {
                    glScalef(1.0f, 1.0f, -1.0f);
                }

                model_render(&model[k], (i * 8 + j == selected ? 0.5f : 1.0f), l, use_tex_spin());
            }
        }
}

static void draw_board(float rot_x, float rot_z, int blend)
{
    coord3_t fixed = {0, 0, -1};
    glLoadIdentity();
    glTranslatef(0, -0.5f, -12.0f );
    glRotatef(rot_x, 1, 0, 0);
    glRotatef(rot_z, 0, 0, 1);

    if (blend) {
        model_render(&board, 0.8f, &fixed, FALSE);
    }
    else
        model_render(&board, 1.0f, &fixed, FALSE);
}

float selector_rotation=0.0;
float selector_bounce=0.0;
float bounce_inc=0.0;
void draw_selector()
{
    float r,g,b,a;
    float width,width2;
    float height;

    theme *t=get_theme(get_selected_theme());
    r=t->selector_colour[0];
    g=t->selector_colour[1];
    b=t->selector_colour[2];
    a=t->selector_colour[3];

    //r=1.0f; g=1.0f; b=0.0f; a=0.25f; /* a=((((SDL_GetTicks() % (1000 / 1)) / (float) (1000 / 1)))/4)+0.1; */
    width=t->selector_size;
    height=0.1;

    if ( t->selector_spinspeed == 0 )
    {
        selector_rotation=0;
    }

    glLoadIdentity();
    glTranslatef(0, -0.5, -12.0);
    glRotatef(x_rotation, 1, 0, 0);
    glRotatef(z_rotation, 0, 0, 1);
    glTranslatef(-3.5 + selector % 8, -3.5 + selector / 8, 0.01f);
    glRotatef(selector_rotation,0,0,1);
    selector_rotation+=t->selector_spinspeed;       

    if ( t->selector_bouncespeed == 0.0 )
        selector_bounce=0;
    else if ( selector_bounce == 0 )
        bounce_inc=t->selector_bouncespeed;
    
    if ( selector_bounce > 0.25 || selector_bounce < 0.0)
        bounce_inc=-(bounce_inc);

    selector_bounce+=bounce_inc; 

    glColor4f(0.0, 0.0, 0.0, 1.0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, get_selector_tex()->id);

    width2=width;
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);
    glVertex3f(-width2, width2, height);
    glTexCoord2f(1,0);
    glVertex3f(width2, width2, height);
    glTexCoord2f(1,1);
    glVertex3f(width2, -width2, height);
    glTexCoord2f(0,1);
    glVertex3f(-width2, -width2, height);
    glEnd();

    glTranslatef(0, 0, selector_bounce+0.01);

    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);
    glVertex3f(-width, width, height);
    glTexCoord2f(1,0);
    glVertex3f(width, width, height);
    glTexCoord2f(1,1);
    glVertex3f(width, -width, height);
    glTexCoord2f(0,1);
    glVertex3f(-width, -width, height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

#ifdef _arch_dreamcast
int find_square(int x, int y, float fd)
{
    return -1;
}
#else
int find_square(int x, int y, float fd)
{
    coord3d_t obj;
    coord3_t win;
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];

    glLoadIdentity();
    glTranslatef(0, -0.5f, -12.0f );
    glRotatef(x_rotation, 1, 0, 0);
    glRotatef(z_rotation, 0, 0, 1);

    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    win.x = x;
    win.y = viewport[3] - y;

    glReadPixels(win.x, win.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &win.z);
    win.z /= fd;
    gluUnProject(win.x, win.y, win.z, modelview, projection, viewport, &obj.x,
                 &obj.y, &obj.z);

    if (obj.x < -4 || obj.x > 4 || obj.y < -4 || obj.y > 4)
        /* TODO Check z? */
        return -1;

    return (floor(obj.y) + 4) * 8 + floor(obj.x) + 4;
}
#endif

static void draw_board_center()
{
    float tc = 46 / 512.0f;

    glLoadIdentity();
    glTranslatef(0, -0.5f, -12.0f );
    glRotatef(x_rotation, 1, 0, 0);
    glRotatef(z_rotation, 0, 0, 1);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, board.texture->id);

    glBegin(GL_QUADS);
    glColor4f(1, 1, 1, 0.75f);
    glTexCoord2f(tc, tc);
    glVertex3f(-4, -4, 0);
    glTexCoord2f(1 - tc, tc);
    glVertex3f(4, -4, 0);
    glTexCoord2f(1 - tc, 1 - tc);
    glVertex3f(4, 4, 0);
    glTexCoord2f(tc, 1 - tc);
    glVertex3f(-4, 4, 0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

static void setup_stencil()
{
    float tc = 46 / 512.0f;

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glEnable(GL_STENCIL_TEST);

    glLoadIdentity();
    glTranslatef(0, -0.5f, -12.0f );
    glRotatef(x_rotation, 1, 0, 0);
    glRotatef(z_rotation, 0, 0, 1);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, board.texture->id);

    glBegin(GL_QUADS);
    glColor4f(0, 0, 0, 1);
    glTexCoord2f(tc, tc);
    glVertex3f(-4, -4, 0);
    glTexCoord2f(1 - tc, tc);
    glVertex3f(4, -4, 0);
    glTexCoord2f(1 - tc, 1 - tc);
    glVertex3f(4, 4, 0);
    glTexCoord2f(tc, 1 - tc);
    glVertex3f(-4, 4, 0);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glStencilFunc(GL_EQUAL, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask(GL_TRUE);
}

void render_scene_3d(board_t *board, int reflections)
{
    glEnable(GL_CULL_FACE);
    if (reflections) {
        setup_stencil();
        glCullFace(GL_FRONT);
        draw_pieces(board, x_rotation, z_rotation, 1);
        glDisable(GL_STENCIL_TEST);
        glCullFace(GL_BACK);
        draw_board_center();
        draw_board(x_rotation, z_rotation, 0);
        draw_pieces(board, x_rotation, z_rotation, 0);
    } else {
        draw_board(x_rotation, z_rotation, 0);
        draw_pieces(board, x_rotation, z_rotation, 0);
    }
    glDisable(GL_CULL_FACE);
    draw_selector();
}

static void update_light()
{
    float len;

    light.x = sin(z_rotation * DC_PI / 180.0f) * (1.0f - cos(x_rotation * DC_PI / 180.0f));
    light.y = -sin(x_rotation * DC_PI / 180.0f) * cos(z_rotation * DC_PI / 180.0f);
    light.z = -cos(x_rotation * DC_PI / 180.0f);

    len = sqrt(magnitude_sqr(light.x, light.y, light.z));

    light.x /= len;
    light.y /= len;
    light.z /= len;
}

void move_camera(float x, float z)
{
    x_rotation -= x;
    if (x_rotation > 0.0f)
        x_rotation = 0.0f;

    if (x_rotation < -90.0f)
        x_rotation = -90.0f;

    z_rotation -= z;
    if (z_rotation >= 360.0f)
        z_rotation -= 360.0f;

    if (z_rotation <= 0.0f)
        z_rotation = 360.0f;

    update_light();
}

void move_selector(int direction)
{
    int steps = (z_rotation + 45.0f) / 90.0f;

    while (steps--)
    {
        switch (direction)
        {
        case SELECTOR_UP:
            direction = SELECTOR_RIGHT;
            break;
        case SELECTOR_RIGHT:
            direction = SELECTOR_DOWN;
            break;
        case SELECTOR_DOWN:
            direction = SELECTOR_LEFT;
            break;
        case SELECTOR_LEFT:
            direction = SELECTOR_UP;
        }
    }

    switch(direction)
    {
    case SELECTOR_UP:
        if (selector < 56)
            selector += 8;
        break;
    case SELECTOR_RIGHT:
        if (selector % 8 < 7)
            selector++;
        break;
    case SELECTOR_DOWN:
        if (selector > 7)
            selector -= 8;
        break;
    case SELECTOR_LEFT:
        if (selector % 8 > 0)
            selector--;
    }
}

int get_selector()
{
    return selector;
}

void select_piece(int square)
{
    selected = square;

    if ( square>=0 )
        selector = square;
}

void reset_3d()
{
    selected = -1;
    selector = 0;
    if (is_2d)
    {
        x_rotation = -10.0f;
        z_rotation = 0.0f;
    }
    else
    {
        x_rotation = -45.0f;
        z_rotation = 0.0f;
    }
    update_light();
}

#endif /* WITH_UI_SDLGL */
