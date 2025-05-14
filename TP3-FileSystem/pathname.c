
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "pathname.h"
#include "directory.h"
#include <string.h>      /* strtok, strncpy */

#define TMP_PATH_MAX 256   /* tamaño seguro para copiar la ruta */

/* Resuelve una ruta ABSOLUTA y devuelve su número de inodo, o -1 en error. */
int pathname_lookup(struct unixfilesystem *fs, const char *pathname)
{
    if (!pathname || pathname[0] != '/')
        return -1;                      /* solo absolutas */

    if (strcmp(pathname, "/") == 0)
        return 1;                       /* el root siempre es inodo 1 */

    char pathcopy[TMP_PATH_MAX];
    strncpy(pathcopy, pathname, sizeof(pathcopy));
    pathcopy[sizeof(pathcopy) - 1] = '\0';

    int cur_inumber = 1;                /* comenzamos en / */

    /* strtok salta el primer '/' con +1 */
    for (char *tok = strtok(pathcopy + 1, "/");
         tok != NULL;
         tok = strtok(NULL, "/"))
    {
        struct direntv6 de;
        if (directory_findname(fs, tok, cur_inumber, &de) < 0)
            return -1;                  /* componente no encontrado */

        cur_inumber = de.d_inumber;     /* avanzar al siguiente nivel */
    }

    return cur_inumber;                 /* éxito */
}

