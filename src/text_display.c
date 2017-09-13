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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <ncurses.h>
#include <time.h>
#include <wchar.h>
#include <ctype.h>

#include "latencytop.h"

static WINDOW *title_bar_window;
static WINDOW *global_window;
static WINDOW *process_window;
static WINDOW *right_window;

static struct process *cursor_e;

static void cleanup_curses(void)
{
	endwin();
}


static void zap_windows(void)
{
	if (title_bar_window) {
		delwin(title_bar_window);
		title_bar_window = NULL;
	}
	if (global_window) {
		delwin(global_window);
		global_window = NULL;
	}
	if (process_window) {
		delwin(process_window);
		process_window = NULL;
	}
	if (right_window) {
		delwin(right_window);
		right_window = NULL;
	}
}


static int maxx, maxy;

static void setup_windows(void)
{
	int midy;
	getmaxyx(stdscr, maxy, maxx);

	zap_windows();

	midy = (maxy+4)/2;
	title_bar_window = subwin(stdscr, 1, maxx, 0, 0);
	global_window = subwin(stdscr, midy-4 , maxx, 2, 0);
	process_window = subwin(stdscr, 1, maxx, maxy-1, 0);
	right_window = subwin(stdscr, (maxy-midy-3), maxx, midy, 0);

	werase(stdscr);
	refresh();
}

static void show_title_bar(void)
{
	wattrset(title_bar_window, COLOR_PAIR(PT_COLOR_HEADER_BAR));
	wbkgd(title_bar_window, COLOR_PAIR(PT_COLOR_HEADER_BAR));
	werase(title_bar_window);

	mvwprintw(title_bar_window, 0, 0,  "   LatencyTOP version "VERSION"       (C) 2008 Intel Corporation");

	wrefresh(title_bar_window);
}



static void print_global_list(void)
{
	struct latency_line *line;
	int i = 1;

	mvwprintw(global_window, 0, 0, "Cause");
	mvwprintw(global_window, 0, 50, "   Maximum     Percentage\n");

	list_for_each(lines, line, node) {
		if (i >= 10)
			break;

		if (line->max*0.001 < 0.1)
			continue;
		mvwprintw(global_window, i, 0, "%s", line->reason);
		mvwprintw(global_window, i, 50, "%5.1f msec        %5.1f %%\n",
				line->max * 0.001,
				(line->time * 100 +0.0001) / total_time);
		i++;
	}

	wrefresh(global_window);

}

static void display_process_list(unsigned int cursor_pid, char filter)
{
	struct process *proc = NULL;
	int i = 0, xpos = 0;
	char startswith;

retry:
	werase(process_window);
	xpos = 0;
	proc = cursor_e;
	if (!proc) {
		proc = list_top(procs, struct process, node);
		cursor_e = proc;
	}

	if (!proc)
		return;

	while (proc->pid > cursor_pid && cursor_pid > 0) {
		proc = list_prev(procs, proc, node);
		cursor_e = proc;
	}

	/* and print 7 */
	i = 0;
	while (proc) {
		startswith = proc->name[0];
		startswith = toupper(startswith);
		if ((filter != '\0') && (startswith != filter)) {
			proc = list_next(procs, proc, node);
			continue;
		}

		if (proc->pid == cursor_pid) {
			if (xpos + strlen(proc->name) + 2 > maxx && cursor_e) {
				cursor_e = list_next(procs, cursor_e, node);
				goto retry;
			}
			wattron(process_window, A_REVERSE);
		}

		if (xpos + strlen(proc->name) + 2 <= maxx)
			mvwprintw(process_window, 0, xpos, " %s ", proc->name);
		xpos += strlen(proc->name)+2;

		wattroff(process_window, A_REVERSE);

		proc = list_next(procs, proc, node);

		i++;
	}
	wrefresh(process_window);
}

static int one_pid_back(unsigned int cursor_pid, char filter)
{
	struct process *proc = NULL;
	char startswith;

	list_for_each(procs, proc, node) {
		if (proc->pid == cursor_pid) {
			break;
		}
	}

	while (proc) {
		if (list_prev(procs, proc, node))
			proc = list_prev(procs, proc, node);
		if (proc) {
			startswith = proc->name[0];
			startswith = toupper (startswith);
			if ((filter == '\0') || (startswith == filter))
				return proc->pid;
			else
				proc = list_prev(procs, proc, node);
		}
	}

	return 0;
}

static int one_pid_forward(unsigned int cursor_pid, char filter)
{
	struct process *proc = NULL;
	char startswith;

	list_for_each(procs, proc, node) {
		if (proc->pid == cursor_pid) {
			break;
		}
	}
	while (proc) {
		if (list_next(procs, proc, node))
				proc = list_next(procs, proc, node);
		if (proc) {
			startswith = proc->name[0];
			startswith = toupper (startswith);
			if ((filter == '\0') || (startswith == filter))
				return proc->pid;
			else
				proc = list_next(procs, proc, node);
		}
	}
	return 0;
}

static void print_process(unsigned int pid)
{
	struct process *proc;
	werase(right_window);
	double total = 0.0;

	list_for_each(procs, proc, node) {
		char header[4096];
		int i = 0;
		struct latency_line *line;

		if (proc->pid != pid)
			continue;

		wattron(right_window, A_REVERSE);
		sprintf(header, "Process %s (%i) ", proc->name, proc->pid);

		while (strlen(header) < maxx)
			strcat(header, " ");

		mvwprintw(right_window, 0, 0, "%s", header);

		list_for_each(&proc->latencies, line, node) {
			if (i >= 6)
				break;
			total = total + line->time;
		}

		mvwprintw(right_window, 0, 43, "Total: %5.1f msec", total*0.001);
		wattroff(right_window, A_REVERSE);

		list_for_each(&proc->latencies, line, node) {
			if (i >= 6)
				break;

			if (line->max*0.001 < 0.1)
				continue;
			mvwprintw(right_window, i+1, 0, "%s", line->reason);
			mvwprintw(right_window, i+1, 50, "%5.1f msec        %5.1f %%",
				line->max * 0.001,
				(line->time * 100 +0.0001) / total
				);
			i++;
		}
	}
	wrefresh(right_window);
}

static int done_yet(int time, struct timeval *p1)
{
	int seconds;
	int usecs;
	struct timeval p2;
	gettimeofday(&p2, NULL);
	seconds = p2.tv_sec - p1->tv_sec;
	usecs = p2.tv_usec - p1->tv_usec;

	usecs += seconds * 1000000;
	if (usecs > time * 1000000)
		return 1;
	return 0;
}



static int update_display(int duration, char *filterchar)
{
	struct timeval start,end,now;
	int key;
	int repaint = 1;
	fd_set rfds;

	gettimeofday(&start, NULL);
	setup_windows();
	show_title_bar();
	print_global_list();
	while (!done_yet(duration, &start)) {
		if (repaint) {
			display_process_list(pid_with_max, *filterchar);
			print_process(pid_with_max);
		}
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		gettimeofday(&now, NULL);
		end.tv_sec = start.tv_sec + duration - now.tv_sec;
		end.tv_usec = start.tv_usec - now.tv_usec;
		while (end.tv_usec < 0) {
			end.tv_sec --;
			end.tv_usec += 1000000;
		};
		key = select(1, &rfds, NULL, NULL, &end);
		repaint = 1;

		if (key) {
			char keychar;
			keychar = fgetc(stdin);
			if (keychar == 27) {
				keychar = fgetc(stdin);
				if (keychar==79)
					keychar = fgetc(stdin);
			}
			keychar = toupper(keychar);
			if (keychar == 'Z' || keychar == 'A' || keychar == 'D')
				pid_with_max = one_pid_back(pid_with_max, *filterchar);
			if (keychar == 'X' || keychar == 'B' || keychar == 'C')
				pid_with_max = one_pid_forward(pid_with_max, *filterchar);
			if (keychar == 'Q')
				return 0;
			if (keychar == 'R') {
				cursor_e = NULL;
				return 1;
			}
			if (keychar == 'S') {
				keychar = fgetc(stdin);
				if (keychar == 27) {
					keychar = fgetc(stdin);
					if (keychar==79)
						keychar = fgetc(stdin);
				}
				keychar = toupper (keychar);
				if (keychar >= 'A' && keychar <= 'Z')
					*filterchar = keychar;
				else if (keychar == '0')
					*filterchar = '\0';
			}
			if (keychar == 'F') {
				endwin();
				if (!fsync_display(duration))
					return 0;
				setup_windows();
				show_title_bar();
			}
			if (keychar < 32)
				repaint = 0;
		}
	}
	cursor_e = NULL;
	return 1;
}

void preinitialize_text_ui(int *argc, char ***argv)
{
}

void start_text_ui(void)
{
	char filterchar = '\0';
	int ret = 1;

	initscr();
	start_color();
	keypad(stdscr, TRUE);	/* enable keyboard mapping */
	nonl();			/* tell curses not to do NL->CR/NL on output */
	cbreak();		/* take input chars one at a time, no wait for \n */
	noecho();		/* dont echo input */
	curs_set(0);		/* turn off cursor */
	use_default_colors();

	init_pair(PT_COLOR_DEFAULT, COLOR_WHITE, COLOR_BLACK);
	init_pair(PT_COLOR_HEADER_BAR, COLOR_BLACK, COLOR_WHITE);
	init_pair(PT_COLOR_ERROR, COLOR_BLACK, COLOR_RED);
	init_pair(PT_COLOR_RED, COLOR_WHITE, COLOR_RED);
	init_pair(PT_COLOR_YELLOW, COLOR_WHITE, COLOR_YELLOW);
	init_pair(PT_COLOR_GREEN, COLOR_WHITE, COLOR_GREEN);
	init_pair(PT_COLOR_BRIGHT, COLOR_WHITE, COLOR_BLACK);

	atexit(cleanup_curses);

	while (ret) {
		update_list();
		ret = update_display(30, &filterchar);
	}
}

