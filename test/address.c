#include <alloca.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "alsa/asoundlib.h"



#define INPUT_CAPS (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)
#define OUTPUT_CAPS (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)
#define IN_OUT_CAPS ((INPUT_CAPS) | (OUTPUT_CAPS))

// #define CLIENT_PREFIX_LEN    3


typedef struct port_spec {
	const char	*name;
	int			 req_id,	// create with expicit id if !=-1
				 id,		// actual id, either req_id or assigned
				 substr;	// test substrings of name
	unsigned	 caps;		// capabilities
} port_spec_t;

int		verbose = 0,
		test_count = 0,
		pass_count = 0,
		fail_count = 0;

// for testing different client and port specification formats 
char	*separators[] = {":", "."},		// client:port vs client.port
		*quotes[] = {"", "'", "\""};	// string vs 'string' vs "string"

port_spec_t	tester_ports[] = {
	{"zero",	-1,		0,	0,	IN_OUT_CAPS	},
};

port_spec_t	sequential_ports[] = {
	{"zero",	-1,		0,	0,	INPUT_CAPS	},
	{"One",		-1,		0,	0,	OUTPUT_CAPS	},
	{"two"	,	-1,		0,	0,	IN_OUT_CAPS	},
	{"three",	-1,		0,	1,	OUTPUT_CAPS	},
	{"FOUR"	,	-1,		0,	0,	INPUT_CAPS	},
	{"five"	,	-1,		0,	0,	IN_OUT_CAPS	},
};

port_spec_t	prefix_ports[] = {
	{"a",		-1,		0,	0,	IN_OUT_CAPS	},
	{"b",		-1,		0,	0,	INPUT_CAPS	},
	{"c"	,	-1,		0,	0,	OUTPUT_CAPS	},
};

port_spec_t	random_ports[] = {
	{"THREE",	 3,		0,	0,	OUTPUT_CAPS	},
	{"six",		 6,		0,	0,	INPUT_CAPS	},
	{"Eight",	 8,		0,	0,	IN_OUT_CAPS	},
	{"ele",		12,		0,	0,	INPUT_CAPS	},	// exact match
	{"lev",		13,		0,	0,	INPUT_CAPS	},	//   "     "
	{"eleven",	11,		0,	0,	INPUT_CAPS	},
	{"eve",		14,		0,	0,	INPUT_CAPS	},	// exact match
	{"ven"	,	15,		0,	0,	INPUT_CAPS	},	//   "     "
	{"twenTY",	20,		0,	0,	OUTPUT_CAPS	},
};




snd_seq_t* create_client(
const char	*name)
{
	snd_seq_t		*client;

	if (snd_seq_open(&client, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "snd_seq_open() failed\n");
		exit(EXIT_FAILURE);
	}

	if (snd_seq_set_client_name(client, name) < 0) {
		fprintf(stderr, "snd_seq_set_client_name() failed\n");
		exit(EXIT_FAILURE);
	}

	return client;
}


int create_port(
snd_seq_t	*client,
const char	*name,
int			 req_id,
unsigned	 caps)
{
	snd_seq_port_info_t *port_info;
	int	client_id;
	int	port_id;
	
	if (req_id < 0) { 
		// Let ALSA automatically assign next unused id
		if ((port_id = snd_seq_create_simple_port(client, name, caps, 0)) < 0) {
			fprintf(stderr,
					"snd_seq_create_simple_port(client, %s, 0x%x, 0) failed\n",
					name,
					caps);
			exit(EXIT_FAILURE);
		}
		return port_id;
	}

	// create port with specified id
	//

	if ((client_id = snd_seq_client_id(client)) < 0) {
		fprintf(stderr, "snd_seq_get_client_id() failed\n");
		exit(EXIT_FAILURE);
	}

	snd_seq_port_info_alloca(&port_info);
	snd_seq_port_info_set_client(port_info, client_id);
	snd_seq_port_info_set_name(port_info, name);
	snd_seq_port_info_set_capability(port_info, caps);
	snd_seq_port_info_set_type(port_info,
							     SND_SEQ_PORT_TYPE_MIDI_GENERIC
							   | SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_port_info_set_port(port_info, req_id);
	snd_seq_port_info_set_port_specified(port_info, 1);

	if (snd_seq_create_port(client, port_info) < 0) {
		fprintf(stderr,
				"snd_seq_create_port(,%s,%d,) failed\n",
				name,
				req_id);
		exit(1);
	}

	port_id = snd_seq_port_info_get_port(port_info);

	if (port_id != req_id) {
		int client_id = snd_seq_client_id(client);
		fprintf(stderr,
				"snd_seq_create_port(), \"%d.%s\" request id %d, got %d\n",
				client_id,
				name,
				req_id,
				port_id);
	 	exit(EXIT_FAILURE);
	}

	return port_id;
}


void create_ports(
snd_seq_t		*client,
port_spec_t		*ports,
unsigned		 count)
{
	unsigned 		 ndx;
	port_spec_t		*spec = ports;
	int				 id;
	
	for (ndx = 0 ; ndx < count ; ++ndx, ++spec) {
		id = create_port(client, spec->name, spec->req_id, spec->caps);
		spec->id = id;
	}
}


void parse_address(
snd_seq_t	*tester,
const char	*string,
const int	 client_id,
const int	 port_id,
int			 expect_error)
{
	snd_seq_addr_t 	addr;
	int				error;

	addr.client = 0;
	addr.port   = 0;

	error = snd_seq_parse_address(tester, &addr, string);

	// snd_seq_parse_address() returns negative errnos
	if (error < 0 && -error != expect_error) {
		fprintf(stderr,
				"snd_seq_parse_address(%s) %s(%d)",
				string,
				strerror(-error),
				error);
		if (expect_error != 0)
			fprintf(stderr,
					" (not expected %s(%d))\n",
					strerror(expect_error),
					-expect_error);
		else
			fprintf(stderr, "\n");
		exit(EXIT_FAILURE);
	}

	if (   error == expect_error
		&& (addr.client != client_id || addr.port != port_id)) {
		fprintf(stderr,
				"snd_seq_parse_address(%s) returned (%d,%d) "
				"not expected (%d,%d)\n",
				string,
				addr.client,
				addr.port,
				client_id,
				port_id);
		exit(EXIT_FAILURE);
	}


	if (verbose) {
		fprintf(stdout, "%s -> %d.%d", string, addr.client, addr.port);
		if (error == 0)
			fprintf(stdout, "\n");
		else
			fprintf(stdout,
				    "  (with expected error: %s(%d))%s)\n",
					strerror(-error),
					error,
					tester ? "" : " (because snd_seq_parse_address(NULL,,)");
	}

	if (error == 0) ++pass_count;
	else			++fail_count;
	++test_count;

}



void check_addresses(
snd_seq_t		*tester,
const char		*client_name,
int			 	 client_id,
port_spec_t		*ports,
unsigned		 count)
{
#define NO_ERROR 0
#define STRING_SIZE 80
	char			 string[STRING_SIZE + 1];
	unsigned 		 tests_bits,
					 port_ndx;
	port_spec_t		*port = ports;
	
#define PARSE(TESTER, FORMAT, OPEN,CLIENT, SEP, PORT, CLOSE, ERROR)		\
	{																	\
		snprintf(string,		\
				 STRING_SIZE,	\
				 (FORMAT),		\
				 (OPEN),		\
				 (CLIENT),		\
				 (SEP),			\
				 (PORT),		\
				 (CLOSE));		\
		parse_address((TESTER), string, client_id, port->id, (ERROR));	\
	}

	for (tests_bits = 0 ; tests_bits < (1<<5) ; ++tests_bits) {
		char		*quote,
					*separator;
		unsigned	 no_ports,
					 no_error,
					 no_error_no_client;
		snd_seq_t	*parser_client;

		if (tests_bits & 1) separator = ":";
		else				separator = ".";

		switch ((tests_bits >> 1) & 0x3) {
			case 0x0: quote = "";   break;
			case 0x1: quote = "'";  break;		
			case 0x2: quote = "\""; break;		
			default:  continue;     break;
		}

		no_ports = !(tests_bits << 3);

		if (tests_bits & (1<<4)) {
			parser_client = NULL;
			no_error = EINVAL; 
			no_error_no_client = EINVAL;
		}
		else {
			parser_client = tester;
			no_error = NO_ERROR;
			no_error_no_client = ENOENT;
		}

		// test client without port
		//

		// test by id
		snprintf(string, STRING_SIZE, "%s%d%s", quote, client_id, quote);
		parse_address(parser_client, string, client_id, 0, no_error);

		// test by name
		snprintf(string, STRING_SIZE, "%s%s%s", quote, client_name, quote);
		parse_address(parser_client, string, client_id, 0, no_error);

		// test bad client
		//
		snprintf(string, STRING_SIZE, "%s256%s", quote, quote);
		parse_address(parser_client, string, 255, 0, EINVAL);

		snprintf(string, STRING_SIZE, "%s-1%s", quote, quote);
		parse_address(parser_client, string, 255, 0, EINVAL);

		snprintf(string, STRING_SIZE, "%snonexistent_client%s", quote, quote);
		parse_address(parser_client, string, 0, 0, no_error_no_client);

		if (no_ports)
			continue;

		// test by id.id
		for (port_ndx = 0, port = ports ;
			 port_ndx < count ;
			 ++port_ndx, ++port)
			PARSE(parser_client,
				  "%s%d%s%d%s",
				  quote,
				  client_id,
				  separator,
				  port->id,
				  quote,
				  no_error);

		// test by name.id
		for (port_ndx = 0, port = ports ;
			 port_ndx < count ; 
			 ++port_ndx, ++port)
			PARSE(parser_client,
				  "%s%s%s%d%s",
				  quote,
				  client_name,
				  separator,
				  port->id,
				  quote,
				  no_error);

		// test by id.name
		for (port_ndx = 0, port = ports ;
			 port_ndx < count ;
			 ++port_ndx, ++port)
			PARSE(parser_client,
				  "%s%d%s%s%s",
				  quote,
				  client_id,
				  separator,
				  port->name,
				  quote,
				  no_error);

		// test by name.name
		for (port_ndx = 0, port = ports ;
			 port_ndx < count ;
			 ++port_ndx, ++port)
			PARSE(parser_client,
				  "%s%s%s%s%s",
				  quote,
				  client_name,
				  separator,
				  port->name,
				  quote,
				  no_error);

		for (port_ndx = 0, port = ports ;
		     port_ndx < count ;
			 ++port_ndx, ++port) {
			if (port->substr) {
				if (port->substr) {
					// test substrings of port name
#define SUBSTR_LEN  3
					char	    sub[SUBSTR_LEN + 1];
					unsigned	char_ndx;
					for (char_ndx = 0 ;
						 char_ndx <= strlen(port->name) - SUBSTR_LEN ;
						 ++char_ndx) {
						strncpy(sub, port->name + char_ndx, SUBSTR_LEN);
						sub[SUBSTR_LEN] = '\0';

						PARSE(parser_client,
							  "%s%d%s%s%s",
							  quote,
							  client_id,
							  separator,
							  sub,
							  quote,
							  no_error);
						PARSE(parser_client,
							  "%s%s%s%s%s",
							  quote,
							  client_name,
							  separator,
							  sub,
							  quote,
							  no_error);
					}
				}
			}
		}

		// test bad client and (good) port
		//
		snprintf(string,
				 STRING_SIZE,
				 "%s256%s0%s",
				 quote,
				 separator,
				 quote);
		parse_address(parser_client, string, 255, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				 "%s-1%s0%s",
				 quote,
				 separator,
				 quote);
		parse_address(parser_client, string, 255, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				 "%snonexistent_client%s0%s",
				 quote,
				 separator,
				 quote);
		parse_address(parser_client, string, 0, 0, no_error_no_client);

		// test client and bad port
		//
		snprintf(string,
				 STRING_SIZE,
				  "%s%d%s256%s",
				  quote,
				  client_id,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				  "%s%s%s256%s",
				  quote,
				  client_name,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				  "%s%d%s-1%s",
				  quote,
				  client_id,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				  "%s%s%s-1%s",
				  quote,
				  client_name,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				  "%s%d%snonexistent_port%s",
				  quote,
				  client_id,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);

		snprintf(string,
				 STRING_SIZE,
				  "%s%s%snonexistent_port%s",
				  quote,
				  client_name,
				  separator,
				  quote);
		parse_address(parser_client, string, 0, 0, EINVAL);
	}
}



int main(
int		 argc,
char	*argv[])
{
	snd_seq_t	*tester		= create_client("tester"),
				*sequential	= create_client("sequential"),
				*prefix		= create_client("seq"),
				*random		= create_client("random");
	int			 test_id	= snd_seq_client_id(tester),
				 seqn_id	= snd_seq_client_id(sequential),
				 prfx_id	= snd_seq_client_id(prefix),
				 rand_id	= snd_seq_client_id(random);
	int			 pause      = 0,
				 option;

	while ((option = getopt_long(argc, argv, "vph", NULL, NULL)) != -1) {
		switch (option) {
		case 'v':
			verbose = 1;
			break;
		case 'p':
			pause = 1;
			break;
		default:
			fprintf(stderr, "Usage: %s [-v] [-p] [-h]\n", argv[0]);
			fprintf(stderr, "       -v  print successful tests\n"
							"       -p  pause after tests until killed\n"
							"       -h  this help message\n");
			exit(1);
		}
	}

	int	test_ports_count = sizeof(tester_ports)     / sizeof(port_spec_t),
		seqn_ports_count = sizeof(sequential_ports) / sizeof(port_spec_t),
		prfx_ports_count = sizeof(prefix_ports)     / sizeof(port_spec_t),
		rand_ports_count = sizeof(random_ports)     / sizeof(port_spec_t);

	create_ports(tester, 	 tester_ports, 	   test_ports_count);
	create_ports(sequential, sequential_ports, seqn_ports_count);
	create_ports(prefix,	 prefix_ports, 	   prfx_ports_count);
	create_ports(random, 	 random_ports, 	   rand_ports_count);
	
	check_addresses(tester,
					"tester",
					test_id,
					tester_ports,
					test_ports_count);
	check_addresses(tester,
					"sequential",
					seqn_id,
					sequential_ports,
					seqn_ports_count);
	check_addresses(tester,
					"seq",
					prfx_id,
					prefix_ports,
					prfx_ports_count);
	check_addresses(tester,
					"random",
					rand_id,
					random_ports,
					rand_ports_count);
	check_addresses(tester,		// test specify client by prefix of name
					"ran",
					rand_id,
					random_ports,
					rand_ports_count);

	if (verbose || pause)
		fprintf(stdout,
				"%d tests\n%d passed\n%d failed with expected error\n",
				test_count,
				pass_count,
				fail_count);

	if (pause) {
		fprintf(stdout, "`aconnect -oil` in separate shell for clients+ports, "
						"type ^C here to exit\n");
		while(1);
	}

	snd_seq_close(random);
	snd_seq_close(sequential);
	snd_seq_close(tester);

	return 0;
}
