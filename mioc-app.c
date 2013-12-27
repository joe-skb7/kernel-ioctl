/*
 * Masha IOCtl application.
 *
 * Copyright (C) 2013 Dark Engineering Initiative.
 * Author: Sam Protsenko <joe.skb7@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mioc.h"

#define DEV_FILE "/dev/mioc"

enum option {
	OPT_UNKNOWN,
	OPT_WRITE,
	OPT_ERASE
};

struct params {
	enum option option;
	char *msg_filename;	/* pointer to corresponding argv[] item */
};

static void print_usage(char *appname)
{
	fprintf(stderr, "Usage: %s OPTION\n\n", appname);
	fprintf(stderr, "  -e            erase message in MIOC driver\n");
	fprintf(stderr, "  -w FILE       write specified file content "
			"to MIOC driver\n");
}

static int parse_params(int argc, char *argv[], struct params *params)
{
	params->option = OPT_UNKNOWN;
	params->msg_filename = NULL;

	if (argc == 2) {
		if (strcmp(argv[1], "-e") == 0)
			params->option = OPT_ERASE;
		else
			goto err_parse;
	} else if (argc == 3) {
		if (strcmp(argv[1], "-w") == 0) {
			params->option = OPT_WRITE;
			params->msg_filename = argv[2];
		} else {
			goto err_parse;
		}
	} else {
		goto err_parse;
	}

	return 1;

err_parse:
	print_usage(argv[0]);
	return 0;
}

static int check_params(struct params * const params)
{
	if (params->option == OPT_WRITE) {
		/* Check if specified file exists */
		if (access(params->msg_filename, F_OK) == -1) {
			fprintf(stderr, "Specified file doesn't exist\n");
			return 0;
		}
	}

	return 1;
}

static void do_ioctl_write(int fd, char *msg_filename)
{
	FILE *msg_file;
	long msg_file_size;
	char *buf;

	msg_file = fopen(msg_filename, "r");
	if (msg_file == NULL) {
		perror("Error opening message file");
		return;
	}

	/* Obtain file size */
	fseek(msg_file, 0L, SEEK_END);
	msg_file_size = ftell(msg_file);
	rewind(msg_file);

	/* Allocate memory for entire content */
	buf = malloc(sizeof(char) * (msg_file_size + 1));
	if (!buf) {
		fprintf(stderr, "Memory allocation failed");
		goto err_buf;
	}

	/* Copy the file into the buffer */
	if (fread(buf, msg_file_size, 1, msg_file) != 1) {
		fprintf(stderr, "File reading failed");
		goto err_fread;
	}

	fclose(msg_file);

	if (ioctl(fd, MIOC_IOCWRITE, buf) == -1)
		perror("Error occurred on write ioctl");

	free(buf);

	return;

err_fread:
	free(buf);
err_buf:
	fclose(msg_file);
}

static void do_ioctl_erase(int fd)
{
	if (ioctl(fd, MIOC_IOCERASE) == -1)
		perror("Error occurred on erase ioctl");
}

int main(int argc, char *argv[])
{
	struct params params;
	int fd;

	if (!parse_params(argc, argv, &params))
		return EXIT_FAILURE;
	if (!check_params(&params))
		return EXIT_FAILURE;


	fd = open(DEV_FILE, O_WRONLY);
	if (fd == -1) {
		perror("Failed to open MIOC dev file");
		return EXIT_FAILURE;
	}

	switch (params.option) {
	case OPT_WRITE:
		do_ioctl_write(fd, params.msg_filename);
		break;
	case OPT_ERASE:
		do_ioctl_erase(fd);
		break;
	default:
		fprintf(stderr, "Unknown option\n");
		return EXIT_FAILURE;
	}

	close(fd);

	return EXIT_SUCCESS;
}
