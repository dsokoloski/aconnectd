#ifndef _ACONNECTD_H
#define _ACONNECTD_H

// client 0: 'System' [type=kernel]
//     0 'Timer           '
//     1 'Announce        '
// client 14: 'Midi Through' [type=kernel]
//     0 'Midi Through Port-0'
// client 16: 'Launchkey Mini' [type=kernel,card=0]
//     0 'Launchkey Mini MIDI 1'
//  0: 128:2
//     1 'Launchkey Mini MIDI 2'
// client 128: 'rtpmidi DMS Keys' [type=user,pid=215]
//     0 'Network         '
//     1 'Nano - LKMk2 InControl'
//     2 'Nano - LKMk2 MIDI1'
//  1: 16:0
//     3 'Nano - SLMk2 MIDI1'
//     4 'Nano - SLMk2 MIDI2'

class acdPatch
{
public:
    const string src_client;
    const string src_port;
    const string dst_client;
    const string dst_port;
    int queue;
    int convert_real;
    int convert_time;
    bool exclusive;

    acdPatch(
        const string &src_client, const string &src_port,
        const string &dst_client, const string &dst_port,
        int queue, int convert_real, int convert_time,
        bool exclusive) :
        src_client(src_client), src_port(src_port),
        dst_client(dst_client), dst_port(dst_port),
        queue(queue), convert_real(convert_real), convert_time(convert_time),
        exclusive(exclusive) { }

    inline void MakeKey(pair<string, string> &key) const {
        key.first = src_client + "/" + src_port;
        key.second = dst_client + "/" + dst_port;
    }
};

class acdConfig
{
public:
    unsigned refresh_ttl;
    map<pair<string, string>, acdPatch> patches;

    acdConfig() : refresh_ttl(30) { }

    void Load(const string &filename);
};

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
    const acdClient &src_client;
    const acdPort &src_port;
    const acdClient &dst_client;
    const acdPort &dst_port;
    snd_seq_query_subs_type_t type;

    acdSubscription(
        const acdClient &src_client, const acdPort &src_port,
        const acdClient &dst_client, const acdPort &dst_port,
        snd_seq_query_subs_type_t type) :
        src_client(src_client), src_port(src_port),
        dst_client(dst_client), dst_port(dst_port),
        type(type) { }

    inline void MakeKey(pair<string, string> &key) const {
        key.first = src_client.name + "/" + src_port.name;
        key.second = dst_client.name + "/" + dst_port.name;
    }

    enum AddrType {
        atSRC,
        atDST
    };

    static bool GetAddress(
        snd_seq_t *seq, const acdPatch &patch,
        snd_seq_addr_t &addr, enum AddrType atype
    );
    static bool GetAddress(
        snd_seq_t *seq, const acdSubscription &subscription,
        snd_seq_addr_t &addr, enum AddrType atype
    );

    static bool Add(snd_seq_t *seq, const acdPatch &patch);
    static bool Remove(snd_seq_t *seq, const acdSubscription &subscription);

    enum ExecType {
        etSUBSCRIBE,
        etUNSUBSCRIBE
    };

    static bool Execute(
        snd_seq_t *seq, snd_seq_port_subscribe_t *sub,
        snd_seq_addr_t &src, snd_seq_addr_t &dst, enum ExecType etype);
};

#endif // _ACONNECTD_H

// vi: expandtab shiftwidth=4 softtabstop=4 tabstop=4
