#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

#define MAX		1024
#define CMD_QUIT	1
#define CMD_PNEXT	2
#define CMD_LNEXT	4

int tty;

int cmdchr(char c) {
	int cmd;

	switch (c) {
	case 'q':
		cmd = CMD_QUIT;
		break;

	case 'f':
	case ' ':
		cmd = CMD_PNEXT;
		break;

	case 'j':
	case '\n':
		cmd = CMD_LNEXT;
		break;

	default:
		cmd = 0;
		break;
	}

	return cmd;
}


// from "/usr/src/contrib/less/screen.c"
void raw_mode(int on) {
	static int curr_on = 0;
	struct termios s;
	static struct termios save_term;
	static int saved_term = 0;

	if (on == curr_on) {
		return;
	}

	if (on) {
		/*
		 * Get terminal modes.
		 */
		ioctl(tty, TIOCGETA, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term) {
			save_term = s;
			saved_term = 1;
		}

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
		// s.c_oflag |=  (OPOST|ONLCR|TAB3);
		// s.c_oflag &= ~(OCRNL|ONOCR|ONLRET);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
	} else {
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	ioctl(tty, TIOCSETAW, &s);

	curr_on = on;
}

void more(FILE *fp) {
	unsigned short sys_width;
	unsigned short sys_height;
	char buf[MAX];
	int line;
	int cmd;

	struct winsize w;
	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0) {
		if (w.ws_row > 0)
			sys_height = w.ws_row;
		if (w.ws_col > 0)
			sys_width = w.ws_col;
	}

	/*
	// on raw-mode?
	if (ioctl(tty, TIOCCBRK) == -1) {
		perror("ioctl(TIOCCBRK)");
		close(tty);
		return 0;
	}
*/

	line = 0;
	while (fgets(buf, MAX, fp) != NULL) {
		fputs(buf, stdout);

		if (++line - (sys_height - 1) >= 0) {
//			fputs("---Speace---", stderr);
READ:
			read(tty, buf, 1);

			cmd = cmdchr(*buf);
			if (cmd == CMD_QUIT) {
				break;
			} else if (cmd == CMD_PNEXT) {
				line = 0;
			} else if (cmd == CMD_LNEXT) {
				line--;
			} else {
				goto READ;
			}
		}
	}
}

void cat(FILE *fp) {
	char buf[MAX];

	while (fgets(buf, MAX, fp) != NULL) {
		fputs(buf, stdout);
	}
}

int main(int argc, char *argv[]) {
	FILE *fp;
	int needclose;
	int ttyfd;
	int line;

	if (isatty(STDIN_FILENO) == 0) {
		fp = stdin;
		needclose = 0;
	} else {
		if (argc <= 1) {
			printf("Usage: mymore [file]\n");
			return 0;
		}

		if ((fp = fopen(argv[1], "r")) == NULL) {
			perror("fopen");
			return 1;
		}
		needclose = 1;
	}

	// cat
	if (isatty(STDOUT_FILENO) == 0) {
		cat(fp);
	// more
	} else {
		if ((tty = open("/dev/tty", O_RDONLY)) == -1) {
			perror("open");
			// return;
			tty = STDERR_FILENO;
		}

		raw_mode(1);
		more(fp);
		raw_mode(0);

		if (tty != STDERR_FILENO) {
			close(tty);
		}
	}

	if (needclose) {
		fclose(fp);
	}

	return 0;
}
