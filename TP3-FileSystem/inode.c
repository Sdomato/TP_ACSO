#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "inode.h"
#include "diskimg.h"

#define INODE_DIRECT_PTR_COUNT 8

/**
 * TODO
 */
int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
     // Verificación básica: número de inodo válido
    if (inumber < 1) {
        return -1; // número de inodo inválido
    }

    // Cálculos básicos
    int inodes_per_sector = DISKIMG_SECTOR_SIZE / sizeof(struct inode);
    int sector_number = INODE_START_SECTOR + (inumber - 1) / inodes_per_sector;
    int inode_position = (inumber - 1) % inodes_per_sector;

    // Leer sector desde disco
    struct inode sector_inodes[inodes_per_sector];
    if (diskimg_readsector(fs->dfd, sector_number, sector_inodes) == -1) {
        return -1; // Error al leer sector del disco
    }

    // Copiar el inode especificado al puntero proporcionado
    *inp = sector_inodes[inode_position];

    return 0; 
}

/**
 * Given an index of a file block, retrieves the file's actual block number
 * from the given inode.
 *
 * Returns the disk block number on success, -1 on error.
 */
int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {

     /* Validación básica */
    if (blockNum < 0) return -1;

    int is_large = inp->i_mode & ILARG;

    if (!is_large) {
        /* ---------------------
         * ARCHIVO PEQUEÑO (punteros directos)
         * ---------------------*/
        if (blockNum >= INODE_DIRECT_PTR_COUNT) return -1;

        int diskBlock = inp->i_addr[blockNum] & 0xFFFF;
        return (diskBlock == 0) ? -1 : diskBlock;

    } else {
        /* ---------------------
         * ARCHIVO GRANDE (7 indirectos + 1 doble)
         * ---------------------*/
        const int PTRS_PER_BLOCK = DISKIMG_SECTOR_SIZE / sizeof(uint16_t); // 256
        const int SINGLE_MAX = 7 * PTRS_PER_BLOCK;                         // 1792

        if (blockNum < SINGLE_MAX) {
            /* ------ Indirecto simple ------ */
            int indirect_index      = blockNum / PTRS_PER_BLOCK; // 0‑6
            int offset_within_block = blockNum % PTRS_PER_BLOCK;

            int indirSector = inp->i_addr[indirect_index] & 0xFFFF;
            if (indirSector == 0) return -1;

            uint16_t indirBuf[PTRS_PER_BLOCK];
            if (diskimg_readsector(fs->dfd, indirSector, indirBuf) == -1) return -1;

            int diskBlock = indirBuf[offset_within_block] & 0xFFFF;
            return (diskBlock == 0) ? -1 : diskBlock;

        } else {
            /* ------ Indirecto doble ------ */
            int dblIndex = blockNum - SINGLE_MAX;
            const int PTRS_DBL = PTRS_PER_BLOCK * PTRS_PER_BLOCK; // 256*256
            if (dblIndex >= PTRS_DBL) return -1;

            int dblSector = inp->i_addr[7] & 0xFFFF;
            if (dblSector == 0) return -1;

            uint16_t dblBuf[PTRS_PER_BLOCK];
            if (diskimg_readsector(fs->dfd, dblSector, dblBuf) == -1) return -1;

            int first = dblIndex / PTRS_PER_BLOCK;
            int second = dblIndex % PTRS_PER_BLOCK;
            int singleSector = dblBuf[first] & 0xFFFF;
            if (singleSector == 0) return -1;

            uint16_t singleBuf[PTRS_PER_BLOCK];
            if (diskimg_readsector(fs->dfd, singleSector, singleBuf) == -1) return -1;

            int diskBlock = singleBuf[second] & 0xFFFF;
            return (diskBlock == 0) ? -1 : diskBlock;
        }
    }
}

int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
