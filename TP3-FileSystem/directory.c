#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define DIR_NAME_MAX 14

int directory_findname(struct unixfilesystem *fs, const char *name,
                       int dirinumber, struct direntv6 *dirEnt)
{
    if (!name || name[0] == '\0') return -1;          /* nombre vacío */

    /* 1 ▪ Obtener el inodo del directorio */
    struct inode dirinode;
    if (inode_iget(fs, dirinumber, &dirinode) < 0) return -1;

    /* 2 ▪ Confirmar que realmente sea un directorio */
    if ((dirinode.i_mode & IFMT) != IFDIR) return -1;

    /* 3 ▪ Calcular cuántos bloques ocupa el directorio */
    int dirsize  = inode_getsize(&dirinode);
    int nblocks  = (dirsize + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
    char block[DISKIMG_SECTOR_SIZE];

    /* 4 ▪ Recorremos bloque por bloque las entradas direntv6 */
    for (int b = 0; b < nblocks; b++) {
        int nbytes = file_getblock(fs, dirinumber, b, block);
        if (nbytes < 0) return -1;

        int nentries = nbytes / sizeof(struct direntv6);
        struct direntv6 *d = (struct direntv6 *)block;

        for (int i = 0; i < nentries; i++, d++) {
            if (d->d_name[0] == 0) continue;                     /* entrada borrada */
            if (strncmp(d->d_name, name, DIR_NAME_MAX) == 0) {   /* match exacto (14 bytes máx) */
                if (dirEnt) *dirEnt = *d;                        /* copiar resultado */
                return 0;                                        /* éxito */
            }
        }
    }
    return -1;   /* nombre no encontrado en este directorio */
}

