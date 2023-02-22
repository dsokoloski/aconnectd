#ifndef _ACONNECTD_H
#define _ACONNECTD_H

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

typedef pair<int, int> acdSubAddr;
typedef vector<pair<acdSubAddr, pair<acdSubAddr, int>>> acdSubAddrMap;

class acdPort;
class acdClient
{
public:
	int id;
	string name;

	acdClient(snd_seq_client_info_t *cinfo) :
		id(snd_seq_client_info_get_client(cinfo)),
		name(snd_seq_client_info_get_name(cinfo)) { }

	size_t AddPorts(const acdClient &client,
		snd_seq_t *seq, snd_seq_client_info_t *cinfo);

	map<int, acdPort> ports;
};

class acdSubscription;
class acdPort
{
public:
	const acdClient &client;
	int id;
	string name;

	acdPort(const acdClient &client, snd_seq_port_info_t *pinfo) :
		client(client),
		id(snd_seq_port_info_get_port(pinfo)),
		name(snd_seq_port_info_get_name(pinfo)) { }

	size_t AddSubscriptions(snd_seq_t *seq, snd_seq_port_info_t *pinfo);
	size_t AddSubscriptions(snd_seq_t *seq,
		snd_seq_query_subscribe_t *subs, snd_seq_query_subs_type_t type);
};

typedef map<pair<string, string>, acdSubscription> acdSubMap;

class acdSubscription
{
public:
	const acdClient &client_src;
	const acdPort &port_src;
	const acdClient &client_dst;
	const acdPort &port_dst;
	snd_seq_query_subs_type_t type;

	acdSubscription(
		const acdClient &client_src, const acdPort &port_src,
		const acdClient &client_dst, const acdPort &port_dst,
		snd_seq_query_subs_type_t type) :
		client_src(client_src), port_src(port_src),
		client_dst(client_dst), port_dst(port_dst),
		type(type) { }
};

#endif // _ACONNECTD_H
