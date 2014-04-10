#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termcap.h>
#include <termios.h>

#define MAX		1024
#define TENTSIZ		1024
#define CMD_QUIT	0x01
#define CMD_PNEXT	0x02
#define CMD_LNEXT	0x04
#define CMD_PPREV	0x08
#define CMD_LPREV	0x10


static int tputc(int c) {
	return write(STDOUT_FILENO, &c, 1);
}
#define	tputs_x(s)	(tputs(s, 1, tputc))


char PC;
char *BC;
char *CL;
char *CM;
char *UP;
char *SF;
char *SR;
char *SC;
char *RC;
char *HO;
char *CE;

int tty;
char tent[TENTSIZ];


int termcap() {
	char *term;
	char *p;
	char *tmp;

	term = getenv("TERM");
	if (term == NULL) {
		return -1;
	}

	if (tgetent(tent, term) == -1) {
		return -1;
	}

	p = tent;
	if ((tmp = tgetstr("cl", &p)) != NULL) {
		PC = *tmp;
	}
	CL = tgetstr("cl", &p);
	CM = tgetstr("cm", &p);
	UP = tgetstr("up", &p);
	SR = tgetstr("sr", &p);
	HO = tgetstr("ho", &p);
	RC = tgetstr("rc", &p);
	SC = tgetstr("sc", &p);
	CE = tgetstr("ce", &p);

	return 0;
}

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

	case 'u':
		cmd = CMD_PPREV;
		break;

	case 'k':
		cmd = CMD_LPREV;
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

char* getline(char *p, int line) {
	int l;

	l = 0;
	while (*p++ != '\0') {
		if (*p == '\n') {
			if (l++ >= line) {
				break;
			}
			p++;
		}
	}

	return p;
}

#define putline(p, l)	(putlines(p, l, 1))

void putlines(char *p, int line, int range) {
	char *q;
	int i;

	for (i = 0; i < line; i++) {
		if ((p = strchr(p, '\n')) == NULL) {
			return;
		}
		p++;
	}

	for (i = 0; i < range; i++) {
		if ((q = strchr(p, '\n')) == NULL) {
			break;
		}

		*q = '\0';
		puts(p);
		*q = '\n';
		p = q + 1;
	}
}

char* readfile(int fd, int bufsiz) {
	char *buf, *p;
	int count, len;

	buf = (char*)malloc(bufsiz * sizeof(char));
	if (buf == NULL) {
		return NULL;
	}

	p = buf;
	count = 1;
	while ((len = read(fd, p, bufsiz)) > 0) {
		len += p - buf;

		buf = (char*)realloc(buf, bufsiz * ++count * sizeof(char));
		if (buf == NULL) {
			return NULL;
		}

		p = buf + len;
	}

	return buf;
}

int countlines(char *p) {
	int lines;

	lines = 1;
	while ((p = strchr(p, '\n')) != NULL) {
		p++;
		lines++;
	}

	return lines;
}

void more(FILE *fp) {
	struct winsize w;
	unsigned short sys_width;
	unsigned short sys_height;
	char cmdbuf[1];
	int cmd;
	int n;
	int lines;
	char *buf;

	if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0) {
		if (w.ws_row > 0) {
			sys_height = w.ws_row;
		}
		if (w.ws_col > 0) {
			sys_width = w.ws_col;
		}
	}

	if ((buf = readfile(fp->_file, MAX)) == NULL) {
		perror("malloc");
		return;
	}

	lines = countlines(buf);

	putlines(buf, 0, sys_height - 1);
	n = sys_height - 1;

	while (n < lines) {
		read(tty, cmdbuf, 1);

		cmd = cmdchr(*cmdbuf);
		if (cmd == CMD_QUIT) {
			break;
		} else if (cmd == CMD_PNEXT) {
			putlines(buf, n, sys_height - 1);
			n += sys_height - 1;
		} else if (cmd == CMD_LNEXT) {
			putline(buf, n++);
		} else if (cmd == CMD_PPREV) {
			tputs_x(CL);

			if (n - (sys_height - 1) * 2 < 0) {
				n = 0;
			} else {
				n -= (sys_height - 1) * 2;
			}

			putlines(buf, n, sys_height - 1);
			n += sys_height - 1;
		} else if (cmd == CMD_LPREV) {
			if (n - sys_height < 0) {
				continue;
			}
			tputs_x(SC); // save pos
			tputs_x(HO); // set home pos
			tputs_x(SR); // scroll revrse
			putline(buf, n-- - sys_height);
			tputs_x(RC); // restore pos
			tputs_x(CE); // clr line
		}
	}

	free(buf);
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
		if (termcap() == -1) {
			fprintf(stderr, "failed to load termcap.");
		}

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
