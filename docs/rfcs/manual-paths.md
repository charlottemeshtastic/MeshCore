# Round-trip manual paths

Status: fork-specific revision 1 implemented for authenticated direct requests.
Text-message/ACK envelopes and upstream allocation remain follow-on work.

## Request format

The encrypted V1 REQ plaintext carries a four-byte timestamp/tag followed by:

```
[0xF0][revision=1][return-path metadata][return-path bytes][body length][body]
```

Body length is one byte and includes the original request type. It must be
nonzero. Nested `0xF0` envelopes are rejected. Any trailing cipher padding must
be zero. The complete plaintext must fit the V1 encrypted-payload budget;
manual paths consume that budget and reduce the available request-body size.

The outer packet uses DIRECT routing and carries the independently supplied
outbound path. Companion endpoints and the simple repeater unwrap only after
V1 authentication. Replies use the supplied return path in preference to a
learned path. Neither send nor reply processing replaces the contact's learned
route. Zero-hop outbound and return paths are explicit valid choices.

These values are experimental allocations in this fork. No reserved V1 packet
header bits change meaning. They do not claim an upstream registry reservation.

## Capability exchange

Send an ordinary encrypted REQ whose body begins with `0xF1`. Its response
contains the echoed four-byte little-endian tag followed by `CMF`, schema byte 1
and five feature-revision bytes, in this order:

1. Manual request envelope revision.
2. Sub-mesh profile revision.
3. LZW message envelope revision.
4. Virtual-node hosting revision.
5. Adaptive coding-rate revision.

The implemented feature record is `43 4d 46 01 01 00 00 00 00`. Zero means
unsupported. The LZW codec's presence does not advertise compressed-message
support. Queries are handled by companion endpoints and the simple repeater;
other example applications must implement support before advertising it.

Apps receive peer responses through the authenticated firmware response path,
correlate the tag and keep the result for the requested full peer key. The
connected companion's support does not establish recipient support. Unknown
peers receive ordinary requests. A timeout does not authorise flooding a manual
request or changing its encoding. No automatic extension sends are enabled.

## Companion commands

Fork command `0xF1` with no arguments returns `0xF1` followed by the local
feature record. Its response identifies the connected companion's capabilities.
The local query does not send a radio packet.

Fork command `0xF0` sends a manual round-trip request:

```
[0xF0][revision=1][confirmed-peer-support=1][recipient public key:32]
[outbound path metadata][outbound path bytes]
[return path metadata][return path bytes][body length][body]
```

The app sets the required support flag only after the recipient advertises
manual-request revision 1. Unknown flags/revisions, malformed paths, missing
body bytes and trailing command bytes are rejected before sending. A successful
send uses the ordinary SENT response and pending binary-response tag handling.
No contact update is needed to choose either route. Other clients continue using
the established companion commands.

## Core API

`BaseChatMesh::sendRoundTripRequest` accepts independent paths and a request body.
The caller establishes recipient support first. The API returns
`MSG_SEND_SENT_DIRECT` on queueing, or `MSG_SEND_FAILED` for invalid data,
insufficient encrypted-payload space or an unavailable packet.

Its timeout estimate uses the longer route's hop count, rather than the encoded
path metadata byte. Outbound path bytes participate in packet airtime estimation.
The reply path is scoped to the active request and is never retained as a pointer
into a freed packet. Request handlers receive the body's declared byte length.

The repeater preserves its permission and timestamp-replay checks. Its request
handlers check minimum lengths for telemetry, access-list and neighbor queries
before reading their parameters. Anonymous direct requests retain their separate
established format and use the bounded `Packet::readPath` helper.

## Vectors and validation

This envelope carries a two-hop return path with two-byte hashes and request
body `01 00 00`:

```
f0 01 42 11 22 33 44 03 01 00 00
```

Native tests cover every supported path width/count, explicit zero hops,
truncated input, malformed versions, nested requests, cipher padding, output
capacity, command opt-in and independent routes. Random-input checks run under
address and undefined-behavior sanitizers. Firmware builds check both endpoint
implementations; these checks do not establish RF delivery.

Physical acceptance uses A -> R1 -> R2 -> B outbound and B -> R3 -> A inbound.
Verify the supplied path wins over a different stored route, zero-hop behavior,
response tag matching, replay rejection, lost replies and unchanged contact paths.

## Follow-on work

Generalise the authenticated message envelope to text, compressed text and ACKs.
Define one logical transaction identity across retries and encodings. Specify how
manual routes interact with optional flood fallback and transport scopes.

Apps need path editors, trace verification, capability caching and response
handling. Runtime capability caching in firmware and hardware topology validation
remain separate milestones. V2 security and migration decisions are described in
[v2-protocol.md](v2-protocol.md).
