/*
 * Copyright (C) 1994-2016 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */
/**
 * @file	qalter.c
 * @brief
 * 	qalter - (PBS) alter batch job
 *
 * @author  Terry Heidelberg
 * 			Livermore Computing
 *
 * @author	Bruce Kelly
 *      	National Energy Research Supercomputer Center
 *
 * @author	Lawrence Livermore National Laboratory
 *      	University of California
 */

#include "cmds.h"
#include "pbs_ifl.h"
#include <pbs_config.h>   /* the master config generated by configure */
#include <pbs_version.h>


/**
 * @brief
 * 	prints usage format for qalter command
 *
 * @return - Void
 *
 */
static void
print_usage()
{
	static char usag2[]="       qalter --version\n";
	static char usage[]=
		"usage: qalter [-a date_time] [-A account_string] [-c interval] [-e path]\n"
	"\t[-h hold_list] [-j y|n] [-k keep] [-J X-Y[:Z]] [-l resource_list]\n"
	"\t[-m mail_options] [-M user_list] [-N jobname] [-o path] [-p priority]\n"
	"\t[-r y|n] [-S path] [-u user_list] [-W dependency_list] [-P project_name] job_identifier...\n";
	fprintf(stderr, "%s", usage);
	fprintf(stderr, "%s", usag2);
}

/**
 * @brief
 * 	handles attribute errors and prints appropriate errmsg 
 *
 * @param[in] connect - value indicating server connection
 * @param[in] err_list - list of possible attribute errors
 * @param[in] id - corresponding id(string) for attribute error
 *
 * @return - Void
 *
 */
static void
handle_attribute_errors(int connect,
	struct ecl_attribute_errors *err_list, char *id)
{
	struct attropl *attribute;
	char * opt;
	int i;

	for (i=0; i<err_list->ecl_numerrors; i++) {
		attribute = err_list->ecl_attrerr[i].ecl_attribute;
		if (strcmp(attribute->name, ATTR_a) == 0)
			opt="a";
		else if (strcmp(attribute->name, ATTR_A) == 0)
			opt="A";
		else if (strcmp(attribute->name, ATTR_project) == 0)
			opt = "P";
		else if (strcmp(attribute->name, ATTR_c) == 0)
			opt="c";
		else if (strcmp(attribute->name, ATTR_e) == 0)
			opt="e";
		else if (strcmp(attribute->name, ATTR_h) == 0)
			opt="h";
		else if (strcmp(attribute->name, ATTR_j)==0)
			opt="j";
		else if (strcmp(attribute->name, ATTR_k)==0)
			opt="k";
		else if (strcmp(attribute->name, ATTR_l)==0) {
			opt="l";
			fprintf(stderr, "qalter: %s %s\n",
				err_list->ecl_attrerr[i].ecl_errmsg, id);
			exit(err_list->ecl_attrerr[i].ecl_errcode);
		}
		else if (strcmp(attribute->name, ATTR_m)==0)
			opt="m";
		else if (strcmp(attribute->name, ATTR_M)==0)
			opt="M";
		else if (strcmp(attribute->name, ATTR_N)==0)
			opt="N";
		else if (strcmp(attribute->name, ATTR_o)==0)
			opt="o";
		else if (strcmp(attribute->name, ATTR_p)==0)
			opt="p";
		else if (strcmp(attribute->name, ATTR_r)==0)
			opt="r";
		else if (strcmp(attribute->name, ATTR_S)==0)
			opt="S";
		else if (strcmp(attribute->name, ATTR_u)==0)
			opt="u";
		else if ((strcmp(attribute->name, ATTR_depend)==0) ||
			(strcmp(attribute->name, ATTR_stagein)==0) ||
			(strcmp(attribute->name, ATTR_stageout)==0) ||
			(strcmp(attribute->name, ATTR_sandbox)==0) ||
			(strcmp(attribute->name, ATTR_umask) == 0) ||
			(strcmp(attribute->name, ATTR_runcount) == 0) ||
			(strcmp(attribute->name, ATTR_g)==0))
			opt="W";
		else
			return;

		pbs_disconnect(connect);
		CS_close_app();

		if (err_list->ecl_attrerr->ecl_errcode == PBSE_JOBNBIG)
                        fprintf(stderr, "qalter: Job %s \n", err_list->ecl_attrerr->ecl_errmsg);
                else {
			fprintf(stderr, "qalter: illegal -%s value\n", opt);
			print_usage();
		}
		exit(2);
	}
}


int
main(int argc, char **argv, char **envp) /* qalter */
{
	int c;
	int errflg=0;
	int any_failed=0;
	char *pc;
	int i;
	struct attrl *attrib = NULL;
	char *keyword;
	char *valuewd;
	char *erplace;
	time_t after;
	char a_value[80];

	char job_id[PBS_MAXCLTJOBID];

	char job_id_out[PBS_MAXCLTJOBID];
	char server_out[MAXSERVERNAME];
	char rmt_server[MAXSERVERNAME];
	struct ecl_attribute_errors *err_list;
#ifdef WIN32
	struct attrl *ap = NULL;
	short int nSizeofHostName = 0;
	char* orig_apvalue = NULL;
	char* temp_apvalue = NULL;
#endif

#define GETOPT_ARGS "a:A:c:e:h:j:k:l:m:M:N:o:p:r:S:u:W:P:"

	/*test for real deal or just version and exit*/

	execution_mode(argc, argv);

#ifdef WIN32
	winsock_init();
#endif

	while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
		switch (c) {
			case 'a':
				if ((after = cvtdate(optarg)) < 0) {
					fprintf(stderr, "qalter: illegal -a value\n");
					errflg++;
					break;
				}
				sprintf(a_value, "%ld", (long)after);
				set_attr(&attrib, ATTR_a, a_value);
				break;
			case 'A':
				set_attr(&attrib, ATTR_A, optarg);
				break;
			case 'P':
				set_attr(&attrib, ATTR_project, optarg);
				break;
			case 'c':
				while (isspace((int)*optarg)) optarg++;
				pc = optarg;
				if ((pc[0] == 'u') && (pc[1] == '\0')) {
					fprintf(stderr, "qalter: illegal -c value\n");
					errflg++;
					break;
				}
				set_attr(&attrib, ATTR_c, optarg);
				break;
			case 'e':
				set_attr(&attrib, ATTR_e, optarg);
				break;
			case 'h':
				while (isspace((int)*optarg)) optarg++;
				set_attr(&attrib, ATTR_h, optarg);
				break;
			case 'j':
				set_attr(&attrib, ATTR_j, optarg);
				break;
			case 'k':
				set_attr(&attrib, ATTR_k, optarg);
				break;
			case 'l':
				if ((i = set_resources(&attrib, optarg, TRUE, &erplace)) != 0) {
					if (i > 1) {
						pbs_prt_parse_err("qalter: illegal -l value\n", optarg,
							erplace-optarg, i);

					} else
						fprintf(stderr, "qalter: illegal -l value\n");
					errflg++;
				}
				break;
			case 'm':
				while (isspace((int)*optarg)) optarg++;
				set_attr(&attrib, ATTR_m, optarg);
				break;
			case 'M':
				set_attr(&attrib, ATTR_M, optarg);
				break;
			case 'N':
				set_attr(&attrib, ATTR_N, optarg);
				break;
			case 'o':
				set_attr(&attrib, ATTR_o, optarg);
				break;
			case 'p':
				while (isspace((int)*optarg)) optarg++;
				set_attr(&attrib, ATTR_p, optarg);
				break;
			case 'r':
				if (strlen(optarg) != 1) {
					fprintf(stderr, "qalter: illegal -r value\n");
					errflg++;
					break;
				}
				if (*optarg != 'y' && *optarg != 'n') {
					fprintf(stderr, "qalter: illegal -r value\n");
					errflg++;
					break;
				}
				set_attr(&attrib, ATTR_r, optarg);
				break;
			case 'S':
				set_attr(&attrib, ATTR_S, optarg);
				break;
			case 'u':
				set_attr(&attrib, ATTR_u, optarg);
				break;
			case 'W':
				while (isspace((int)*optarg)) optarg++;
				if (strlen(optarg) == 0) {
					fprintf(stderr, "qalter: illegal -W value\n");
					errflg++;
					break;
				}
#ifdef WIN32
				back2forward_slash2(optarg);
#endif
				i = parse_equal_string(optarg, &keyword, &valuewd);
				while (i == 1) {
					set_attr(&attrib, keyword, valuewd);
					i = parse_equal_string((char *)0, &keyword, &valuewd);
				}
				if (i == -1) {
					fprintf(stderr, "qalter: illegal -W value\n");
					errflg++;
				}
				break;
			case '?':
			default :
				errflg++;
				break;
		}

	if (errflg || optind == argc) {
		print_usage();
		exit(2);
	}

	/*perform needed security library initializations (including none)*/

	if (CS_client_init() != CS_SUCCESS) {
		fprintf(stderr, "qalter: unable to initialize security library.\n");
		exit(1);
	}

	for (; optind < argc; optind++) {
		int connect;
		int stat=0;
		int located = FALSE;

		strcpy(job_id, argv[optind]);
		if (get_server(job_id, job_id_out, server_out)) {
			fprintf(stderr, "qalter: illegally formed job identifier: %s\n", job_id);
			any_failed = 1;
			continue;
		}
cnt:
		connect = cnt2server(server_out);
		if (connect <= 0) {
			fprintf(stderr, "qalter: cannot connect to server %s (errno=%d)\n",
				pbs_server, pbs_errno);
			any_failed = pbs_errno;
			continue;
		}

		stat = pbs_alterjob(connect, job_id_out, attrib, NULL);
		if (stat && (pbs_errno != PBSE_UNKJOBID)) {
			if ((err_list = pbs_get_attributes_in_error(connect)))
				handle_attribute_errors(connect, err_list, job_id_out);

			prt_job_err("qalter", connect, job_id_out);
			any_failed = pbs_errno;
		} else if (stat && (pbs_errno == PBSE_UNKJOBID) && !located) {
			located = TRUE;
			if (locate_job(job_id_out, server_out, rmt_server)) {
				pbs_disconnect(connect);
				strcpy(server_out, rmt_server);
				goto cnt;
			}
			prt_job_err("qalter", connect, job_id_out);
			any_failed = pbs_errno;
		}

		pbs_disconnect(connect);
	}
	CS_close_app();
	exit(any_failed);
}
