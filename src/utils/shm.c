#include "shm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static char *rand_name()
{
	const char	 *prefix = "/wlx-shm-";
	const char	 *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	const uint8_t randlen = 8;

	// init name string
	char *name = (char *)malloc(strlen(prefix) + randlen + 1);
	strcpy(name, prefix);

	// set seed to ns time
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	srand(ts.tv_nsec);

	// create random string
	for (int i = 0; i < randlen; i++) {
		(name + strlen(prefix))[i] = charset[rand() % strlen(charset)];
	}
	name[strlen(prefix) + randlen] = '\0';
	return name;
}

static int open_shm_fd(char *name)
{
	int retries = 100;
	do {
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd >= 0)
			return fd;

		retries--;
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

int create_shm_file(size_t size)
{
	char *name = rand_name();
	int	  fd = open_shm_fd(name);
	if (fd < 0) {
		free(name);
		return -1;
	}

	shm_unlink(name);

	int ret;
	do
		ret = ftruncate(fd, size);
	while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

bool create_shm_file_pair(size_t size,
						  int	*rw_fd_ptr,
						  int	*ro_fd_ptr)
{
	char *name = rand_name();
	int	  rw_fd = open_shm_fd(name);
	if (rw_fd < 0) {
		free(name);
		return false;
	}

	int ro_fd = shm_open(name, O_RDONLY, 0);
	shm_unlink(name);
	free(name);

	if (ro_fd < 0) {
		close(rw_fd);
		return false;
	}

	int ret;
	do
		ret = ftruncate(rw_fd, size);
	while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		close(rw_fd);
		close(ro_fd);
		return false;
	}

	*rw_fd_ptr = rw_fd;
	*ro_fd_ptr = ro_fd;
	return true;
}
