/*-
 * Copyright (c) 2011 Ganael LAPLANCHE <ganael.laplanche@martymac.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "fpart.h"
#include "types.h"
#include "utils.h"
#include "options.h"
#include "partition.h"
#include "file_entry.h"
#include "dispatch.h"

/* NULL */
#include <stdlib.h>

/* fprintf(3), fopen(3), fclose(3), fgets(3), foef(3) */
#include <stdio.h>

/* getopt(3) */
#include <unistd.h>
#if !defined(__SunOS_5_9)
#include <getopt.h>
#endif

/* strlen(3) */
#include <string.h>

/* bzero(3) */
#include <strings.h>

/* errno */
#include <errno.h>

/* assert(3) */
#include <assert.h>

/* Print version */
void
version(void)
{
    fprintf(stderr, "fpart v" FPART_VERSION "\n"
        "Copyright (c) 2011 Ganael LAPLANCHE <ganael.laplanche@martymac.org>\n"
        "WWW: http://contribs.martymac.org\n");
    fprintf(stderr, "Build options: debug=");
#if defined(DEBUG)
    fprintf(stderr, "yes, fts=");
#else
    fprintf(stderr, "no, fts=");
#endif
#if defined(EMBED_FTS)
    fprintf(stderr, "embedded\n");
#else
    fprintf(stderr, "system\n");
#endif
}

/* Print usage */
void
usage(void)
{
    fprintf(stderr, "Usage: fpart [-h] [-V] -n num | -f files | -s size "
        "[-i infile] [-a]\n"
        "             [-o outfile] [-d depth] [-e] [-z] [-Z] [-v] [-D] [-L] "
        "[-w cmd] [-W cmd]\n"
        "             [-l] [-x] [-p num] [-q num] [-r num] "
        "[file(s) or dir(s) ...]\n");
    fprintf(stderr, "Sort and divide files into partitions.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "General:\n");
    fprintf(stderr, "  -h\tthis help\n");
    fprintf(stderr, "  -V\tprint version\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Partition control:\n");
    fprintf(stderr, "  -n\tset number of desired partitions\n");
    fprintf(stderr, "  -f\tlimit files per partition\n");
    fprintf(stderr, "  -s\tlimit partition size\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Input control:\n");
    fprintf(stderr, "  -i\tinput file (stdin if '-' is specified)\n");
    fprintf(stderr, "  -a\tinput contains arbitrary values\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output control:\n");
    fprintf(stderr, "  -o\toutput file template "
        "(stdout if '-' is specified)\n");
    fprintf(stderr, "  -d\tswitch to directory names display "
        "after certain <depth>\n");
    fprintf(stderr, "  -e\tadd ending slash to directory names\n");
    fprintf(stderr, "  -z\tinclude empty directories (default: include files "
        "only)\n");
    fprintf(stderr, "  -Z\ttreat un-readable directories as empty "
        "(implies -z)\n");
    fprintf(stderr, "  -v\tverbose mode (may be specified more than once)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Behaviour:\n");
    fprintf(stderr, "  -D\tgroup leaf directories as single file entries (implies -z)\n");
    fprintf(stderr, "  -L\tenable live mode\n");
    fprintf(stderr, "  -w\tpre-partition hook (live mode only)\n");
    fprintf(stderr, "  -W\tpost-partition hook (live mode only)\n");
    fprintf(stderr, "  -l\tfollow symbolic links\n");
    fprintf(stderr, "  -x\tdo not cross file system boundaries "
        "(default: cross)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Size handling:\n");
    fprintf(stderr, "  -p\tpreload each partition with num bytes\n");
    fprintf(stderr, "  -q\toverload each file with num bytes\n");
    fprintf(stderr, "  -r\tround each file size up to next num bytes "
        "multiple\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Example: fpart -n 3 -o var-parts /var\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Please report bugs to Ganael LAPLANCHE "
        "<ganael.laplanche@martymac.org>\n");
    return;
}

/* Handle one argument (either a path to crawl or an arbitrary
   value) and update file entries (head)
   - returns != 0 if a critical error occurred
   - returns with head set to the last element added
   - updates totalfiles with the number of elements added */
int
handle_argument(char *argument, fnum_t *totalfiles, struct file_entry **head,
    struct program_options *options)
{
    assert(argument != NULL);
    assert(totalfiles != NULL);
    assert(head != NULL);
    assert(options != NULL);

    if(options->arbitrary_values == OPT_ARBITRARYVALUES) {
    /* handle arbitrary values */
        fsize_t input_size = 0;
        char *input_path = NULL;
        input_path  = malloc(strlen(argument) + 1);
        if(input_path == NULL) {
            fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
            return (1);
        }

        if(sscanf(argument, "%lld %[^\n]", &input_size, input_path) == 2) {
            if(handle_file_entry(head, input_path, input_size, options) == 0)
                (*totalfiles)++;
            else {
                fprintf(stderr, "%s(): cannot add file entry\n", __func__);
                free(input_path);
                return (1);
            }
        }
        else
            fprintf(stderr, "error parsing input values: %s\n", argument);

        /* cleanup */
        free(input_path);
    }
    else {
    /* handle paths, must examine filesystem */
        char *input_path = NULL;
        size_t input_path_len = strlen(argument);
        size_t malloc_size = input_path_len + 1;
        input_path  = malloc(malloc_size);
        if(input_path == NULL) {
            fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
            return (1);
        }
        snprintf(input_path, malloc_size, "%s", argument);
    
        /* remove multiple ending slashes */
        while((input_path_len > 1) &&
            (input_path[input_path_len - 1] == '/')  &&
            (input_path[input_path_len - 2] == '/')) {
            input_path[input_path_len - 1] = '\0';
            input_path_len--;
        }

        /* crawl path */
        if(input_path[0] != '\0') {
#if defined(DEBUG)
            fprintf(stderr, "init_file_entries(): examining %s\n",
                input_path);
#endif
            if(init_file_entries(input_path, head, totalfiles, options) != 0) {
                fprintf(stderr, "%s(): cannot initialize file entries\n",
                    __func__);
                free(input_path);
                return (1);
            }
        }

        /* cleanup */
        free(input_path);
    }

    return (0);
}

int main(int argc, char** argv)
{
    fnum_t totalfiles = 0;

/******************
  Handle options
 ******************/

    /* Program options */
    struct program_options options;

    /* Set default options */
    init_options(&options);

    /* Options handling */
    extern char *optarg;
    extern int optind;
    int ch;
    while((ch = getopt(argc, argv, "?hVn:f:s:i:ao:d:ezZvDLw:W:lxp:q:r:")) !=
        -1) {
        switch(ch) {
            case '?':
            case 'h':
                usage();
                uninit_options(&options);
                return (0);
                break;
            case 'V':
                version();
                uninit_options(&options);
                return (0);
                break;
            case 'n':
            {
                char *endptr = NULL;
                long num_parts = strtol(optarg, &endptr, 10);
                /* refuse values <= 0 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') ||
                    (num_parts <= 0)) {
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.num_parts = (pnum_t)num_parts;
                break;
            }
            case 'f':
            {
                char *endptr = NULL;
                long long max_entries = strtoll(optarg, &endptr, 10);
                /* refuse values <= 0 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') ||
                    (max_entries <= 0)) {
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.max_entries = (fnum_t)max_entries;
                break;
            }
            case 's':
            {
                char *endptr = NULL;
                long long max_size = strtoll(optarg, &endptr, 10);
                /* refuse values <= 0 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') || (max_size <= 0)) {
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.max_size = (fsize_t)max_size;
                break;
            }
            case 'i':
            {
                /* check for empty argument */
                if(strlen(optarg) == 0)
                    break;
                /* replace previous filename if '-i' specified multiple times */
                if(options.in_filename != NULL)
                    free(options.in_filename);
                options.in_filename = abs_path(optarg);
                if(options.in_filename == NULL) {
                    fprintf(stderr, "%s(): cannot determine absolute path for "
                        "file '%s'\n", __func__, optarg);
                    uninit_options(&options);
                    return (1);
                }
                break;
            }
            case 'a':
                options.arbitrary_values = OPT_ARBITRARYVALUES;
                break;
            case 'o':
            {
                /* check for empty argument */
                if(strlen(optarg) == 0)
                    break;
                /* replace previous filename if '-o' specified multiple times */
                if(options.out_filename != NULL)
                    free(options.out_filename);
                /* '-' goes to stdout */
                if((optarg[0] == '-') && (optarg[1] == '\0')) {
                    options.out_filename = NULL;
                } else {
                    options.out_filename = abs_path(optarg);
                    if(options.out_filename == NULL) {
                        fprintf(stderr, "%s(): cannot determine absolute path "
                            "for file '%s'\n", __func__, optarg);
                        uninit_options(&options);
                        return (1);
                    }
                }
                break;
            }
            case 'd':
            {
                char *endptr = NULL;
                long dir_depth = strtol(optarg, &endptr, 10);
                /* refuse values < 0 (-1 being used to disable this option) */
                if((endptr == optarg) || (*endptr != '\0') || (dir_depth < 0)) {
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.dir_depth = (int)dir_depth;
                break;
            }
            case 'e':
                options.add_slash = OPT_ADDSLASH;
                break;
            case 'z':
                options.empty_dirs = OPT_EMPTYDIRS;
                break;
            case 'Z':
                options.dnr_empty = OPT_DNREMPTY;
                options.empty_dirs = OPT_EMPTYDIRS;
                break;
            case 'v':
                options.verbose++;
                break;
            case 'D':
                options.leaf_dirs = OPT_LEAFDIRS;
                options.empty_dirs = OPT_EMPTYDIRS;
                break;
            case 'L':
                options.live_mode = OPT_LIVEMODE;
                break;
            case 'w':
            {
                /* check for empty argument */
                size_t malloc_size = strlen(optarg) + 1;
                if(malloc_size <= 1)
                    break;
                /* replace previous hook if '-w' specified multiple times */
                if(options.pre_part_hook != NULL)
                    free(options.pre_part_hook);
                options.pre_part_hook = malloc(malloc_size);
                if(options.pre_part_hook == NULL) {
                    fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
                    uninit_options(&options);
                    return (1);
                }
                snprintf(options.pre_part_hook, malloc_size, "%s", optarg);
                break;
            }
            case 'W':
            {
                /* check for empty argument */
                size_t malloc_size = strlen(optarg) + 1;
                if(malloc_size <= 1)
                    break;
                /* replace previous hook if '-W' specified multiple times */
                if(options.post_part_hook != NULL)
                    free(options.post_part_hook);
                options.post_part_hook = malloc(malloc_size);
                if(options.post_part_hook == NULL) {
                    fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
                    uninit_options(&options);
                    return (1);
                }
                snprintf(options.post_part_hook, malloc_size, "%s", optarg);
                break;
            }
            case 'l':
                options.follow_symbolic_links = OPT_FOLLOWSYMLINKS;
                break;
            case 'x':
                options.cross_fs_boundaries = OPT_NOCROSSFSBOUNDARIES;
                break;
            case 'p':
            {
                char *endptr = NULL;
                long long preload_size = strtoll(optarg, &endptr, 10);
                /* refuse values <= 0 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') ||
                    (preload_size <= 0)) {
                    fprintf(stderr,
                        "Option -p requires a value greater than 0.\n");
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.preload_size = (fsize_t)preload_size;
                break;
            }
            case 'q':
            {
                char *endptr = NULL;
                long long overload_size = strtoll(optarg, &endptr, 10);
                /* refuse values <= 0 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') ||
                    (overload_size <= 0)) {
                    fprintf(stderr,
                        "Option -q requires a value greater than 0.\n");
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.overload_size = (fsize_t)overload_size;
                break;
            }
            case 'r':
            {
                char *endptr = NULL;
                long long round_size = strtoll(optarg, &endptr, 10);
                /* refuse values <= 1 and partially converted arguments */
                if((endptr == optarg) || (*endptr != '\0') ||
                    (round_size <= 1)) {
                    fprintf(stderr,
                        "Option -r requires a value greater than 1.\n");
                    usage();
                    uninit_options(&options);
                    return (1);
                }
                options.round_size = (fsize_t)round_size;
                break;
            }
        }
    }
    argc -= optind;
    argv += optind;

    /* check for options consistency */
    if((options.num_parts == DFLT_OPT_NUM_PARTS) &&
        (options.max_entries == DFLT_OPT_MAX_ENTRIES) &&
        (options.max_size == DFLT_OPT_MAX_SIZE)) {
        fprintf(stderr, "Please specify either -n, -f or -s.\n");
        usage();
        uninit_options(&options);
        return (1);
    }

    if((options.num_parts != DFLT_OPT_NUM_PARTS) &&
        ((options.max_entries != DFLT_OPT_MAX_ENTRIES) ||
                    (options.max_size != DFLT_OPT_MAX_SIZE) ||
                    (options.live_mode != DFLT_OPT_LIVEMODE))) {
        fprintf(stderr,
            "Option -n is incompatible with options -f, -s and -L.\n");
        usage();
        uninit_options(&options);
        return (1);
    }

    if((options.live_mode == OPT_NOLIVEMODE) &&
        ((options.pre_part_hook != NULL) ||
        (options.post_part_hook != NULL))) {
        fprintf(stderr,
            "Hooks can only be used with option -L.\n");
        usage();
        uninit_options(&options);
        return (1);
    }

    if((options.in_filename == NULL) && (argc <= 0)) {
        /* no file specified, force stdin */
        char *opt_input = "-";
        size_t malloc_size = strlen(opt_input) + 1;
        options.in_filename = malloc(malloc_size);
        if(options.in_filename == NULL) {
            fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
            uninit_options(&options);
            return (1);
        }
        snprintf(options.in_filename, malloc_size, "%s", opt_input);
    }

/**************
  Handle stdin
***************/

    /* our main double-linked file list */
    struct file_entry *head = NULL;

    if(options.verbose >= OPT_VERBOSE)
        fprintf(stderr, "Examining filesystem...\n");

    /* work on each file provided through input file (or stdin) */
    if(options.in_filename != NULL) {
        /* handle fd opening */
        FILE *in_fp = NULL;
        if((options.in_filename[0] == '-') &&
            (options.in_filename[1] == '\0')) {
            /* working from stdin */
            in_fp = stdin;
        }
        else {
            /* working from a filename */
            if((in_fp = fopen(options.in_filename, "r")) == NULL) {
                fprintf(stderr, "%s: %s\n", options.in_filename,
                    strerror(errno));
                uninit_options(&options);
                return (1);
            }
        }

        /* read fd and do the work */
        char line[MAX_LINE_LENGTH];
        char *line_end_p = NULL;
        bzero(line, MAX_LINE_LENGTH);
        while(fgets(line, MAX_LINE_LENGTH, in_fp) != NULL) {
            /* replace '\n' with '\0' */
            if ((line_end_p = strchr(line, '\n')) != NULL)
                *line_end_p = '\0';

            if(handle_argument(line, &totalfiles, &head, &options) != 0) {
                uninit_file_entries(head, &options);
                uninit_options(&options);
                return (1);
            }

            /* cleanup */
            bzero(line, MAX_LINE_LENGTH);
        }

        /* check for error reading input */
        if(ferror(in_fp) != 0) {
            fprintf(stderr, "error reading from input stream\n"); 
        }

        /* cleanup */
        if(in_fp != NULL)
            fclose(in_fp);
    }

/******************
  Handle arguments
*******************/

    /* now, work on each path provided as arguments */
    unsigned int i;
    for(i = 0 ; i < argc ; i++) {
        if(handle_argument(argv[i], &totalfiles, &head, &options) != 0) {
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }
    }

/****************
  Display status
*****************/

    /* come back to the first element */
    rewind_list(head);

    /* no file found or live mode */
    if((totalfiles <= 0) || (options.live_mode == OPT_LIVEMODE)) {
        uninit_file_entries(head, &options);
        /* display status */
        fprintf(stderr, "%lld file(s) found.\n", totalfiles);
        uninit_options(&options);
        return (0);
    }

    /* display status */
    fprintf(stderr, "%lld file(s) found.\n", totalfiles);

    if(options.verbose >= OPT_VERBOSE)
        fprintf(stderr, "Sorting entries...\n");

/************************************************
  Sort entries with a fixed number of partitions
*************************************************/

    /* our list of partitions */
    struct partition *part_head = NULL;
    pnum_t num_parts = options.num_parts;

    /* sort files with a fixed size of partitions */
    if(options.num_parts != DFLT_OPT_NUM_PARTS) {
        /* create a fixed-size array of pointers to sort */
        struct file_entry **file_entry_p = NULL;

        file_entry_p =
            malloc(sizeof(struct file_entry *) * totalfiles);

        if(file_entry_p == NULL) {
            fprintf(stderr, "%s(): cannot allocate memory\n", __func__);
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }

        /* initialize array */
        init_file_entry_p(file_entry_p, totalfiles, head);
    
        /* sort array */
        qsort(&file_entry_p[0], totalfiles, sizeof(struct file_entry *),
            &sort_file_entry_p);
    
        /* create a double_linked list of partitions
           which will hold dispatched files */
        if(add_partitions(&part_head, options.num_parts, &options) != 0) {
            fprintf(stderr, "%s(): cannot init list of partitions\n",
                __func__);
            uninit_partitions(part_head);
            free(file_entry_p);
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }
        /* come back to the first element */
        rewind_list(part_head);
    
        /* dispatch files */
        if(dispatch_file_entry_p_by_size
            (file_entry_p, totalfiles, part_head, options.num_parts) != 0) {
            fprintf(stderr, "%s(): unable to dispatch file entries\n",
                __func__);
            uninit_partitions(part_head);
            free(file_entry_p);
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }
    
        /* re-dispatch empty files */
        if(dispatch_empty_file_entries
            (head, totalfiles, part_head, options.num_parts) != 0) {
            fprintf(stderr, "%s(): unable to dispatch empty file entries\n",
                __func__);
            uninit_partitions(part_head);
            free(file_entry_p);
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }

        /* cleanup */
        free(file_entry_p);
    }

/***************************************************
  Sort entries with a variable number of partitions
****************************************************/

    /* sort files with a file number or size limit per-partitions.
       In this case, partitions are dynamically-created */
    else {
        if((num_parts = dispatch_file_entries_by_limits
            (head, &part_head, options.max_entries, options.max_size,
            &options)) == 0) {
            fprintf(stderr, "%s(): unable to dispatch file entries\n",
                __func__);
            uninit_partitions(part_head);
            uninit_file_entries(head, &options);
            uninit_options(&options);
            return (1);
        }
        /* come back to the first element
           (we may have exited with part_head set to partition 1, 
           after default partition) */
        rewind_list(part_head);
    }

/***********************
  Print result and exit
************************/

    /* print result summary */
    print_partitions(part_head);

    if(options.verbose >= OPT_VERBOSE)
        fprintf(stderr, "Writing output lists...\n");

    /* print file entries */
    print_file_entries(head, options.out_filename, num_parts);

    if(options.verbose >= OPT_VERBOSE)
        fprintf(stderr, "Cleaning up...\n");

    /* free stuff */
    uninit_partitions(part_head);
    uninit_file_entries(head, &options);
    uninit_options(&options);
    return (0);
}
