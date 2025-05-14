#ifndef _PATHNAME_H_
#define _PATHNAME_H_

#include "unixfilesystem.h"

/* solo la firma */
int pathname_lookup(struct unixfilesystem *fs, const char *pathname);

#endif /* _PATHNAME_H_ */

