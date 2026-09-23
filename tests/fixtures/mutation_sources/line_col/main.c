/* Source: src/task_ops.c (line_col_offset), verbatim.
   Oracle: outputs of the original function on boundary inputs. */
#include <stddef.h>

static long line_col_offset(const char *data, size_t len, long line, long col)
{
    long l = 1;
    size_t i = 0;
    while (i < len && l < line) {
        if (data[i] == '\n')
            l++;
        i++;
    }
    if (l != line || col < 1)
        return -1;
    size_t off = i + (size_t)(col - 1);
    return off <= len ? (long)off : -1;
}

int main(void)
{
    int bad = 0;
    bad += (line_col_offset("ab\ncd\n", 6, 1, 1)) != 0;
    bad += (line_col_offset("ab\ncd\n", 6, 1, 3)) != 2;
    bad += (line_col_offset("ab\ncd\n", 6, 2, 1)) != 3;
    bad += (line_col_offset("ab\ncd\n", 6, 2, 2)) != 4;
    bad += (line_col_offset("ab\ncd\n", 6, 3, 1)) != 6;
    bad += (line_col_offset("ab\ncd\n", 6, 4, 1)) != -1;
    bad += (line_col_offset("ab\ncd\n", 6, 2, 0)) != -1;
    bad += (line_col_offset("ab\ncd\n", 6, 3, 2)) != -1;
    bad += (line_col_offset("abc", 3, 1, 4)) != 3;
    bad += (line_col_offset("abc", 3, 1, 5)) != -1;
    bad += (line_col_offset("", 0, 1, 1)) != 0;
    return bad == 0 ? 0 : 1;
}
