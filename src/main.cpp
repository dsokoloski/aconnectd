#include <string>
#include <iostream>
#include <vector>
#include <map>

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

#include "aconnectd.h"

static int acd_my_id = -1;

static acdSubAddrMap acd_sub_addr_map;
static acdSubMap acd_sub_map;

static void acd_error(const char *file, int line, const char *function,
	int err, const char *fmt, ...)
{
	va_list arg;

	if (err == ENOENT) return;

	va_start(arg, fmt);
	fprintf(stderr, "ALSA error: %s:%i:(%s) ", file, line, function);
	vfprintf(stderr, fmt, arg);
	if (err)
		fprintf(stderr, ": %s", snd_strerror(err));
	putc('\n', stderr);
	va_end(arg);
}

// client 0: 'System' [type=kernel]
//     0 'Timer           '
//     1 'Announce        '
// client 14: 'Midi Through' [type=kernel]
//     0 'Midi Through Port-0'
// client 16: 'Launchkey Mini' [type=kernel,card=0]
//     0 'Launchkey Mini MIDI 1'
// 	0: 128:2
//     1 'Launchkey Mini MIDI 2'
// client 128: 'rtpmidi DMS Keys' [type=user,pid=215]
//     0 'Network         '
//     1 'Nano - LKMk2 InControl'
//     2 'Nano - LKMk2 MIDI1'
// 	1: 16:0
//     3 'Nano - SLMk2 MIDI1'
//     4 'Nano - SLMk2 MIDI2'

size_t acdClient::AddPorts(const acdClient &client,
	snd_seq_t *seq, snd_seq_client_info_t *cinfo) {

	ports.clear();

	snd_seq_port_info_t *pinfo;
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_port_info_set_port(pinfo, -1);
	snd_seq_port_info_set_client(
		pinfo,
		snd_seq_client_info_get_client(cinfo)
	);

	while (snd_seq_query_next_port(seq, pinfo) >= 0) {
		acdPort port(client, pinfo);

		auto it = ports.insert(make_pair(port.id, port));

		if (it.second == true) {
			fprintf(stdout, "Inserted new port: %d: %s\n", port.id, port.name.c_str());

			it.first->second.AddSubscriptions(seq, pinfo);
		}
	}

	return ports.size();
}

static map<int, acdClient> acd_clients;

size_t acdPort::AddSubscriptions(snd_seq_t *seq, snd_seq_port_info_t *pinfo)
{
	snd_seq_query_subscribe_t *subs;
	snd_seq_query_subscribe_alloca(&subs);

	snd_seq_query_subscribe_set_root(
		subs,
		snd_seq_port_info_get_addr(pinfo)
	);

	size_t count = 0;
	count += AddSubscriptions(seq, subs, SND_SEQ_QUERY_SUBS_READ);
	count += AddSubscriptions(seq, subs, SND_SEQ_QUERY_SUBS_WRITE);

	return count;
}

size_t acdPort::AddSubscriptions(snd_seq_t *seq,
	snd_seq_query_subscribe_t *subs, snd_seq_query_subs_type_t type)
{
	size_t count = 0;
	snd_seq_query_subscribe_set_type(subs, type);
	snd_seq_query_subscribe_set_index(subs, 0);

	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
		const snd_seq_addr_t *addr;

		addr = snd_seq_query_subscribe_get_addr(subs);

		acd_sub_addr_map.push_back(
			make_pair(
				acdSubAddr(client.id, id),
				make_pair(
					acdSubAddr(addr->client, addr->port),
					type
				)
			)
		);

		fprintf(stdout, "Added subscriptions: %d:%d %s %d:%d: %d\n",
			client.id, id,
			(type == SND_SEQ_QUERY_SUBS_READ) ? "->" : "<-",
			addr->client, addr->port, type
		);

		count++;

		snd_seq_query_subscribe_set_index(subs,
			snd_seq_query_subscribe_get_index(subs) + 1
		);
	}

	return count;
}

static void acd_refresh(snd_seq_t *seq)
{
	acd_clients.clear();

	acd_sub_addr_map.clear();
	acd_sub_map.clear();

	snd_seq_client_info_t *cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	while (snd_seq_query_next_client(seq, cinfo) >= 0) {

		if (snd_seq_client_info_get_client(cinfo) == acd_my_id) continue;

		acdClient client(cinfo);

		auto it = acd_clients.insert(make_pair(client.id, client));

		if (it.second == true) {
			fprintf(stdout, "Inserted new client: %d: %s\n",
				client.id, client.name.c_str()
			);

			it.first->second.AddPorts(it.first->second, seq, cinfo);
		}
	}
}

static void acd_resolve_subscriptions(void)
{
	for (auto &it_sub : acd_sub_addr_map) {

		auto it_client_src = acd_clients.find(it_sub.first.first);

		if (it_client_src == acd_clients.end()) {
			fprintf(stderr, "Subscription from invalid source client: %d\n",
				it_sub.first.first
			);
			continue;
		}

		auto it_port_src = (*it_client_src).second.ports.find(it_sub.first.second);

		if (it_port_src == (*it_client_src).second.ports.end()) {
			fprintf(stderr, "Subscription from invalid source address: %d:%d\n",
				it_sub.first.first, it_sub.first.second
			);
			continue;
		}

		auto it_client_dst = acd_clients.find(it_sub.second.first.first);

		if (it_client_dst == acd_clients.end()) {
			fprintf(stderr, "Subscription from invalid destination client: %d\n",
				it_sub.second.first.first
			);
			continue;
		}

		auto it_port_dst = (*it_client_dst).second.ports.find(it_sub.second.first.second);

		if (it_port_dst == (*it_client_dst).second.ports.end()) {
			fprintf(stderr, "Subscription from invalid source address: %d:%d\n",
				it_sub.second.first.first, it_sub.second.first.second
			);
			continue;
		}

		acdSubscription subscription(
			(*it_client_src).second, (*it_port_src).second,
			(*it_client_dst).second, (*it_port_dst).second,
			(snd_seq_query_subs_type_t)it_sub.second.second
		);

		auto it = acd_sub_map.insert(
			make_pair(
				make_pair(
					(*it_client_src).second.name + ":" + (*it_port_src).second.name,
					(*it_client_dst).second.name + ":" + (*it_port_dst).second.name
				),
				subscription
			)
		);

		if (it.second == true &&
			it_sub.second.second == SND_SEQ_QUERY_SUBS_READ) {
			fprintf(stdout, "Resolved subscription: %s -> %s\n",
				it.first->first.first.c_str(),
				it.first->first.second.c_str()
			);
		}
	}
}

int main(int argc, char *argv[])
{
	snd_seq_t *seq;

	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "Error opening sequencer\n");
		return 1;
	}

	snd_lib_error_set_handler(acd_error);

	acd_my_id = snd_seq_client_id(seq);

	acd_refresh(seq);
	acd_resolve_subscriptions();

	snd_seq_close(seq);

	return 0;
}
