## textscroll

A program that auto-scrolls files or command output on your terminal.

It supports many file types including:

    text, html, pdf, gzip, tar, zip, ar, bzip2, ms-word, nroff (man pages),
    executable, directory, .deb, .so, .rpm, piped output from other programs.

You can choose between different kinds of scroll modes and options, such
as changing the scroll speed, pausing, colors, search strings, case, and info.

If a file has extra blank lines they won't be shown, so that you're never 
staring at empty space.

Some examples of possible uses for textscroll:

- As a screen saver.

- Practice speed reading. Just set the program at a fast
  speed and see if you can keep up.

- Playing a musical instrument while reading sheet music.

- Read while doing exercises in front of your computer. Perhaps the what
  you're reading is pre-written fitness instructions such as right-punch,
  left-front-kick, right-side-kick, and so on.

## Requirements

textscroll comes with the 3rd party script "lesspipe.sh" that's used for
reading compressed files. You need to place this file in your path and make 
sure your environment variable $LESSOPEN points to it. Some systems come with 
lesspipe.sh already installe, so check to see if your version is up to date.

textscroll also uses the fmt(1) command to word wrap files. You need the fmt
command installed because this is turned on by default.

## Usage

The filename must come first on the command-line if you have other
options, unless you use the -f flag. For example:

    Correct:   ./textscroll file.txt -options
    Incorrect: ./textscroll -options file.txt
    Correct:   ./textscroll -options -f file.txt

To scroll a log file and highlight any lines containing the string 'lincoln':

    $ ./textscroll log.txt -w lincoln

To scroll a PDF file very slowly and in all uppercase letters:

    $ ./textscroll /stories/moby_dick.pdf -s 5000 -u

You can even scroll the output of other programs by piping to textscroll.
To do this you need to pass the name of the current tty you're running
textscroll from. To get the name of the tty just type the command tty(1).
It reports the full path to the current ttyname such as '/dev/tty0'.

    $ dmesg | textscroll -t /dev/tty0

There's an alternate mode (and more modes coming soon) in textscroll that lets 
you scroll letter by letter instead of line by line. Just use the -m flag:

    $ ./textscroll file.txt -m 

When you're viewing a non-text file, like a pdf or an html file, textscroll
renders it so its viewable. If you hit 'e' to open the file in your external
$EDITOR you will be editing the actual source of the file. For instance: 

    $ ./textscroll index.html
   
will scroll the html as you'd see it while viewing in a (text-based) web browser.

Change colors by using "-c <color>". Choose from:

    red, bgred, blue, bgblue, green, bggren, yellow, bgyellow, magenta,
    bgmagenta, bgwhite and bgblack. 

The string 'bg' in a color, e.g., bgblack, means 'background'.

Character at a time viewing mode:

    $ ./textscroll file.txt -m -c bgblue -s 100

Scroll URLs:

    $ lynx -dump http://site.com/page.html > page.txt ; textscroll page.txt

Get a full list of commands.

    $ ./textscroll -h


While textscroll is running you can use the following option keys:

    'q' to quit
    'p' to pause
    'spacebar' to scroll very fast (hit again to slow back down)
    'c' to clear the current screen
    'n' to turn off the status bar
    'v' to turn the status bar back on
    'f' to speed scrolling up in 25 percent increments
    's' to slow scrolling speed down in 25 percent increments
    'o' to go back to the original scroll speed
    'a' to toggle Auto-pausing on/off. Off by default. Can pause on highlights.
    'e' to open the file you're scrolling in an external editor. (Uses the 
        environment variables $VISUAL then $EDITOR or /bin/vi if none are set)
    'i' to view detailed file/program/etc information
