#ifndef WLX_UTILS_SHM_H
#define WLX_UTILS_SHM_H

#include <stdlib.h>

int create_shm_file(size_t size);

bool create_shm_file_pair(size_t size,
						  int	*rw_fd_ptr,
						  int	*ro_fd_ptr);

#endif // WLX_UTILS_SHM_H
