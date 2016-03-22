/**
 * Nanobenchmark: ADD
 *   JC. PROCESS = {ovewrite file and sync. at /test/$PROCESS}
 *       - TEST: journal commit
 */	      
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "fxmark.h"
#include "util.h"
#include <stdlib.h>	/*to use posix_memalign*/
#include <assert.h> /*to use assert() */

static void set_test_root(struct worker *worker, char *test_root)
{
	struct fx_opt *fx_opt = fx_opt_worker(worker);
	sprintf(test_root, "%s/%d", fx_opt->root, worker->id);
}

static int pre_work(struct worker *worker)
{
  	char *page = NULL;
	struct bench *bench = worker->bench;
	char test_root[PATH_MAX];
	char file[PATH_MAX];
	int fd, rc = 0;

	/* create test root */
	set_test_root(worker, test_root);
	rc = mkdir_p(test_root);
	if (rc) return rc;

	/* create a test file */ 
	snprintf(file, PATH_MAX, "%s/n_jnl_cmt.dat", test_root);
	if ((fd = open(file, O_CREAT | O_RDWR, S_IRWXU)) == -1)
		goto err_out;

	/* allocate data buffer aligned with pagesize*/
	if(posix_memalign((void **)&(worker->page), PAGE_SIZE, PAGE_SIZE))
	  goto err_out;

	page = worker->page;
	assert(page);

	/*set flag with O_DIRECT if need*/
	if(bench->directio && (fcntl(fd, F_SETFL, O_DIRECT)==-1))
	  goto err_out;

	if (write(fd, page, PAGE_SIZE) != PAGE_SIZE)
		goto err_out;
out:
	/* put fd to worker's private */
	worker->private[0] = (uint64_t)fd;
	return rc;
err_out:
	rc = errno;
	goto out;
}

static int main_work(struct worker *worker)
{
  	char *page = worker->page;
	struct bench *bench = worker->bench;
	int fd, rc = 0;
	uint64_t iter = 0;

	assert(page);

	/* fsync */
	fd = (int)worker->private[0];
	for (iter = 0; !bench->stop; ++iter) {
	        if (pwrite(fd, page, PAGE_SIZE, 0) != PAGE_SIZE)
			goto err_out;
		if (fsync(fd) == -1)
			goto err_out;
	}
out:
	close(fd);
	worker->works = (double)iter;
	return rc;
err_out:
	bench->stop = 1;
	rc = errno;
	goto out;
}

struct bench_operations n_jnl_cmt_ops = {
	.pre_work  = pre_work, 
	.main_work = main_work,
};
