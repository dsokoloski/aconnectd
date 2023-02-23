#include <string>
#include <fstream>
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

static acdConfig acd_config;

void acdConfig::Load(const string &filename)
{
    json j;
    ifstream ifs(filename);
    if (! ifs.is_open()) {
        fprintf(stderr, "Error loading configuration: %s: %s\n",
            filename.c_str(), strerror(ENOENT));
        return;
    }

    try {
        ifs >> j;
    }
    catch (exception &e) {
        fprintf(stderr, "Error loading configuration: %s: JSON parse error\n",
            filename.c_str());
        return;
    }

    try {
        refresh_ttl = j["refresh_ttl"].get<unsigned>();
    } catch (...) { }

    try {
        patches.clear();

        for (auto &it : j["patches"]) {
            acdPatch patch(
                it["src_client"].get<string>(),
                it["src_port"].get<string>(),
                it["dst_client"].get<string>(),
                it["dst_port"].get<string>()
            );

            pair<string, string> key;
            patch.MakeKey(key);

            patches.insert(make_pair(key, patch));
        }
    } catch (...) { }
}

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
            fprintf(stdout, "Inserted port: %d: %s\n", port.id, port.name.c_str());

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
    //count += AddSubscriptions(seq, subs, SND_SEQ_QUERY_SUBS_WRITE);

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

        fprintf(stdout, "Inserted subscription: %d:%d %s %d:%d: %d\n",
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

bool acdSubscription::GetAddress(
    snd_seq_t *seq, const acdPatch &patch, snd_seq_addr_t &addr, enum AddrType atype)
{
    string client, port;

    switch (atype) {
    case atSRC:
        client = patch.src_client;
        port = patch.src_port;
        break;

    case atDST:
    default:
        client = patch.dst_client;
        port = patch.dst_port;
        break;
    }

    for (auto &it_client : acd_clients) {
        if (client != it_client.second.name) continue;

        for (auto &it_port : it_client.second.ports) {
            if (port != it_port.second.name) continue;

            const string a(
                to_string(it_client.first) + ":" + to_string(it_port.first)
            );

            if (snd_seq_parse_address(seq, &addr, a.c_str()) < 0) {
                fprintf(stderr, "Invalid address: %s:%s (%d:%d)\n",
                    client.c_str(), port.c_str(), it_client.first, it_port.first
                );

                return false;
            }

            return true;
        }
    }

    return false;
}

bool acdSubscription::GetAddress(snd_seq_t *seq,
    const acdSubscription &subscription, snd_seq_addr_t &addr, enum AddrType atype)
{
    string a;

    switch (atype) {
    case atSRC:
        a = to_string(subscription.src_client.id) +
            ":" + to_string(subscription.src_port.id);
        break;

    case atDST:
    default:
        a = to_string(subscription.dst_client.id) +
            ":" + to_string(subscription.dst_port.id);
        break;
    }

    if (snd_seq_parse_address(seq, &addr, a.c_str()) < 0) {
        fprintf(stderr, "Invalid address: %s\n", a.c_str());
        return false;
    }

    return true;
}

bool acdSubscription::Add(snd_seq_t *seq, const acdPatch &patch)
{
    snd_seq_addr_t src, dst;

    if (! acdSubscription::GetAddress(seq, patch, src, atSRC)) return false;
    if (! acdSubscription::GetAddress(seq, patch, dst, atDST)) return false;

    snd_seq_port_subscribe_t *sub;
    snd_seq_port_subscribe_alloca(&sub);

    int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;

    snd_seq_port_subscribe_set_queue(sub, queue);
    snd_seq_port_subscribe_set_exclusive(sub, exclusive);
    snd_seq_port_subscribe_set_time_update(sub, convert_time);
    snd_seq_port_subscribe_set_time_real(sub, convert_real);

    if (acdSubscription::Execute(seq, sub, src, dst, etSUBSCRIBE)) {
        pair<string, string> key;
        patch.MakeKey(key);
        fprintf(stdout, "Subscribed: %s -> %s\n",
            key.first.c_str(), key.second.c_str()
        );
        return true;
    }

    return false;
}

bool acdSubscription::Remove(snd_seq_t *seq, const acdSubscription &subscription)
{
    snd_seq_addr_t src, dst;

    if (! acdSubscription::GetAddress(seq, subscription, src, atSRC)) return false;
    if (! acdSubscription::GetAddress(seq, subscription, dst, atDST)) return false;

    snd_seq_port_subscribe_t *sub;
    snd_seq_port_subscribe_alloca(&sub);

    if (acdSubscription::Execute(seq, sub, src, dst, etUNSUBSCRIBE)) {
        pair<string, string> key;
        subscription.MakeKey(key);
        fprintf(stdout, "Unsubscribed: %s -> %s\n",
            key.first.c_str(), key.second.c_str()
        );
        return true;
    }

    return false;
}

bool acdSubscription::Execute(
    snd_seq_t *seq, snd_seq_port_subscribe_t *sub,
    snd_seq_addr_t &src, snd_seq_addr_t &dst, enum ExecType etype)
{
    snd_seq_port_subscribe_set_sender(sub, &src);
    snd_seq_port_subscribe_set_dest(sub, &dst);

    switch (etype) {
    case etSUBSCRIBE:
        if (snd_seq_subscribe_port(seq, sub) < 0) {
            fprintf(stderr, "Failed to subscribe: %s\n", snd_strerror(errno));
            return false;
        }

        break;

    case etUNSUBSCRIBE:
    default:
        if (snd_seq_subscribe_port(seq, sub) < 0) {
            fprintf(stderr, "Failed to unsubscribe: %s\n", snd_strerror(errno));
            return false;
        }
        break;
    }

    return true;
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
            fprintf(stdout, "Inserted client: %d: %s\n",
                client.id, client.name.c_str()
            );

            it.first->second.AddPorts(it.first->second, seq, cinfo);
        }
    }
}

static void acd_resolve_subscriptions(void)
{
    for (auto &it_sub : acd_sub_addr_map) {

        auto it_src_client = acd_clients.find(it_sub.first.first);

        if (it_src_client == acd_clients.end()) {
            fprintf(stderr, "Subscription from invalid source client: %d\n",
                it_sub.first.first
            );
            continue;
        }

        auto it_src_port = (*it_src_client).second.ports.find(it_sub.first.second);

        if (it_src_port == (*it_src_client).second.ports.end()) {
            fprintf(stderr, "Subscription from invalid source address: %d:%d\n",
                it_sub.first.first, it_sub.first.second
            );
            continue;
        }

        auto it_dst_client = acd_clients.find(it_sub.second.first.first);

        if (it_dst_client == acd_clients.end()) {
            fprintf(stderr, "Subscription from invalid destination client: %d\n",
                it_sub.second.first.first
            );
            continue;
        }

        auto it_dst_port = (*it_dst_client).second.ports.find(it_sub.second.first.second);

        if (it_dst_port == (*it_dst_client).second.ports.end()) {
            fprintf(stderr, "Subscription from invalid source address: %d:%d\n",
                it_sub.second.first.first, it_sub.second.first.second
            );
            continue;
        }

        acdSubscription subscription(
            (*it_src_client).second, (*it_src_port).second,
            (*it_dst_client).second, (*it_dst_port).second,
            (snd_seq_query_subs_type_t)it_sub.second.second
        );

        pair<string, string> key;
        subscription.MakeKey(key);

        auto it = acd_sub_map.insert(
            make_pair(key, subscription)
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
    acd_config.Load("/etc/aconnectd.json");

    snd_seq_t *seq;

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening sequencer\n");
        return 1;
    }

    snd_lib_error_set_handler(acd_error);

    if (snd_seq_set_client_name(seq, "aconnectd") < 0) {
        snd_seq_close(seq);
        fprintf(stderr, "Error setting client name.\n");
        return 1;
    }

    acd_my_id = snd_seq_client_id(seq);

    acd_refresh(seq);
    acd_resolve_subscriptions();

    for (auto &it : acd_config.patches) {
        auto it_sub = acd_sub_map.find(it.first);

        if (it_sub == acd_sub_map.end())
            acdSubscription::Add(seq, it.second);
    }

    for (auto &it : acd_sub_map) {
        auto it_patch = acd_config.patches.find(it.first);

        if (it_patch == acd_config.patches.end())
            acdSubscription::Remove(seq, it.second);
    }

    snd_seq_close(seq);

    return 0;
}

// vi: expandtab shiftwidth=4 softtabstop=4 tabstop=4
