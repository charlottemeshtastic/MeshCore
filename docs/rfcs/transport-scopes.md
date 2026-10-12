# V1 transport scope contract

Status: draft describing the baseline, with explicit byte-order implementation.

## Scope tokens

V1 transport routes carry two unsigned 16-bit integers, little-endian, immediately
after the header. Slot 1 is the forwarding scope token. Slot 2 remains reserved;
senders set it to zero. Receivers preserve it through serialization and must not
interpret a nonzero value as an authenticated return scope.

For a 16-byte region key, compute HMAC-SHA256 over the one-byte payload type
followed by the exact transmitted payload bytes. Interpret the first two digest
bytes as a little-endian integer. Map `0x0000` to `0x0001` and `0xFFFF` to `0xFFFE`.
Header route bits, path bytes and transport slots are excluded. This lets the
token survive forwarding without access to the message encryption key.

Public region keys are the first 16 SHA256 digest bytes of the region name with
one leading `#`. The baseline treats an unprefixed public name as its `#` form.
Names are case-sensitive byte strings; no Unicode normalisation is introduced.
Private names beginning with `$` resolve through the transport key store.

These tokens are short filters. Public region names allow anyone to derive their
keys, and 16-bit tokens can collide. A token is not proof of sender identity or
authorisation. Payload authentication remains separate.

## Routing policy

Preserve the baseline behavior during standardisation:

* Unscoped floods use the wildcard allow/deny policy and unscoped hop limit.
* Scoped floods match slot 1 against allowed configured region keys. Unmatched
  scoped floods are not forwarded by the repeater.
* Region parent relationships describe configuration; they do not implicitly
  grant forwarding to all children. Define inheritance in a separate change.
* Direct routing continues using its path rules. `REGION_DENY_DIRECT` is reserved.
* Replies mirror a resolved request scope. Unscoped flood requests receive
  unscoped replies. An unresolved/direct request uses a configured default scope
  when its reply must flood, otherwise the baseline unscoped fallback applies.
* A reply gets a fresh token computed from its own payload. Copying the request's
  numeric token into the reply would produce an invalid scope match.
* The `{0,0}` transport pair has baseline share/"send to nowhere" behavior.
  Ordinary scoped senders never derive zero for slot 1.

Bridge transport preserves the packet bytes. Repeater RF forwarding applies the
scope policy to bridge ingress as well as radio ingress. Additional restrictions
on traffic travelling inside the bridge medium require an explicit ingress/egress
policy; preserving bytes alone does not implement that restriction.

## Interoperability vectors

Key: `000102030405060708090a0b0c0d0e0f`.

| Payload type | Payload hex | Digest prefix | Token | Wire bytes |
| --- | --- | --- | --- | --- |
| 2 | `68656c6c6f` | `0f7b` | `0x7B0F` | `0f7b` |
| 2 | empty | `cbf5` | `0xF5CB` | `cbf5` |
| 5 | bytes 0 through 31 | `3549` | `0x4935` | `3549` |
| 2 | `17220200` | `0000` | `0x0001` | `0100` |
| 2 | `492b0000` | `ffff` | `0xFFFE` | `feff` |

Vectors were computed independently with Python's HMAC-SHA256 and are tested
against the Crypto library rather than the native SHA256 mock.

## Parser contract

Radio and bridge ingress share `Packet::readFrom`. Reject a null input, frames
shorter than two bytes, transport frames shorter than six bytes, unsupported
payload versions, reserved path-width encoding, truncated paths, payloads over
184 bytes and frames over 255 bytes. Empty payloads remain structurally valid;
payload-type handlers are responsible for their minimum lengths.

`Packet::writeTo` checks path encoding, payload length and output capacity before
writing. Zero means failure. Callers must not transmit a zero-length result.

## Follow-on work

Private key load/save/remove contains stubs at the baseline. Specify a storage
backend and key migration before declaring persistent private scopes complete.
This contract does not invent hardware-protected storage on unsupported boards.

Add region policy tests with real keys, wildcard cases, key rotation and reboot.
Build and exercise both bridge types. Reserve slot 2 until a reviewed extension
defines how its return-scope meaning is negotiated and validated.
