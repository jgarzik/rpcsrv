
/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <cstdio>
#include <cstring>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

int write_pid_file(const char *pid_fn)
{
	char str[32], *s;
	size_t bytes;
	int fd;
	struct flock lock;
	int err;

	/* build file data */
	sprintf(str, "%u\n", (unsigned int) getpid());

	/* open non-exclusively (works on NFS v2) */
	fd = open(pid_fn, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		err = errno;

		syslog(LOG_DAEMON|LOG_ERR, "Cannot open PID file %s: %s",
			 pid_fn, strerror(err));
		return -err;
	}

	/* lock */
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	if (fcntl(fd, F_SETLK, &lock) != 0) {
		err = errno;
		if (err == EAGAIN) {
			syslog(LOG_DAEMON|LOG_ERR, "PID file %s is already locked",
				 pid_fn);
		} else {
			syslog(LOG_DAEMON|LOG_ERR, "Cannot lock PID file %s: %s",
				 pid_fn, strerror(err));
		}
		close(fd);
		return -err;
	}

	/* write file data */
	bytes = strlen(str);
	s = str;
	while (bytes > 0) {
		ssize_t rc = write(fd, s, bytes);
		if (rc < 0) {
			err = errno;
			syslog(LOG_DAEMON|LOG_ERR, "PID number write failed: %s",
				 strerror(err));
			goto err_out;
		}

		bytes -= rc;
		s += rc;
	}

	/* make sure file data is written to disk */
	if (fsync(fd) < 0) {
		err = errno;
		syslog(LOG_DAEMON|LOG_ERR, "PID file fsync failed: %s", strerror(err));
		goto err_out;
	}

	return fd;

err_out:
	unlink(pid_fn);
	close(fd);
	return -err;
}

