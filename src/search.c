/*
  Axel -- A lighter download accelerator for Linux and other Unices

  Copyright 2001-2007 Wilmer van der Gaast
  Copyright 2008      Y Giridhar Appaji Nag
  Copyright 2010      Philipp Hagemeister
  Copyright 2016      Stephen Thirlwall
  Copyright 2017      Antonio Quartulli
  Copyright 2017      Ismael Luceno
  Copyright 2017      Joao Eriberto Mota Filho

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  In addition, as a special exception, the copyright holders give
  permission to link the code of portions of this program with the
  OpenSSL library under certain conditions as described in each
  individual source file, and distribute linked combinations including
  the two.

  You must obey the GNU General Public License in all respects for all
  of the code used other than OpenSSL. If you modify file(s) with this
  exception, you may extend this exception to your version of the
  file(s), but you are not obligated to do so. If you do not wish to do
  so, delete this exception statement from your version. If you delete
  this exception statement from all source files in the program, then
  also delete it here.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* filesearching.com searcher */

#include "axel.h"
#include "sleep.h"

static char *axel_strrstr(char *haystack, char *needle);
static void *search_speedtest(void *r);
static int search_sortlist_qsort(const void *a, const void *b);

#ifdef STANDALONE
int
main(int argc, char *argv[])
{
	conf_t conf[1];
	search_t *res;
	int i, j, num_mirrors, ret = 1;

	if (argc != 2) {
		fprintf(stderr, _("Incorrect amount of arguments\n"));
		return 1;
	}

	conf_init(conf);
	ssl_init(conf);

	res = calloc(conf->search_amount + 1, sizeof(search_t));
	if (!res)
		goto out;

	res->conf = conf;

	i = search_makelist(res, argv[1]);
	if (i == -1) {
		fprintf(stderr, _("File not found\n"));
		goto out;
	}
	num_mirrors = search_getspeeds(res, i);
	if (num_mirrors < 0) {
		fprintf(stderr, _("Speed testing failed\n"));
		goto out;
	}
	printf(_("%i usable mirrors:\n"), num_mirrors);
	search_sortlist(res, i);
	for (j = 0; j < i; j++)
		printf("%-70.70s %5i\n", res[j].url, res[j].speed);

	ret = 0;
out:
	free(res);

	return ret;
}
#endif

int
search_makelist(search_t *results, char *url)
{
	int i, size = 8192, j = 0;
	char *s, *s1, *s3;
	conn_t conn[1];
	double t;

	memset(conn, 0, sizeof(conn_t));

	conn->conf = results->conf;
	t = axel_gettime();
	if (!conn_set(conn, url) || !conn_init(conn) || !conn_info(conn))
		return -1;

	strncpy(results[0].url, url, sizeof(results[0].url) - 1);
	results[0].url[sizeof(results[0].url) - 1] = '\0';
	results[0].speed = 1 + 1000 * (axel_gettime() - t);
	results[0].size = conn->size;

	s = malloc(size);
	if (!s)
		return -1;

	sprintf(s, "http://www.filesearching.com/cgi-bin/s?q=%s&w=a&l=en&"
		"t=f&e=on&m=%i&o=n&s1=%lld&s2=%lld&x=15&y=15",
		conn->file, results->conf->search_amount,
		conn->size, conn->size);

	conn_disconnect(conn);
	memset(conn, 0, sizeof(conn_t));
	conn->conf = results->conf;

	if (!conn_set(conn, s) || !conn_setup(conn) || !conn_exec(conn)) {
		free(s);
		return 1;
	}

	while ((i = tcp_read(conn->tcp, s + j, size - j)) > 0) {
		j += i;
		if (j + 10 >= size) {
			size *= 2;
			char *tmp = realloc(s, size);
			if (!tmp) {
				free(s);
				return 1;
			}
			s = tmp;
			memset(s + size / 2, 0, size / 2);
		}
	}
	s[j] = '\0';

	conn_disconnect(conn);

	s1 = strstr(s, "<pre class=list");
	s1 = strchr(s1, '\n') + 1;
	if (strstr(s1, "</pre>") == NULL) {
		/* Incomplete list */
		free(s);
		return 1;
	}
	for (i = 1;
	     strncmp(s1, "</pre>", 6) && i < results->conf->search_amount && *s1;
         i++) {
		s3 = strchr(s1, '\n');
		*s3 = 0;
		char *s2 = axel_strrstr(s1, "<a href=") + 8;
		*s3 = '\n';
		s3 = strchr(s2, ' ');
		*s3 = 0;
		if (strcmp(results[0].url, s2)) {
			strncpy(results[i].url, s2, sizeof(results[i].url) - 1);
			results[i].size = results[0].size;
			results[i].conf = results->conf;
		} else {
			/* The original URL might show up */
			i--;
		}
		for (s1 = s3; *s1 != '\n'; s1++) ;
		s1++;
	}

	free(s);

	return i;
}

enum {
	SPEED_ACTIVE  = -3,
	SPEED_FAILED  = -2,
	SPEED_DONE    = -1,
	SPEED_PENDING =  0,
	/* Or > 0 */
};

int
search_getspeeds(search_t *results, int count)
{
	const struct timespec delay = {.tv_nsec = 10000000};

	int left = count, correct = 0;
	for (int i = 0; i < count; i++) {
		if (results[i].speed) {
			results[i].speed_start_time = 0;
			left--;
			if (results[i].speed > 0)
				correct++;
		}
	}

	for (int running = 0; left > 0; axel_sleep(delay)) {
		for (int i = 0; i < count; i++) {
			switch (results[i].speed) {
			case SPEED_ACTIVE:
				if (axel_gettime() < results[i].speed_start_time
				    + results->conf->search_timeout)
					continue; // not timed out yet
				pthread_cancel(*results[i].speed_thread);
				break; // do the bookkeeping
			case SPEED_FAILED:
				break; // do the bookkeeping
			case SPEED_DONE:
				continue; // already processed
			case SPEED_PENDING:
				if (running >= results->conf->search_threads)
					continue; // running too many, skip
				results[i].speed = SPEED_ACTIVE;
				results[i].speed_start_time = axel_gettime();
				if (pthread_create(results[i].speed_thread,
						   NULL, search_speedtest,
						   &results[i]) == 0)
					running++;
				else
					results[i].speed = SPEED_PENDING;
				continue; // go to the next item
			default:
				if (!results[i].speed_start_time)
					continue; // already processed
				// do the bookkeeping
			}
			// The thread must have been cancelled or finished
			pthread_join(*results[i].speed_thread, NULL);
			running--;
			left--;
			switch (results[i].speed) {
			case SPEED_ACTIVE:
			case SPEED_FAILED:
				results[i].speed = SPEED_DONE;
				break;
			default:
				results[i].speed_start_time = 0;
				if (results[i].speed > 0)
					correct++;
			}
		}
	}

	return correct;
}

static
void *
search_speedtest(void *r)
{
	search_t *results = r;
	conn_t conn[1];
	int oldstate;

	/* Allow this thread to be killed at any time. */
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

	memset(conn, 0, sizeof(conn_t));
	conn->conf = results->conf;
	if (conn_set(conn, results->url)
	    && conn_init(conn)
	    && conn_info(conn)
	    && conn->size == results->size)
		/* Add one because it mustn't be zero */
		results->speed = 1 + 1000 * (axel_gettime() - results->speed_start_time);
	else
		results->speed = SPEED_FAILED;

	conn_disconnect(conn);

	return NULL;
}

static
char *
axel_strrstr(char *haystack, char *needle)
{
	int i, j;

	for (i = strlen(haystack) - strlen(needle); i > 0; i--) {
		for (j = 0; needle[j] && haystack[i + j] == needle[j]; j++) ;
		if (!needle[j])
			return haystack + i;
	}

	return NULL;
}

void
search_sortlist(search_t *results, int count)
{
	qsort(results, count, sizeof(search_t), search_sortlist_qsort);
}

static
int
search_sortlist_qsort(const void *a, const void *b)
{
	if (((search_t *) a)->speed < 0 && ((search_t *) b)->speed > 0)
		return 1;
	if (((search_t *) a)->speed > 0 && ((search_t *) b)->speed < 0)
		return -1;
	if (((search_t *) a)->speed < ((search_t *) b)->speed)
		return -1;
	else
		return ((search_t *) a)->speed > ((search_t *) b)->speed;
}
