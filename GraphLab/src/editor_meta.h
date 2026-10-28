#ifndef GL_EDITOR_META_H
#define GL_EDITOR_META_H
#include "expr.h"

/* Ephemeral source-position metadata, not part of a saved formula. Existing
   structural delimiters retain their owners while the user edits inside them. */
typedef struct {
    int16_t mate[EXPR_MAX];
    int16_t comma_owner[EXPR_MAX];
} EditorMeta;
void editor_meta_init(EditorMeta *m,const char *text);
/* Call before applying each insertion/deletion to the text. */
void editor_meta_insert(EditorMeta *m,const char *text,int at,const char *inserted);
void editor_meta_delete(EditorMeta *m,int old_len,int start,int end);
/* The nearest lexically unclosed '(' before the caret, or -1. */
int editor_meta_open_before(const char *text,int cursor);
/* Reuse an already-present delimiter only when it belongs to that opening. */
int editor_meta_skip(const EditorMeta *m,const char *text,int cursor,char delimiter);
/* Refuse to cross an outer template's closer while an inner manual '(' is
   still open; otherwise a later strict parse would change the grouping. */
int editor_meta_can_cross(const EditorMeta *m,const char *text,int from,int to);
#endif
