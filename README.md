# AConnectD

ALSA Sequencer Patch Manager

Written by Darryl Sokoloski <darryl@sokoloski.ca>

Licenced under the GPL version 3.

Based off of aconnect [https://alsa.opensrc.org/Aconnect](Aconnect) by Takashi Iwai.

## Overview

The purpose of this application is to automatically synchronize ALSA
Sequencer connections as defined by a JSON "*patch*" configuration file.

This functionality is primarily useful when deployed in conjunction with [RTP MIDI](https://github.com/davidmoreno/rtpmidid).

The daemon will periodically (configurable interval) check for ALSA devices.
New device subscriptions will be made according to the configuration file, and
existing connections are verified against the configuration file.  Patches (or
subscriptions) are removed if a corresponding "*patch*" isn't configured.

## Configuration

The daemon is configured by a JSON file.

The refresh interval (TTL) is configured by a the top-level key: `refresh_ttl`

The TTL is an integer representing the number of seconds to wait between
refreshes.

The patches are defined as an array of objects.  Each "*patch*" object is
defined using the following schema:

`enabled: boolean, default: true`

Rather than delete patches, they can be temporarily disabled using the `enabled` option.

`convert_time_mode: string, default: null, valid values: real, tick`

The `convert_time_mode` setting, can be omitted (or `null`), `real`, or `tick`.
According to the [aconnect(1)](https://linux.die.net/man/1/aconnect) man page,
the options have the following effect:

`real`: Convert time-stamps of event packets to the current value of the given
real-time queue. This is option is, however, not so useful, since the receiver
port must use (not necessarily own) the specified queue.

`tick`: Like the `real` option, but time-stamps are converted to the current
value of the given tick queue.

`convert_time_queue: integer, default: 0`

The time queue to apply the conversion to.

`exclusive: boolean, default: false`

Connect ports with exclusive mode. Both sender and receiver ports can be no
longer connected by any other ports.

`src_client: string, default: null, required`

The source client name.

`src_port: string, default: null, required`

The source port name.

`dst_client: string, default: null, required`

The destination client name.

`dst_port: string, default: null, required`

The destination port name.

### A Minimal Example Patch

```json
    "patches": [
        {
            "src_client": "Launchkey Mini",
            "src_port": "Launchkey Mini MIDI 1",
            "dst_client": "rtpmidi DMS Keys",
            "dst_port": "Nano - LKMk2 MIDI1"
        }
    ]
```

## Command-line Options

The daemon accepts a few command-line options:

`-c, --config <file>`: Configuration file override.  Default: `/etc/aconnectd.json`
`-d, --daemon`: Run in daemon mode (detatch).
`-v, --verbose`: Output verbose messages, useful for debugging.

