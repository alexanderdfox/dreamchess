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

#include "ui_sdlgl.h"

static texture_t backdrop;
static texture_t border[9];
static texture_t menu_border[9];
static texture_t mouse_cursor;
static texture_t white_pieces[7];
static texture_t black_pieces[7];
static texture_t selector_tex;
static theme themes[25];
static int selected_theme=0;
static int theme_count=0;

static char white_name[80];
static char black_name[80];

void set_theme_count( int count )
{
    theme_count=count;
}

int get_theme_count()
{
    return theme_count;
}

int get_selected_theme()
{
    return selected_theme;
}

void set_selected_theme( int index )
{
    selected_theme=index;
}

texture_t *get_menu_border()
{
    return menu_border;
}

texture_t *get_border()
{
    return border;
}

theme *get_theme( int index )
{
    return &themes[index];
}

texture_t *get_mouse_cursor()
{
    return &mouse_cursor;
}

char *get_white_name()
{
    return white_name;
}

char *get_black_name()
{
    return black_name;
}

texture_t *get_backdrop()
{
    return &backdrop;
}

texture_t *get_white_pieces()
{
    return white_pieces;
}

texture_t *get_black_pieces()
{
    return black_pieces;
}

texture_t *get_white_piece( int index )
{
    return &white_pieces[index];
}

texture_t *get_black_piece( int index )
{
    return &black_pieces[index];
}

texture_t *get_selector_tex()
{
    return &selector_tex;
}

/* xml theme options */
int use_lighting()
{
    if (selected_theme==theme_count)
        return TRUE;
    else
        return themes[selected_theme].lighting;
}

int use_tex_spin()
{
    if (selected_theme==theme_count)
        return FALSE;
    else
        return themes[selected_theme].piece_tex_spin;
}

int get_tex_spin_speed()
{
    return themes[selected_theme].piece_tex_spin_speed;
}

int load_opaque(mxml_node_t *top, char *name, char *dest)
{
    mxml_node_t *node = mxmlFindElement(top, top, name, NULL, NULL, MXML_DESCEND);
    if (node)
    {
        node = mxmlWalkNext(node, node, MXML_DESCEND);
        if (node && node->type == MXML_OPAQUE)
        {
            strcpy(dest, node->value.opaque);
            return 0;
        }
    }
    return 1;
}

/** @brief Load the themes XML
 *
 */
void load_theme_xml( char *xmlfile )
{
    FILE *fp;
    mxml_node_t *tree=NULL, *theme=NULL;

    fp = fopen(xmlfile, "r");
    if (fp)
        tree = mxmlLoadFile(NULL, fp, MXML_OPAQUE_CALLBACK);
    else
        DBG_ERROR("error opening theme file" );

    fclose(fp);

    theme = tree;

    DBG_LOG("loading %s", xmlfile );
    while ((theme = mxmlFindElement(theme, tree, "theme", NULL, NULL, MXML_DESCEND)))
    {
        mxml_node_t *node, *node2;
        /* Set theme to defaults.. incase we have missing bits..*/
        sprintf( themes[theme_count].name, "Un named" );
        sprintf( themes[theme_count].style, "default" );
        sprintf( themes[theme_count].pieces, "classiclow" );
        sprintf( themes[theme_count].board, "classic" );
        sprintf( themes[theme_count].white_name, "White" );
        sprintf( themes[theme_count].black_name, "Black" );
        themes[theme_count].lighting=TRUE;
        themes[theme_count].piece_tex_spin=FALSE;
        themes[theme_count].piece_tex_spin_speed=0;
        themes[theme_count].selector_colour[0]=1.0;
        themes[theme_count].selector_colour[1]=1.0;
        themes[theme_count].selector_colour[2]=0.0;
        themes[theme_count].selector_colour[3]=0.25;  
        themes[theme_count].selector_spinspeed=0;
        themes[theme_count].selector_size=0.5;
        themes[theme_count].selector_bouncespeed=0;

        load_opaque(theme, "name", themes[theme_count].name);
        load_opaque(theme, "style", themes[theme_count].style);
        load_opaque(theme, "pieces", themes[theme_count].pieces);
        load_opaque(theme, "board", themes[theme_count].board);
        load_opaque(theme, "white_name", themes[theme_count].white_name);
        load_opaque(theme, "black_name", themes[theme_count].black_name);

        node = mxmlFindElement(theme, theme, "selector", NULL, NULL, MXML_DESCEND);
        if (node)
        {
            char *temp=(char*)mxmlElementGetAttr(node, "spinspeed");
            if ( temp )
                themes[theme_count].selector_spinspeed=atof(temp);

            temp=(char*)mxmlElementGetAttr(node, "size");
            if ( temp )
                themes[theme_count].selector_size=atof(temp);

            temp=(char*)mxmlElementGetAttr(node, "bouncespeed");
            if ( temp )
            {
                themes[theme_count].selector_bouncespeed=atof(temp);
            }

            node = mxmlWalkNext(node, node, MXML_DESCEND);
            node = mxmlFindElement(node, node, "colour", NULL, NULL, MXML_DESCEND);
            if (node)
            {
                node2 = mxmlWalkNext(node, node, MXML_DESCEND);
                node2 = mxmlFindElement(node2, node2, "red", NULL, NULL, MXML_DESCEND);  
                if (node2)
                {
                    node2 = mxmlWalkNext(node2, node2, MXML_DESCEND);
                    themes[theme_count].selector_colour[0]=atof(node2->value.opaque);
                }       

                node2 = mxmlWalkNext(node, node, MXML_DESCEND);
                node2 = mxmlFindElement(node2, node2, "green", NULL, NULL, MXML_DESCEND);  
                if (node2)
                {
                    node2 = mxmlWalkNext(node2, node2, MXML_DESCEND);
                    themes[theme_count].selector_colour[1]=atof(node2->value.opaque);
                }       

                node2 = mxmlWalkNext(node, node, MXML_DESCEND);
                node2 = mxmlFindElement(node2, node2, "blue", NULL, NULL, MXML_DESCEND);  
                if (node2)
                {
                    node2 = mxmlWalkNext(node2, node2, MXML_DESCEND);
                    themes[theme_count].selector_colour[2]=atof(node2->value.opaque);
                }        

                node2 = mxmlWalkNext(node, node, MXML_DESCEND);
                node2 = mxmlFindElement(node2, node2, "alpha", NULL, NULL, MXML_DESCEND);  
                if (node2)
                {
                    node2 = mxmlWalkNext(node2, node2, MXML_DESCEND);
                    themes[theme_count].selector_colour[3]=atof(node2->value.opaque);
                }               
            }

            //themes[theme_count].selector_colour[0]=atof(mxmlElementGetAttr(node, "red"));
            //themes[theme_count].selector_colour[1]=atof(mxmlElementGetAttr(node, "green"));
            //themes[theme_count].selector_colour[2]=atof(mxmlElementGetAttr(node, "blue"));
        }

        node = mxmlFindElement(theme, theme, "lighting", NULL, NULL, MXML_DESCEND);
        if (node)
        {
            node = mxmlWalkNext(node, node, MXML_DESCEND);

            if (node && node->type == MXML_OPAQUE)
                if ( !strcmp( node->value.opaque, "off" ) )
                    themes[theme_count].lighting=FALSE;
        }

        node = mxmlFindElement(theme, theme, "tex_spin", NULL, NULL, MXML_DESCEND);
        if (node)
        {
            node = mxmlWalkNext(node, node, MXML_DESCEND);

            themes[theme_count].piece_tex_spin=TRUE;
            themes[theme_count].piece_tex_spin_speed=atoi(node->value.opaque);
            /* printf( "Speed: %i\n", themes[theme_count].piece_tex_spin_speed );
            if (node && node->type == MXML_OPAQUE)
                if ( !strcmp( node->value.opaque, "off" ) )
                    themes[theme_count].lighting=FALSE; */
        }
        DBG_LOG("added theme: %s", themes[theme_count].name );

        theme_count++;
    }

    mxmlDelete(tree);
}

/** @brief Loads a style.
 *
 *  @param name The name of the subdirectory of the theme to load.
 */
void load_theme(char* style, char* pieces, char *board)
{
    ch_datadir();
    chdir("styles");
    chdir(style);

    /* Theme! */
    load_texture_png( &backdrop, "backdrop.png", 0, 1 );

    #ifndef _arch_dreamcast
    #ifndef __BEOS__
    load_texture_png( &mouse_cursor, "mouse_cursor.png", 1, 1 );
	#endif /* __BEOS__ */
    #endif /* _arch_dreamcast */

    load_border(border, "border.png");
    load_pieces();

    ch_datadir();
    chdir("pieces");
    chdir(pieces);
    loadmodels("set.cfg");
    texture_t seltex;
    load_texture_png(&selector_tex, "selector.png", 1,1);

    ch_datadir();
    chdir("boards");
    chdir(board);
    load_board("board.dcm", "board.png");

    ch_datadir();
}

/** @brief Frees all textures of the currently loaded theme. */
void unload_theme()
{
    glDeleteTextures(1, &white_pieces[GUI_PIECE_KING].id);
    glDeleteTextures(1, &backdrop.id);
    glDeleteTextures(1, &selector_tex.id);
    unload_border(border);
    freemodels();
}
