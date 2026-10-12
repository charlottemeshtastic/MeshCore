# Bounded LZW message compression

Status: reference codec implemented; message-envelope integration is proposed.

## Codec revision 1

Input is an arbitrary byte string of at most 184 bytes. Dictionary symbols 0-255
are single bytes. Code 256 is CLEAR, code 257 is END, and generated entries begin
at 258. Dictionary capacity is 512. Codes are always nine bits, packed most
significant bit first. The first code is CLEAR. END terminates the stream; unused
bits in its last byte are zero. Extra bytes are invalid. Interior CLEAR codes are
invalid in this revision. Dictionary entries freeze at capacity.

Every message has its own dictionary. No dictionary state crosses messages or
peers. Empty input is valid. The decoder handles the standard LZW next-code case
and rejects undefined codes, missing END, nonzero padding and expansion beyond
184 bytes. Return errors rather than truncated text. Callers must discard partial
output on failure and must use separate input and output buffers.

The codec performs no heap allocation. Its workspaces occupy roughly one kilobyte
per active call, excluding input/output buffers and compiler overhead. Measure
stack high-water usage on each target before integration.

## Message integration

Negotiate the exact codec revision with the recipient. Compress the text body
before encryption; leave routing and transport-token computation unchanged.
The authenticated envelope identifies encoding and expected decoded length.
Decode only after authentication and require an exact decoded-length match.

Compare complete envelope sizes. Send uncompressed when the compressed envelope
is not smaller or exceeds the plaintext budget. Compression is not a way to
exceed the application's maximum display-message length.

ACKs and deduplication identify one logical message independently of the chosen
body encoding. Preserve attempt handling across compressed retries. Disable
compression for payloads mixing secrets with attacker-controlled data unless the
application has reviewed observable-length leakage.

Group traffic needs a group-wide capability policy. One capable local companion
does not prove every group recipient can decode LZW. Default groups to ordinary
V1 text until that policy is configured.

## Shared vectors

| Input ASCII | Encoded hex |
| --- | --- |
| empty | `804040` |
| `A` | `80106020` |
| `AAAAAAA` | `80106050320c04` |
| `TOBEORNOTTOBEORTOBEORNOT` | `801509e422293ca44e2795205048342e0b0784c040` |

`LzwCodec` is tested against an independent string-dictionary oracle over all
input lengths 0-184 and several alphabets. Parser tests include truncated streams,
arbitrary bytes, padding and decompression expansion bounds.

An app codec must match these bytes exactly. Switching to variable-width LZW,
LSB-first packing or a different reset convention requires a different revision.

## Remaining acceptance

Add authenticated envelope handling in firmware and apps, capability fallback,
UTF-8 preservation, message/ACK identity tests and hardware memory measurements.
The presence of `LzwCodec` alone does not enable compressed messages on the air.
