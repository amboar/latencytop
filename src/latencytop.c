/*
 * Copyright 2008, Intel Corporation
 *
 * This file is part of LatencyTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authors:
 * 	Arjan van de Ven <arjan@linux.intel.com>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "latencytop.h"

LIST_HEAD(_lines);
struct list_head *lines = &_lines;

LIST_HEAD(_procs);
struct list_head *procs = &_procs;

LIST_HEAD(_allprocs);
struct list_head *allprocs = &_allprocs;

int total_time = 0;
int total_count = 0;

unsigned int pid_with_max = 0;
unsigned int pidmax = 0;
int firsttime = 1;
int noui; 
int dump_unknown;

char *prefered_process;

static void add_to_global(struct latency_line *line)
{
	struct latency_line *line2;

	list_for_each(lines, line2, node) {
		if (strcmp(line->reason, line2->reason)==0) {
			line2->count += line->count;
			line2->time += line->time;
			if (line->max > line2->max)
				line2->max = line->max;
			free(line);
			return;
		}
	}

	list_add_tail(lines, &line->node);
}

static void add_to_process(struct process *process, struct latency_line *line)
{
	struct latency_line *line2;

	list_for_each(&process->latencies, line2, node) {
		if (strcmp(line->reason, line2->reason)==0) {
			line2->count += line->count;
			line2->time += line->time;
			if (line->max > line2->max)
				line2->max = line->max;
			free(line);
			return;
		}
	}

	list_add_tail(&process->latencies, &line->node);
}

static void fixup_reason(struct latency_line *line, char *c)
{
	char *c2;

	c2 = strchr(c, '\n');
	if (c2)
		*c2=0;
	while (c[0]==' ')
		c++;
	strncpy(line->backtrace, c, 4096);
	c2 = translate(c);
	if (c2 == NULL) {
		line->reason[0] = '[';
		strncpy(line->reason + 1, c, 1022);
		for (c2 = line->reason + 1; *c2 && (c2 - line->reason) < 1022; c2++)
			if (*c2 == ' ')
				break;
		*(c2++) = ']';
		*(c2++) = 0;
	} else
		strncpy(line->reason, c2, 1024);
}

void parse_global_list(void)
{
	FILE *file;
	char *ln;
	size_t dummy;
	file = fopen("/proc/latency_stats","r+");
	if (!file) {
		fprintf(stderr, "Please enable the CONFIG_LATENCYTOP configuration in your kernel.\n");
		fprintf(stderr, "Exiting...\n");
		exit(EXIT_FAILURE);
	}
	/* wipe first line */
	ln = NULL;
	if (getline(&ln, &dummy, file) < 0) {
		free(ln);
		fclose(file);
		return;
	}
	free(ln);
	total_time = 0;
	total_count = 0;
	while (!feof(file)) {
		struct latency_line *line;
		char *c;
		ln = NULL;
		if (getline(&ln, &dummy, file) < 0) {
			free(ln);
			break;
		}
		if (strlen(ln)<2) {
			free(ln);
			break;
		}
		line = malloc(sizeof(struct latency_line));
		memset(line, 0, sizeof(struct latency_line));
		line->count = strtoull(ln, &c, 10);
		line->time = strtoull(c, &c, 10);
		line->max = strtoull(c, &c, 10);
		total_time += line->time;
		total_count += line->count;
		fixup_reason(line, c);
		add_to_global(line);
		free(ln);
		ln = NULL;
	}
	/* reset for next time */
	fprintf(file, "erase\n");
	fclose(file);
}

#if 0
gint comparef(gconstpointer A, gconstpointer B)
{
	struct latency_line *a, *b;
	a = (struct latency_line *)A; 
	b = (struct latency_line *)B;
	if (a->max >  b->max)
		return -1;
	if (a->max < b->max)
		return 1;
	if (a->time >  b->time)
		return -1;
	if (a->time < b->time)
		return 1;
	return -1;
}
#endif

void sort_list(void)
{
	struct latency_line *line;

	total_time = 0;
#if 0 /* We need to sort this shit somehow */
	lines = g_list_sort(lines, comparef);
#endif

	list_for_each(lines, line, node) {
		total_time = total_time + line->time;
	}
}


void delete_list(void)
{
	struct latency_line *lcurrent = NULL, *lnext = NULL;
	struct process *pcurrent, *pnext;

	if (!list_empty(lines)) {
		list_for_each_safe(lines, lcurrent, lnext, node) {
			list_del(&lcurrent->node);
			free(lcurrent);
		}
	}

	list_for_each_safe(allprocs, pcurrent, pnext, node) {
		list_for_each_safe(&pcurrent->latencies, lcurrent, lnext, node) {
			list_del(&lcurrent->node);
			free(lcurrent);
		}

		pcurrent->max = 0;
		if (!pcurrent->exists && !pcurrent->pinned) {
			list_del(&pcurrent->node);
			free(pcurrent);
		}
	}
}

void prune_unused_procs(void)
{
	struct latency_line *lcurrent, *lnext;
	struct process *pcurrent, *pnext;

	list_for_each_safe(procs, pcurrent, pnext, node) {
		if (!pcurrent->used && !pcurrent->pinned) {
			list_for_each_safe(&pcurrent->latencies, lcurrent,
					   lnext, node) {
				list_del(&lcurrent->node);
				free(lcurrent);
			}
			list_del(&pcurrent->node);
			free(pcurrent);
		}
	}
}

void parse_process(struct process *process)
{
	DIR *dir;
	struct dirent *dirent;
	char filename[PATH_MAX];

	sprintf(filename, "/proc/%i/task/", process->pid);

	dir = opendir(filename);
	if (!dir)
		return;
	while ((dirent = readdir(dir))) {
		FILE *file;
		char *line = NULL;
		size_t dummy;
		int pid;
		if (dirent->d_name[0]=='.')
			continue;
		pid = strtoull(dirent->d_name, NULL, 10);
		if (pid<=0) /* not a process */
			continue;


		sprintf(filename, "/proc/%i/task/%i/latency", process->pid, pid);
		file = fopen(filename, "r+");
		if (!file)
			continue;
		/* wipe first line*/
		if (getline(&line, &dummy, file) < 0) {
			free(line);
			continue;
		}
		free(line);
		while (!feof(file)) {
			struct latency_line *ln;
			char *c, *c2;
			line = NULL;
			if (getline(&line, &dummy, file) < 0) {
				free(line);
				break;
			}
			if (strlen(line)<2) {
				free(line);
				break;
			}
			ln = malloc(sizeof(struct latency_line));
			memset(ln, 0, sizeof(struct latency_line));
			ln->count = strtoull(line, &c, 10);
			ln->time = strtoull(c, &c, 10);
			ln->max = strtoull(c, &c, 10);
			fixup_reason(ln, c);

			if (ln->max > process->max)
				process->max = ln->max;
			add_to_process(process, ln);
			process->used = 1;
			free(line);
			line = NULL;
		}
		/* reset for next time */
		fprintf(file, "erase\n");
		fclose(file);
	}
	/* 100 usec minimum */
	if (!firsttime) {
		struct latency_line *ln, *ln2;
			
		ln = malloc(sizeof(struct latency_line));
		ln2 = malloc(sizeof(struct latency_line));
		memset(ln, 0, sizeof(struct latency_line));
		
		if (process->delaycount)
			ln->count = process->delaycount;
		else
			ln->count = 1;
		if (process->totaldelay > 0.00001)
			ln->time = process->totaldelay * 1000;
		else
			ln->time = process->maxdelay * 1000;    
		ln->max = process->maxdelay * 1000;    
		strcpy(ln->reason, "Scheduler: waiting for cpu");
		if (ln->max > process->max)
			process->max = ln->max;
		memcpy(ln2, ln, sizeof(struct latency_line));
		add_to_process(process, ln);
		add_to_global(ln2);
		process->used = 1;
	}
	closedir(dir);
}

struct process* find_create_process(unsigned int pid)
{
	struct process *proc;

	list_for_each(allprocs, proc, node) {
		if (proc->pid == pid) {
			return proc;
		}
	}

	proc = malloc(sizeof(struct process));
	memset(proc, 0, sizeof(struct process));
	proc->pid = pid;
	list_head_init(&proc->latencies);
	list_add_tail(allprocs, &proc->node);

	return proc;
}

void parse_processes(void)
{
	DIR *dir;
	struct dirent *dirent;
	char filename[PATH_MAX];
	struct process *process;

	pidmax = 0;

	dir = opendir("/proc");
	if (!dir)
		return;

	/* Should we loop and clear exists and/or used here ? */

	while ((dirent = readdir(dir))) {
		FILE *file;
		char *line;
		size_t dummy;
		int pid;
		if (dirent->d_name[0]=='.')
			continue;
		pid = strtoull(dirent->d_name, NULL, 10);
		if (pid<=0) /* not a process */
			continue;

		process = find_create_process(pid);
		process->exists = 1;

		sprintf(filename, "/proc/%i/status", pid);
		file = fopen(filename, "r");
		if (file) {
			char *q;
			line = NULL;
			if (getline(&line, &dummy, file) >= 0) {
				strncpy(&process->name[0], &line[6], 64);
				q = strchr(process->name, '\n');
				if (q) *q=0;
				fclose(file);
			}
			free(line);
			line = NULL;
		}

		if (process->name && prefered_process && strcmp(process->name, prefered_process)==0) {
			pid_with_max = pid;
			pidmax = INT_MAX;
		}

		sprintf(filename, "/proc/%i/sched", pid);
		file = fopen(filename, "r+");
		if (file) {
			char *q;
			double d;
			while (!feof(file)) {
				line = NULL;
				if (getline(&line, &dummy, file) < 0) {
					free(line);
					break;
				}
				q = strchr(line, ':');
				if (strstr(line, "se.wait_max") && q) {
					sscanf(q+1,"%lf", &d);
					process->maxdelay = d;
				}
				if (strstr(line, "se.wait_sum") && q) {
					sscanf(q+1,"%lf", &d);
					process->totaldelay = d;
				}
				if (strstr(line, "se.wait_count") && q) {
					sscanf(q+1,"%lf", &d);
					process->delaycount = d;
				}
				free(line);
				line = NULL;
			}
			fprintf(file,"erase");
			fclose(file);
		}

		sprintf(filename, "/proc/%i/statm", pid);
		file = fopen(filename, "r");
		if (file) {
			line = NULL;
			if (getline(&line, &dummy, file) >= 0) {
				if (strcmp(line, "0 0 0 0 0 0 0\n")==0)
					process->kernelthread = 1;
			}
			fclose(file);
			free(line);
			line = NULL;
		}

		parse_process(process);

		/* If process is pinned, we always add it to the list */
		if (!list_empty(&process->latencies)) {
#if 0 /* Need to sort this somehow */
			process->latencies = g_list_sort(process->latencies, comparef);
#endif
		}
		if (!list_empty(&process->latencies) || process->pinned) {
			if (!process->kernelthread && pidmax < process->max) {	
				pidmax = process->max;
				pid_with_max = process->pid;
			}
			list_add_tail(procs, &process->node);
		};
	}
	closedir(dir);
}


void dump_global_to_console(void)
{
	struct latency_line *line;

	list_for_each(lines, line, node) {
		printf( "[max %4.1fmsec] %40s - %5.2f msec (%3.1f%%)\n",
			line->max * 0.001,
			line->reason,
			(line->time * 0.001 +0.0001) / line->count,
			line->time * 100.0 /  total_time );
	}
}

static void enable_sysctl(void)
{
	FILE *file;
	file = fopen("/proc/sys/kernel/latencytop", "w");
	if (!file)
		return;
	fprintf(file, "1");
	fclose(file);
}

static void disable_sysctl(void)
{
	FILE *file;
	file = fopen("/proc/sys/kernel/latencytop", "w");
	if (!file)
		return;
	fprintf(file, "0");
	fclose(file);
}

void update_list(void)
{
	delete_list();
	parse_processes();
	prune_unused_procs();
	parse_global_list();
	sort_list();
	if (!total_time)
		total_time = 1;
	firsttime = 0;
}

static void cleanup_sysctl(void) 
{
	disable_sysctl();
	disable_fsync_tracer();
}

int main(int argc, char **argv)
{
	int i, use_gtk = 0;

	enable_sysctl();
	enable_fsync_tracer();
	atexit(cleanup_sysctl);

#ifdef HAS_GTK_GUI
	if (preinitialize_gtk_ui(&argc, &argv))
		use_gtk = 1;
#endif
	if (!use_gtk)
		preinitialize_text_ui(&argc, &argv);

	for (i = 1; i < argc; i++)		
		if (strcmp(argv[i],"-d") == 0) {
			init_translations("latencytop.trans");
			parse_global_list();
			sort_list();
			dump_global_to_console();
			return EXIT_SUCCESS;
		}
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "--unknown") == 0) {
			noui = 1;
			dump_unknown = 1;
		}

	/* Allow you to specify a process name to track */
	for (i = 1; i < argc; i++)
		if (argv[i][0] != '-') {
			prefered_process = strdup(argv[i]);
			break;
		}

	init_translations("/usr/share/latencytop/latencytop.trans");
	if (!translations)
		init_translations("latencytop.trans"); /* for those who don't do make install */
	
	while (noui) {
		sleep(5);
		fprintf(stderr, ".");
	}
#ifdef HAS_GTK_GUI
	if (use_gtk)
		start_gtk_ui();
	else
#endif
		start_text_ui();

	prune_unused_procs();
	delete_list();

	return EXIT_SUCCESS;
}
