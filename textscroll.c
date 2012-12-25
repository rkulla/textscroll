/* textscroll.c  -by Ryan Kulla 
   E-Mail: rkulla@gmail.com 
   Last modified: Feb 2nd 2003 
   License: GPL */

#include <curses.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define BUFMAX 1024
#define OFF 0
#define ON 1
#define UC 1 /* upper case */
#define LC 2 /* lower case */
#define CR 13 /* carriage return */
#define LF 10 /* line feed */

struct my_windows {
    WINDOW *scrollwin;
    WINDOW *statwin;
} *pstat, *pscroll;

struct {
    char *filename;
    char *display_filename;
    char *tty_name;
    FILE *fp;
    FILE *which;
    unsigned long int the_file_size;
    unsigned int piped;
    float percent;
    unsigned long int page_num;
    char homedir[BUFMAX];
    char text_file[BUFMAX];
    char text_pipe[BUFMAX];
    char text_tmp[BUFMAX];
} lfile = { NULL, NULL, NULL, NULL, NULL, 0, 1, 0, 1 };

struct text_options {
    unsigned long int default_speed;
    char *special_word;
    char *editor;
    char *use_color;
    unsigned int want_color;
    unsigned int y;
    unsigned int pos_changed;
    unsigned int view_normal;
    unsigned int case_change;
    unsigned int case_type;
    unsigned int scrollmode_chars;
    unsigned int statusbar;
    unsigned int auto_pause;
    unsigned int beep_ok;
    unsigned int reshow_statusbar;
    unsigned int highlight;
    unsigned int new_speed;
} lop = { 1000, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 };  
  
SCREEN *STDIN_SCREEN;
char *progname;

void scan_command_line(int, char **);
void text_colors(void);
void scroll_it(unsigned int, int, char *);
void usage(void);
void quit_cleanly(void);
void get_stats(unsigned long int, unsigned long int);
void highlight_word(char *, unsigned int, unsigned int, unsigned int, unsigned long int, unsigned long int);
void user_input(unsigned int *, unsigned int, unsigned int *, unsigned long int, unsigned long int);
void do_options(unsigned int, int, char *);
unsigned int get_key(void);
int char_check(char *);
long line_nums(FILE *);
void change_case(char *, int);
void char_scroll(unsigned int);
void show_info(unsigned int, unsigned int, unsigned long int, unsigned long int);
int file_size(FILE *);
int check_if_pdf(char *);
void lesspipe(void);
void fmt(void);
void get_editor(void);
int start_editor(unsigned long int);
void strip_extra_blanks(void);
char *get_basename(char *);
char *str_trunc(char *, int);
void my_perror(char *);
void cperror(char *);
void catch_sigwinch(int signo);
void catch_sigint(int signo);
void signal_setup(void);
void create_windows(void);
void check_homedir(void);
void check_stdin(void);
FILE *open_tty(char *);

int main(int argc, char **argv)
{
    signal_setup();
    progname = argv[0];
    check_homedir();
    get_editor();
    scan_command_line(argc, argv);
    unlink(lfile.text_file);
    if (lfile.piped)
        unlink(lfile.text_pipe);
    clear();
    refresh();
    endwin();
    return 0;
}

void check_homedir(void)
{
char *home = getenv("HOME");

    if (home) {
        snprintf(lfile.homedir, (strlen(home) + 12), "%s/.textscroll/", home);
        if ((access(lfile.homedir, F_OK|W_OK)) != 0)
            if (mkdir(lfile.homedir, S_IRUSR|S_IWUSR|S_IXUSR) != 0) 
                my_perror(lfile.homedir); 
    }
    snprintf(lfile.text_file, sizeof lfile.text_file, "%stext_file", lfile.homedir);
    snprintf(lfile.text_tmp, sizeof lfile.text_tmp, "%stext_tmp", lfile.homedir);
    if (lfile.piped)
        snprintf(lfile.text_pipe, sizeof lfile.text_pipe, "%spiped", lfile.homedir);

}

void check_stdin(void)
{
/* do this before any initscr()'ing due to ncurses tty i/o handling
   when input is piped from another program */
char stdin_buf[BUFMAX];
FILE *fp, *input, *output;

    if (lfile.piped) {
        input = output = open_tty(lfile.tty_name); 

        if (!(fp = fopen(lfile.text_pipe, "w")))
            my_perror("fopen()");
        while (fgets(stdin_buf, sizeof stdin_buf, stdin)) {
            fprintf(fp, "%s", stdin_buf);
        }
        fclose(fp);
        STDIN_SCREEN = newterm((char *)0, output, input);
    }
}

FILE *open_tty(char *tty_path)
{
FILE *fp;
struct stat sb;

    if (stat(tty_path, &sb) < 0)
        my_perror(tty_path);
    if ((sb.st_mode & S_IFMT) != S_IFCHR) {
        errno = ENOTTY;
        my_perror(tty_path);
    }

    if (!(fp = fopen(tty_path, "a+")))
        my_perror("fopen()");
    if (fp == 0)
        my_perror(tty_path);

    return fp;
}

void create_windows(void)
{
    initscr();
    if (lfile.piped) {
        set_term(STDIN_SCREEN); /* switch to a real tty */
        refresh();
        endwin();
    }
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    curs_set(FALSE);
    if (lop.want_color)
        text_colors();

    if (!(pscroll = (struct my_windows *)malloc(sizeof(struct my_windows)))) 
        cperror("malloc()");
   
    pscroll->scrollwin = newwin(LINES, 0, 0, 0);

    if (!(pstat = (struct my_windows *)malloc(sizeof(struct my_windows)))) 
        cperror("malloc()");

    pstat->statwin = newwin(1, COLS, LINES - 1, 0);
    refresh();
}

void scan_command_line(int argc, char **argv)
{
int optch, opt;
static char optstring[] = "s:p:f:w:c:navhbluxmt:";
char speed[20], position[3], *filename_nodashf;
unsigned int scroll_speed = lop.default_speed; 

    if (argc < 2)
        usage();
    if (argv[1])
        filename_nodashf = argv[1];
    for (opt = 1; opt < argc; opt++) {
        if (!strncmp(argv[opt], "-", 1))
            break;
        else { /* no options just filename */
            lfile.piped = FALSE;
            if (strlen(argv[opt]) > BUFMAX)
                usage();
            if (!(lfile.filename = (char *)malloc(strlen(argv[opt])+1)))
                my_perror("malloc()");
            strncpy(lfile.filename, argv[opt], strlen(argv[opt]));
            if (!(lfile.fp = fopen(lfile.filename, "r")))
                my_perror("fopen()");
        }
    }

    while ((optch = getopt(argc, argv, optstring)) != -1)
        switch (optch) {
            case 's':
                if (!char_check(optarg))
                    usage();
                strncpy(speed, optarg, 20);
                scroll_speed = (unsigned int)atoi(speed);
                break;
            case 'p':
                if (!char_check(optarg))
                    usage();
                strncpy(position, optarg, 3);
                lop.pos_changed = (unsigned int)atoi(position);
                break;
            case 'f':
                lfile.piped = FALSE;
                if (strlen(optarg) > BUFMAX)
                    usage();
                if (!(lfile.filename = (char *)malloc(strlen(optarg)+1)))
                    my_perror("malloc()");
                strncpy(lfile.filename, optarg, strlen(optarg));
                if (!(lfile.fp = fopen(lfile.filename, "r")))
                    my_perror("fopen()"); 
                break;
            case 't':
                if (!(lfile.tty_name = (char *)malloc(strlen(optarg)+1)))
                    my_perror("malloc()");
                strncpy(lfile.tty_name, optarg, strlen(optarg));
                break;
            case 'w':
                if (strlen(optarg) > BUFMAX)
                    usage();
                if (!(lop.special_word = (char *)malloc(strlen(optarg)+1))) 
                    my_perror("malloc()");
                strncpy(lop.special_word, optarg, strlen(optarg));
                break;
            case 'n':
                lop.view_normal = TRUE;
                break;
            case 'u':
                lop.case_type = UC;
                lop.case_change = TRUE;
                break;
            case 'l':
                lop.case_type = LC;
                lop.case_change = TRUE; 
                break; 
            case 'c':
                lop.want_color = 1;
                if (!(lop.use_color = (char *)malloc(strlen(optarg)+1)))
                    my_perror("malloc");
                strncpy(lop.use_color, optarg, strlen(optarg));
                break;
            case 'x':
                lop.statusbar = FALSE;
                break;
            case 'v':
                endwin();
                printf("textscroll -Ryan Kulla\n");
                exit(EXIT_SUCCESS);
                break;
            case 'h':
                usage();
                break;
            case 'a':
                lop.auto_pause = TRUE;
                break;
            case 'm':
                lop.scrollmode_chars = TRUE;
                break;
            case 'b':
                lop.beep_ok = TRUE;
                break;
            default:
                usage();
                break;
        }
        do_options(scroll_speed, argc, filename_nodashf);
}

void text_colors(void)
{
    if (has_colors() == FALSE)
        cperror("Your terminal does not support colors");

    start_color();
    if (!strncmp(lop.use_color, "red", 3)) 
        assume_default_colors(COLOR_RED, COLOR_BLACK);
    if (!strncmp(lop.use_color, "bgred", 5))
        assume_default_colors(COLOR_WHITE, COLOR_RED);
    if (!strncmp(lop.use_color, "bgblue", 6))
        assume_default_colors(COLOR_WHITE, COLOR_BLUE);
    if (!strncmp(lop.use_color, "blue", 4))
        assume_default_colors(COLOR_BLUE, COLOR_BLACK);
    if (!strncmp(lop.use_color, "green", 5))
        assume_default_colors(COLOR_GREEN, COLOR_BLACK);
    if (!strncmp(lop.use_color, "bggreen", 7))
        assume_default_colors(COLOR_BLUE, COLOR_GREEN);
    if (!strncmp(lop.use_color, "magenta", 7))
        assume_default_colors(COLOR_MAGENTA, COLOR_BLACK);
    if (!strncmp(lop.use_color, "bgmagenta", 9)) 
        assume_default_colors(COLOR_WHITE, COLOR_MAGENTA);
    if (!strncmp(lop.use_color, "yellow", 6))
        assume_default_colors(COLOR_YELLOW, COLOR_BLACK);
    if (!strncmp(lop.use_color, "bgyellow", 8))
        assume_default_colors(COLOR_WHITE, COLOR_YELLOW);
    if (!strncmp(lop.use_color, "bgwhite", 7))
        assume_default_colors(COLOR_BLACK, COLOR_WHITE);
    if (!strncmp(lop.use_color, "bgblack", 7))
        assume_default_colors(COLOR_WHITE, COLOR_BLACK);
}

void do_options(unsigned int scroll_speed, int argc, char *filename_nodashf)
{
    if (lfile.piped) {
        lfile.filename = lfile.text_pipe;
        check_stdin();
    }

    lesspipe();
    fmt();

    if (!lop.view_normal)
        strip_extra_blanks();

    create_windows();
    
    if (lfile.filename) 
        lfile.display_filename = str_trunc(get_basename(lfile.filename), 15);

    if (!lop.special_word)
        lop.special_word = "textscroll";

    if (scroll_speed) 
         scroll_it(scroll_speed, argc, filename_nodashf);
    else
        usage();
}

void lesspipe(void)
{
FILE *fp_read, *fp_write;
char *c, buf[BUFMAX], command[BUFMAX], lesscommand[BUFMAX], *lesspipe;
unsigned long int empty = 0;
char qfilename[BUFMAX]; /* quoted filename */

    printf("Loading...\n");
    /* $LESSOPEN will look like:  |/usr/bin/lesspipe.sh %s */
    lesspipe = getenv("LESSOPEN");
    if (lesspipe) {
        /* strip off the leading | */ 
        if ((c = strchr(lesspipe, '|'))) {
            strncpy(lesscommand, c + 1, strlen(c + 1));
            /* redirection */
            if (check_if_pdf(lfile.filename))
                strncat(lesscommand, " - > ", 5); /* pdftotext needs the - */
            else 
                strncat(lesscommand, " > ", 3);
            strncat(lesscommand, lfile.text_file, strlen(lfile.text_file));
            /* add quotes for filenames containing spaces: */
            snprintf(qfilename, sizeof qfilename, "\"%s\"", lfile.filename);
            snprintf(command, (strlen(lesscommand) + strlen(qfilename)), lesscommand, qfilename);
        }
        system(command); /* run lesspipe.sh */
    }

    if (!(fp_write = fopen(lfile.text_file, "r"))) 
        my_perror("fopen()");
    if (file_size(fp_write) == 0)
        empty = 1;
    fclose(fp_write);

    if (empty) {
    /* lesspipe didn't have any output, make the file ourselves */
        if (!(fp_read = fopen(lfile.filename, "r")))
            my_perror("fopen()");
        if (!(fp_write = fopen(lfile.text_file, "w")))
            my_perror("fopen()");
        while (fgets(buf, sizeof buf, fp_read)) {
            fprintf(fp_write, "%s", buf);
        }
        fclose(fp_read);
        fclose(fp_write);
    }
}

int check_if_pdf(char *filename)
{
char *c;

    if (strstr(filename, ".pdf")) {
        if ((c = strrchr(filename, '.')))
            if (!strncmp(c, ".pdf", 4))
                return 1;
    }

    return 0;
}

void fmt(void)
{
char command[BUFMAX];

    snprintf(command, sizeof command, "fmt -s %s > %s", lfile.text_file, lfile.text_tmp);
    system(command);

    if (unlink(lfile.text_file))
        my_perror("unlink()");
    if (link(lfile.text_tmp, lfile.text_file) != 0)
        my_perror("link()");
    if (unlink(lfile.text_tmp))
        my_perror("unlink()");
}

void strip_extra_blanks(void)
{
FILE *fp_read, *fp_write;
char buf[BUFMAX];
unsigned int blank = 0;

    if (!(fp_read = fopen(lfile.text_file, "r+"))) 
        my_perror("fopen()");
    if (!(fp_write = fopen(lfile.text_tmp, "w")))
        my_perror("fopen()");
    while (fgets(buf, sizeof buf, fp_read)) {
        /* Don't print 2+ blank lines */        
        if (buf[0] == CR || buf[0] == LF)
            blank++;
        else
            blank = 0;
        if (blank == 2) {
            blank = 1;
            continue;
        }
        fprintf(fp_write, "%s", buf);
    }
    fclose(fp_read);
    fclose(fp_write);
    if (unlink(lfile.text_file))
        my_perror("unlink()");
    if (link(lfile.text_tmp, lfile.text_file) != 0) 
        my_perror("link()");
    if (unlink(lfile.text_tmp))
        my_perror("unlink()");
}

void scroll_it(unsigned int scroll_speed, int argc, char *filename_nodashf)
{
char buf[BUFMAX], *s;
unsigned int origspeed = scroll_speed, toggle = ON;
unsigned long int total_lines = 0, line = 0;

    lop.y = LINES - 2;
    if (lop.pos_changed)
        lop.y = lop.pos_changed;
    if ((lop.y > LINES - 2) || (lop.y < 1)) 
        usage();

    if (!lfile.piped)  { /* if they used -f */
        if (!(lfile.fp = fopen(lfile.text_file, "r")))
            cperror("fopen()");
        lfile.which = lfile.fp;
        total_lines = line_nums(lfile.which);
    } else { 
        if (!(lfile.which = fopen(lfile.text_pipe, "r")))
            cperror("fopen()");
        lfile.display_filename = "piped output";
        total_lines = line_nums(lfile.which);
    }
    lfile.the_file_size = file_size(lfile.which);

    if (lop.scrollmode_chars) 
        char_scroll(scroll_speed);
    else {
        scrollok(pscroll->scrollwin, TRUE);
        while ((s = fgets(buf, sizeof(buf), lfile.which))) { /* scroll time */
            flushinp();
            line++;
            if (lop.statusbar)
                if (toggle) 
                    get_stats(total_lines, line);
            highlight_word(buf, scroll_speed, origspeed, toggle, total_lines, line);
            if (lop.case_change)
                change_case(buf, lop.case_type);
            mvwprintw(pscroll->scrollwin, lop.y, 0, "%s", buf);
            napms(scroll_speed);
            scroll(pscroll->scrollwin);
            wrefresh(pscroll->scrollwin);
            user_input(&scroll_speed, origspeed, &toggle, total_lines, line);
        } 
        get_stats(total_lines, line); /* see stats at eof */
        wgetch(pscroll->scrollwin);
        fclose(lfile.which);
    }
}

void char_scroll(unsigned int scroll_speed)
{
unsigned int i, line = 0;
char buf[BUFMAX], *s;
unsigned int origspeed = scroll_speed, toggle = ON;
unsigned long int total_lines = line_nums(lfile.which);
  
    while ((s = fgets(buf, sizeof(buf), lfile.which))) {
        line++;
        highlight_word(buf, scroll_speed, origspeed, toggle, total_lines, line);
        if (lop.case_change)
            change_case(buf, lop.case_type);
        if (lop.highlight) {
            lop.highlight = FALSE;
            wclear(pscroll->scrollwin);
            wrefresh(pscroll->scrollwin);
            continue;
        }
        for (i = 0; buf[i] != '\0'; i++) {
            if (lop.statusbar)
                if (toggle)
                    get_stats(total_lines, line);
            if (lop.reshow_statusbar) {
                get_stats(total_lines, line); /* see status bar */
                lop.reshow_statusbar = FALSE;
            }
            if (lop.pos_changed)
                mvwprintw(pscroll->scrollwin, lop.y, i, "%c", buf[i]);
            else
                mvwprintw(pscroll->scrollwin, lop.y / 2, i, "%c", buf[i]);
            napms(scroll_speed); 
            wrefresh(pscroll->scrollwin);
            user_input(&scroll_speed, origspeed, &toggle, total_lines, line);
        }
        wclear(pscroll->scrollwin);
        wrefresh(pscroll->scrollwin);
    } 
    get_stats(total_lines, line); /* see stats at eof */
    wgetch(pscroll->scrollwin);
    fclose(lfile.which);
}

int file_size(FILE *fp)
{
unsigned long int len = 0;

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    return(len);
}

void change_case(char *buf, int choice)
{
char *p;

    for (p = buf; *p != '\0'; p++) {
        if (choice == LC)
            *p = tolower(*p);
        else
            *p = toupper(*p);
    }
} 

void highlight_word(char *buf, unsigned int scroll_speed, unsigned int origspeed, unsigned int toggle, unsigned long int total_lines, unsigned long int line)
{
unsigned int i = 0;

    /* highlight the entire line special_word is on */
    if (strstr(buf, lop.special_word)) {
        lop.highlight = TRUE;
        wattrset(pscroll->scrollwin, A_BOLD);
        if (lop.beep_ok)
            beep(); 

        if (lop.scrollmode_chars) {
            /* print again to see correctly */
            for (i = 0; buf[i] != '\0'; i++) {
                if (lop.statusbar)
                    if (toggle)
                        get_stats(total_lines, line);
                if (lop.pos_changed)
                    mvwprintw(pscroll->scrollwin, lop.y, i, "%c", buf[i]);
                else
                    mvwprintw(pscroll->scrollwin, lop.y / 2, i, "%c", buf[i]);
                user_input(&scroll_speed, origspeed, &toggle, total_lines, line);
                napms(scroll_speed);
                wrefresh(pscroll->scrollwin);
            }
        }
        if (lop.auto_pause) {
            wrefresh(pscroll->scrollwin);
            nodelay(stdscr, FALSE);
            getch();
        }
    }
    else
        wattrset(pscroll->scrollwin, A_NORMAL);
}

void get_stats(unsigned long int total_lines, unsigned long int line)
{
time_t now;
struct tm *tmptr;
char sdate[64];

    time(&now);
    tmptr = localtime(&now);
    strftime(sdate, sizeof sdate, "%a %b %d  %I:%M:%S%p", tmptr);

    lfile.percent = (((float)line / (float)total_lines) * 100);

    wbkgd(pstat->statwin, A_REVERSE);

    if ((line % LINES) == 0)
        lfile.page_num++;
    mvwprintw(pstat->statwin, 0, 0, "%ld/%ld - %.0f%%  Page: %ld - %s", line,
total_lines, lfile.percent, lfile.page_num, lfile.display_filename);

    mvwprintw(pstat->statwin, 0, COLS - 23, "%s", sdate);

    wrefresh(pstat->statwin);
}

void get_editor(void)
{
    if (getenv("VISUAL"))
        lop.editor = getenv("VISUAL");
    else if (getenv("EDITOR"))
        lop.editor = getenv("EDITOR");

   if ((access(lop.editor, F_OK)) != 0) {
       if (!(access("/bin/vi", F_OK)))
           lop.editor = "/bin/vi";
       else
           lop.editor = "None";
    }
}

int start_editor(unsigned long int line)
{
char *editor_base, line_str[BUFMAX];
pid_t pid, wpid;
int status;

    if ((strncmp(lop.editor, "None", 4)) != 0) {
        editor_base = get_basename(lop.editor);
        snprintf(line_str, sizeof line_str, "+%ld", line);
        def_prog_mode();

        pid = fork();
        if (pid == -1) {
           cperror("fork()");
        } else if (pid == 0) {
            execlp(lop.editor, editor_base, line_str, lfile.filename, NULL);
            cperror("execlp()");
        } else {
            wpid = wait(&status);
            if (wpid == -1) 
                cperror("wait()");
            if (wpid != pid)
                exit(1);
        }
        clear();
        refresh();
        reset_prog_mode();
        refresh();
    }
    return 0;
}

char *get_basename(char *editor)
{
char *c;

    if ((c = strrchr(editor, '/')))
        return (c + 1);
    else
        return editor;
}

void user_input(unsigned int *scroll_speed, unsigned int origspeed, unsigned
int *toggle, unsigned long int total_lines, unsigned long int line)
{
    switch (get_key()) {
        case 1:
            if (*scroll_speed != 1) /* speed up */
                *scroll_speed = 1;
            else {
                if (lop.new_speed)
                    *scroll_speed = lop.new_speed;
                else
                    *scroll_speed = origspeed; /* stop speeding */
            }
            break;
        case 2:
            quit_cleanly();
            break;
        case 3:
            *scroll_speed = origspeed;
            lop.new_speed = 0;
            break;
        case 4:
            if (lop.statusbar)
                if (toggle)
                    get_stats(total_lines, line);
            nodelay(stdscr, FALSE);	
            getch();
            break;
        case 5:
            if (!lop.statusbar)
                lop.statusbar = ON;
            *toggle = ON;
            break;
        case 6:
            if (!lop.statusbar)
               break;
            *toggle = OFF;
            wbkgd(pstat->statwin, A_NORMAL);
            wclear(pstat->statwin);
            wrefresh(pstat->statwin);
            break;
        case 7:
            wclear(pscroll->scrollwin);
            wrefresh(pscroll->scrollwin);
            break;
        case 8: 
            *scroll_speed -= ((*scroll_speed * 25) / 100);
            lop.new_speed = *scroll_speed; 
            break;
        case 9:
            *scroll_speed += ((*scroll_speed * 25) / 100);
            lop.new_speed = *scroll_speed; 
            break;
        case 10:
            lop.auto_pause ^= 1; /* toggle */
            break;
        case 11:
            if (lop.beep_ok) 
                beep();
            show_info(*scroll_speed, origspeed, total_lines, line);
            break;
        case 12:
            start_editor(line);
            break;
    }
}

unsigned int get_key(void)
{
chtype key;

    nodelay(stdscr, TRUE);
    if ((key = getch()) != (unsigned long int)ERR)
        if (key == 32)  return 1;
        if (key == 'q') return 2;
        if (key == 'o') return 3;
        if (key == 'p') return 4;
        if (key == 'v') return 5;
        if (key == 'n') return 6;
        if (key == 'c') return 7;
        if (key == 'f') return 8;
        if (key == 's') return 9;
        if (key == 'a') return 10;
        if (key == 'i') return 11;
        if (key == 'e') return 12;

    return 0;
}

void show_info(unsigned int scroll_speed, unsigned int origspeed, unsigned long int total_lines, unsigned long int line)
{
char buf[BUFMAX], *current_dir;
char *header_msg = "textscroll [Press any key]";

    if (!getcwd(buf, BUFMAX)) 
        cperror("getcwd()");
    if (strlen(buf) > (COLS - 18))
        current_dir = str_trunc(buf, (COLS - 18));
    else
        current_dir = buf;

    def_prog_mode(); /* save the old screen */
    clear(); 
    refresh();
 
    box(stdscr, 0, 0);
    attrset(A_BOLD);
    mvprintw(0, ((COLS / 2) - (strlen(header_msg) / 2)), "%s", header_msg);
    attrset(A_NORMAL);
    mvhline(1, 1, ACS_HLINE, COLS - 2);
    attrset(A_BOLD); mvprintw(2, 1, "Filename: "); attrset(A_NORMAL);
    mvprintw(2, 11, "%s", get_basename(lfile.filename));
    attrset(A_BOLD); mvprintw(3, 1, "File Size: "); attrset(A_NORMAL);
    mvprintw(3, 12, "%ld bytes", lfile.the_file_size);
    attrset(A_BOLD); mvprintw(4, 1, "Current Dir: "); attrset(A_NORMAL);
    mvprintw(4, 14, "%s%c", current_dir, strlen(current_dir) < 2 ? ' ' : '/');
    attrset(A_BOLD); mvprintw(5, 1, "Current Line: "); attrset(A_NORMAL);
    mvprintw(5, 15, "%ld of %ld - %.0f%% - Page: %d", line, total_lines, lfile.percent, lfile.page_num);
    attrset(A_BOLD); mvprintw(6, 1, "Scroll Speed: "); attrset(A_NORMAL);
    mvprintw(6, 15, "(%d Milliseconds | %.2f Second%s", scroll_speed, (float)scroll_speed / 1000, scroll_speed > 1000 ? "s)" : ")");
    attrset(A_BOLD); mvprintw(7, 1, "Original Speed: "); attrset(A_NORMAL);
    mvprintw(7, 17, "(%d Milliseconds | %.2f Second%s", origspeed, (float)origspeed / 1000, origspeed > 1000 ? "%s)" : ")");
    attrset(A_BOLD); mvprintw(8, 1, "External Editor: "); attrset(A_NORMAL);
    mvprintw(8, 18, "%s", lop.editor);
    attrset(A_BOLD); mvprintw(9, 1, "Terminal Size: "); attrset(A_NORMAL);
    mvprintw(9, 16, "%d Rows, %d Columns", LINES, COLS);
    attrset(A_BOLD); mvprintw(10, 1, "Beep On Events: "); attrset(A_NORMAL);
    mvprintw(10, 17, "%s", lop.beep_ok ? "Yes" : "No");
    attrset(A_BOLD); mvprintw(11, 1, "Auto-Pause: "); attrset(A_NORMAL);
    mvprintw(11, 13, "%s", lop.auto_pause ? "Yes" : "No");
    attrset(A_BOLD); mvprintw(12, 1, "Skip Extra Blank Lines: "); attrset(A_NORMAL);
    mvprintw(12, 25, "%s", lop.view_normal ? "No" : "Yes");

    nodelay(stdscr, FALSE);
    getch();
    clear(); 
    refresh();
    reset_prog_mode(); /* Return the screen */
    refresh(); /* needed to show the screen */
    lop.reshow_statusbar = TRUE; 
}

void usage()
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("-f <filename>   File to scroll.\n"
    "-s <n>          Scroll at <n> milliseconds. Default: 1000 (1 sec).\n"
    "-p <n>          Start text on row <n> (1 to LINES-2).\n"
    "-w <string>     Highlight all lines that <string> appears on.\n"
    "-l              Display in all lower case characters.\n"
    "-u              Display in all upper case characters.\n"
    "-c <color>      red, bgred, green, bggreen, blue, bgblue, yellow, bgyellow, magenta, bgmagenta, bgwhite.\n"
    "-n              Don't take out extra blank lines. Display as is.\n" 
    "-a              Automatically pause on highlight word from -w.\n"
    "-v              Display program version number.\n"
    "-h              Help (This message).\n"
    "-x              Don't show status bar.\n"
    "-b              Allow beeping on important events.\n"
    "-m              Scroll a character at a time mode.\n"
    "-t <ttyname>    Name of tty your running textscroll from while piped\n"

    "\tWhile textscroll is running you can use the option keys:\n"
    "'q' to quit.\n'p' to pause.\n'spacebar' to scroll super"
    "fast (hit again to turn off).\n'c' to clear the current screen."
    "\n'n' to turn off the status bar.\n'v' to turn the status bar back on.\n"
    "'f' to speed scrolling up in 25 percent increments.\n's' to slow "
    "scrolling speed down in 25 percent increments.\n"
    "'o' to go back to original speed.\n'a' to toggle Auto-Pausing on/off.\n"
    "'e' open file in your editor. Uses $VISUAL, $EDITOR or /bin/vi.\n"
    "'i' to view detailed file/program/etc information.\n");

    exit(EXIT_SUCCESS);
}

void quit_cleanly(void)
{
    unlink(lfile.text_file);
    if (lfile.piped)
        unlink(lfile.text_pipe);
    clear();
    refresh();
    flushinp();
    endwin();
    exit(EXIT_SUCCESS);
}

int char_check(char *str)
{
/* checks if any non-digits are found in the input string */

    for (; *str; str++)
        if (!isdigit(*str))
            return 0;

    return 1;
}

long line_nums(FILE *fp)
{
char buf[255];
long linenum = 0;

    /* add up all the lines of the file */
    while (fgets(buf, sizeof(buf), fp))
        ++linenum;

    rewind(fp);

    return (linenum);
}

char *str_trunc(char *s, int n)
{
char *buf;

    if (n > strlen(s)) 
        n = strlen(s) + 1;

    if (!(buf = (char *)malloc(n + 1))) 
        cperror("malloc()");

    strncpy(buf, s, n);

    if (strlen(s) > strlen(buf))
        strncat(buf, "~", 1);

    return (buf);
}

void my_perror(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void cperror(char *msg)
{
    endwin(); /* kill the window first so we can print to stdout */
    perror(msg);
    quit_cleanly();
}

void catch_sigwinch(int signo)
{
    /* terminal resizing functionality */
    refresh();
    endwin();
    free(pscroll->scrollwin);
    free(pstat->statwin);
    create_windows();
    scrollok(pscroll->scrollwin, TRUE);
    lop.y = LINES - 2;
}

void catch_sigint(int signo)
{
    quit_cleanly();
}

void signal_setup(void)
{
struct sigaction sa_resize_old, sa_resize_new;
struct sigaction sa_kill_old, sa_kill_new;

    /* xterm resizing */
    sa_resize_new.sa_handler = catch_sigwinch;
    sigemptyset(&sa_resize_new.sa_mask);
    sa_resize_new.sa_flags = 0;
    sigaction(SIGWINCH, &sa_resize_new, &sa_resize_old);

    /* control-C */
    sa_kill_new.sa_handler = catch_sigint;
    sigemptyset(&sa_kill_new.sa_mask);
    sa_kill_new.sa_flags = 0;
    sigaction(SIGINT, &sa_kill_new, &sa_kill_old);
}
