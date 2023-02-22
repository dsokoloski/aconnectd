#include <string>
#include <iostream>

#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdarg>
#include <clocale>

#include <getopt.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <alsa/asoundlib.h>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;

static void acd_error(const char *file, int line, const char *function,
	int err, const char *fmt, ...)
{
	va_list arg;

	if (err == ENOENT) return;

	va_start(arg, fmt);
	fprintf(stderr, "ALSA lib %s:%i:(%s) ", file, line, function);
	vfprintf(stderr, fmt, arg);
	if (err)
		fprintf(stderr, ": %s", snd_strerror(err));
	putc('\n', stderr);
	va_end(arg);
}

static void acd_get_port(snd_seq_t *seq, snd_seq_client_info_t *cinfo,
	snd_seq_port_info_t *pinfo, int count)
{
	if (! count) {
		int card = -1, pid = -1;

		printf("client %d: '%s' [type=%s",
		       snd_seq_client_info_get_client(cinfo),
		       snd_seq_client_info_get_name(cinfo),
		       (snd_seq_client_info_get_type(cinfo) == SND_SEQ_USER_CLIENT ?
			"user" : "kernel")
		);

		card = snd_seq_client_info_get_card(cinfo);
		if (card != -1)
			printf(",card=%d", card);

		pid = snd_seq_client_info_get_pid(cinfo);
		if (pid != -1)
			printf(",pid=%d", pid);
		printf("]\n");
	}

	printf("  %3d '%-16s'\n",
	       snd_seq_port_info_get_port(pinfo),
	       snd_seq_port_info_get_name(pinfo));
}

static void acd_get_subscribers(
	snd_seq_t *seq, snd_seq_query_subscribe_t *subs, snd_seq_query_subs_type_t type)
{
	int count = 0;
	snd_seq_query_subscribe_set_type(subs, type);
	snd_seq_query_subscribe_set_index(subs, 0);

	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
		const snd_seq_addr_t *addr;

		if (count++ == 0)
			printf("\t%d: ", type);
		else
			printf(", ");

		addr = snd_seq_query_subscribe_get_addr(subs);
		printf("%d:%d", addr->client, addr->port);

		if (snd_seq_query_subscribe_get_exclusive(subs))
			printf("[ex]");

		if (snd_seq_query_subscribe_get_time_update(subs)) {
			printf("[%s:%d]",
				(snd_seq_query_subscribe_get_time_real(subs) ? "real" : "tick"),
				snd_seq_query_subscribe_get_queue(subs)
			);
		}

		snd_seq_query_subscribe_set_index(subs,
			snd_seq_query_subscribe_get_index(subs) + 1
		);
	}

	if (count > 0) printf("\n");
}

static void acd_get_subscribers(snd_seq_t *seq)
{
	int count;
	snd_seq_port_info_t *pinfo;
	snd_seq_client_info_t *cinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(
			pinfo,
			snd_seq_client_info_get_client(cinfo)
		);

		count = 0;
		snd_seq_port_info_set_port(pinfo, -1);

		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
			acd_get_port(seq, cinfo, pinfo, count);

			snd_seq_query_subscribe_t *subs;
			snd_seq_query_subscribe_alloca(&subs);
			snd_seq_query_subscribe_set_root(
				subs,
				snd_seq_port_info_get_addr(pinfo)
			);

			acd_get_subscribers(seq, subs, SND_SEQ_QUERY_SUBS_READ);
			acd_get_subscribers(seq, subs, SND_SEQ_QUERY_SUBS_WRITE);

			count++;
		}
	}
}

int main(int argc, char *argv[])
{
	snd_seq_t *seq;
	//snd_seq_port_subscribe_t *subs;
	//snd_seq_addr_t sender, dest;

	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "%s: can't open sequencer\n", __PRETTY_FUNCTION__);
		return 1;
	}

	snd_lib_error_set_handler(acd_error);

	acd_get_subscribers(seq);

	return 0;
}
