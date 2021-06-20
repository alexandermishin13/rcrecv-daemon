/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2021 Alexander Mishin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * GPIORCRECV - Remote control receiver module over GPIO.
 *
 * Remote control receiver module can't be discovered automatically, please
 * specify hints as part of loader or kernel configuration:
 *	hint.rcrecv.0.at="gpiobus0"
 *	hint.rcrecv.0.pins=<PIN>
 *
 * Or configure via FDT data.
 */

#include <sys/event.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>

#include <libutil.h>
#include <libgpio.h>

#include <dev/rcrecv/rcrecv.h>

#define DEVICE_GPIOC  "/dev/gpioc0"
#define DEVICE_RCRECV "/dev/rcrecv"

char *dev_rcrecv = DEVICE_RCRECV;
char *dev_gpio = DEVICE_GPIOC;
int dev;
gpio_handle_t gpioc;
struct pidfh *pfh;
bool background = false;
unsigned long interval = 1000; // Minimal interval between codes is 1s

typedef struct rcc_entry {
    unsigned long code;
    gpio_pin_t pin;
    char state;

    SLIST_ENTRY(rcc_entry) switches;
} *rcc_entry_t;

struct rcc_list search_switch;

SLIST_HEAD(rcc_list, rcc_entry);

static void
usage()
{
    fprintf(stderr, "usage: %s [-d <ctldev>] [-g <gpioc>] [-i <ms>]"
	"-(s|u|t) code=<code>,pin=<pin> [-b] [-h]\n\n",
	getprogname());
    fprintf(stderr,
	"Options:\n"
	"    -d, --device=<ctldev>\n"
	"                        A remote control receiver device name.\n"
	"                        Default: /dev/rcrecv;\n"
	"    -g, --gpio=<gpioc>  A gpio controller device name\n"
	"                        Default: /dev/gpioc0;\n"
	"    -i, --interval=<ms> A minimal valid interval between repeated codes\n"
	"                        If an interval is less than that value the next same ones\n"
	"                        will be ignored. Default value is 1000ms;"
	"    -s, --set code=<code>,pin=<pin>\n"
	"    -u, --unset code=<code>,pin=<pin>\n"
	"    -u, --toggle code=<code>,pin=<pin>\n"
	"                        A way which the <pin> should be changed after the\n"
	"                        <code> is received;\n"
	"    -b,                 Run in background as a daemon;\n"
	"    -h, --help          Print this help.\n"
    );

}

/* Signals handler. Prepare the programm for end */
static void
termination_handler(int signum)
{
    /* Free allocated memory blocks */
    while (!SLIST_EMPTY(&search_switch)) {           /* List Deletion. */
	rcc_entry_t node = SLIST_FIRST(&search_switch);
	SLIST_REMOVE_HEAD(&search_switch, switches);
	free(node);
    }

    /* Close devices */
    close(dev);
    gpio_close(gpioc);

    /* Remove pidfile and exit */
    pidfile_remove(pfh);

    exit(EXIT_SUCCESS);
}

/* Daemonize the program */
static void
daemonize(void)
{
    pid_t otherpid;

    /* Try to create a pidfile */
    pfh = pidfile_open(NULL, 0600, &otherpid);
    if (pfh == NULL)
    {
	if (errno == EEXIST)
	    errx(EXIT_FAILURE, "Daemon already running, pid: %jd.", (intmax_t)otherpid);

	/* If we cannot create pidfile from other reasons, only warn. */
	warn("Cannot open or create pidfile");
	/*
	 * Even though pfh is NULL we can continue, as the other pidfile_*
	 * function can handle such situation by doing nothing except setting
	 * errno to EDOOFUS.
	 */
    }

    /* Try to demonize the process */
    if (daemon(0, 0) == -1)
    {
	pidfile_remove(pfh);
	errx(EXIT_FAILURE, "Cannot daemonize");
    }

    pidfile_write(pfh);
}

/* Adds new code map entry */
static rcc_entry_t
search_rcc_entry(const unsigned long *code)
{
    rcc_entry_t node, tmpnode = NULL;

    SLIST_FOREACH(node, &search_switch, switches)
    {
	printf("code: %lu, pin: %u, state: %c\n", node->code, node->pin, node->state);
	if (node->code == *code) {
	    tmpnode = node;
	    break;
	}
    }

    return node;
}

/* Adds new code map entry */
static rcc_entry_t
add_rcc_entry(rcc_entry_t curnode)
{
    rcc_entry_t newnode;

    newnode = (rcc_entry_t)malloc(sizeof(*newnode));
    if (newnode == NULL)
	err(EXIT_FAILURE, "Unable to malloc symbol_node\n");

    if (curnode == NULL)
	SLIST_INSERT_HEAD(&search_switch, newnode, switches);
    else if (SLIST_NEXT(curnode, switches) == NULL)
	SLIST_INSERT_AFTER(curnode, newnode, switches);
    else
	err(EXIT_FAILURE, "Unable to add a switch entry\n");

    return newnode;
}

/* Get and decode params */
static void
get_param(int argc, char **argv)
{
    int ch, long_index = 0;
    rcc_entry_t node;
    extern char *optarg, *suboptarg;
    char *options, *value, *end;

    char *subopts[] = {
#define CODE		0
		"code",
#define PIN		1
		"pin",
	NULL
    };

    static struct option long_options[] = {
//        {"status", no_argument,       0, 'v' },
	{"device",   required_argument, 0, 'd' },
	{"gpio",     required_argument, 0, 'g' },
	{"interval", required_argument, 0, 'i' },
	{"set",      required_argument, 0, 's' },
	{"unset",    required_argument, 0, 'u' },
 	{"toggle",   required_argument, 0, 't' },
	{"help",     required_argument, 0, 'h' },
	{0, 0, 0, 0}
    };

    SLIST_INIT(&search_switch);
    node = SLIST_FIRST(&search_switch);

    while ((ch = getopt_long(argc, argv, "d:s:t:u:bh",long_options,&long_index)) != -1) {
	switch (ch) {
	case 'd':
	    dev_rcrecv = optarg;
	    break;
	case 'g':
	    dev_gpio = optarg;
	    break;
	case 'i':
	    interval = strtoul(optarg, &end, 0);
	    break;
	case 'b':
	    background = true;
	    break;
	case 's':
	    /* FALLTHROUGH */
	case 'u':
	    /* FALLTHROUGH */
	case 't':
	    node = add_rcc_entry(node);

	    /* Set a pin state */
	    node->state = ch;

	    options = optarg;
	    while (*options) {
		switch(getsubopt(&options, subopts, &value)) {
		case CODE:
		    if (!value)
			errx(EXIT_FAILURE, "no code");
		    node->code = strtoul(value, &end, 0);
		    break;
		case PIN:
		    if (!value)
			errx(EXIT_FAILURE, "no pin number");
		    node->pin = strtoul(value, &end, 0);
		    break;
		case -1:
		    if (suboptarg)
			errx(EXIT_FAILURE, "illegal sub option %s", suboptarg);
		    else
			errx(EXIT_FAILURE, "no code neither pin number");
		    break;
		}
	    }
	    break;
	case 'h':
	    /* FALLTHROUGH */
	default:
	    usage();
	    exit(EXIT_SUCCESS);
	}
    }
    argv += optind;
    argc -= optind;
}

int
main(int argc, char **argv)
{
    rcc_entry_t node;

    struct timespec timeout;
    const size_t waitms = 10000;
    int64_t last_time = 0;
    unsigned long last_code = 0;

    struct kevent event;    /* Event monitored */
    struct kevent tevent;   /* Event triggered */
    int kq, ret;
    struct rcrecv_code rcc;

    get_param(argc, argv);

    /* Set a timeout by 'waitms' value */
    timeout.tv_sec = waitms / 1000;
    timeout.tv_nsec = (waitms % 1000) * 1000 * 1000;

    /* Open GPIO controller */
    gpioc = gpio_open_device(dev_gpio);
    //gpio_pin_output(handle, 16);
    if (gpioc == GPIO_INVALID_HANDLE)
	errx(EXIT_FAILURE, "Failed to open '%s'", dev_gpio);

    /* Open RCRecv device */
    dev = open(dev_rcrecv, O_RDONLY);
    if (dev < 0)
	errx(EXIT_FAILURE, "Failed to open '%s'", dev_rcrecv);

    /* Create kqueue. */
    kq = kqueue();
    if (kq == -1)
	err(EXIT_FAILURE, "kqueue() failed");

    /* Initialize kevent structure. */
    EV_SET(&event, dev, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
    /* Attach event to the kqueue. */
    ret = kevent(kq, &event, 1, NULL, 0, NULL);
    if (ret == -1)
	err(EXIT_FAILURE, "kevent register");
    if (event.flags & EV_ERROR)
	errx(EXIT_FAILURE, "Event error: %s", strerror(event.data));

    /* If there are dirty kevents read and drop irrelevant data */
    ret = kevent(kq, NULL, 0, &tevent, 1, &timeout);
    if (ret == -1) {
	err(EXIT_FAILURE, "kevent wait");
    }
    else if (ret > 0) {
	ioctl(dev, RCRECV_READ_CODE_INFO, &rcc);
    }

    /* Unbinds from terminal if '-b' */
    if (background) {
	daemonize();

	/* Create kqueue for child */
	kq = kqueue();
	if (kq == -1)
	    err(EXIT_FAILURE, "kqueue() failed");

	/* Initialize kevent structure once more */
	EV_SET(&event, dev, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
	/* and  once more attach event to the kqueue. */
	ret = kevent(kq, &event, 1, NULL, 0, NULL);
	if (ret == -1)
	    err(EXIT_FAILURE, "kevent register");
	if (event.flags & EV_ERROR)
	    errx(EXIT_FAILURE, "Event error: %s", strerror(event.data));
    }

    /* Intercept signals to our function */
    if (signal (SIGINT, termination_handler) == SIG_IGN)
	signal (SIGINT, SIG_IGN);
    if (signal (SIGTERM, termination_handler) == SIG_IGN)
	signal (SIGTERM, SIG_IGN);

    for (;;) {
	/* Sleep until a code received */
	ret = kevent(kq, NULL, 0, &tevent, 1, &timeout);
	if (ret == -1) {
	    err(EXIT_FAILURE, "kevent wait");
	}
	else if (ret > 0) {
	    /* Get a code from remote control
	       and search a node for it */
	    ioctl(dev, RCRECV_READ_CODE_INFO, &rcc);
	    node = search_rcc_entry(&rcc.value);
	    /* No actions if the same code is repeated too fast */
	    if (node != NULL &&
	       (last_code != rcc.value ||
		llabs(last_time - rcc.last_time) > (interval * 1000)))
	    {
		/* Config a pin from the node for output before change it */
		gpio_pin_output(gpioc, node->pin);
		/* Change state of the pin as set in the node */
		switch(node->state) {
		case 's':
		    gpio_pin_high(gpioc, node->pin);
		    break;
		case 'u':
		    gpio_pin_low(gpioc, node->pin);
		    break;
		case 't':
		    gpio_pin_toggle(gpioc, node->pin);
		    break;
		}
		last_time = rcc.last_time;
		last_code = rcc.value;
		syslog(LOG_INFO, "Receiving code 0x%lX: %c %u\n",
			node->code, node->state, node->pin);
	    }
	}
    }
}
