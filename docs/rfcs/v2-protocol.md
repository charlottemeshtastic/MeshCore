# V2 packet protocol discussion draft

Status: requirements proposal. There is no consensus claim, assigned wire format
or implemented V2 encryption suite in this document.

## Requirements

V2 should address routing-hash collisions, endpoint authentication, replay
protection, explicit extensibility and coexistence with V1. Packet size remains
bounded by the actual physical radio MTU and airtime constraints.

Separate protocol version, payload encoding revision and companion protocol
revision. Decide which version bits discriminate the outer frame before assigning
any reserved values. V1 already supports 1-, 2- and 3-byte path hashes; compare
their actual collision and overhead costs rather than treating wider hashes as
a V2-only invention.

## Threat model

Assume attackers can receive, inject, replay and forward packets, manipulate
unauthenticated routes, join public scopes and impersonate short hash prefixes.
Specify the protections of direct messages, group messages and management
commands independently. Physical key extraction is a target-specific concern.

Transport filtering is advisory routing policy. It cannot replace authenticated
endpoint authorisation. Document what a forwarding node may legitimately mutate
and what the endpoint must authenticate.

## Candidate frame design

Discuss a fixed discriminator plus bounded lengths for immutable authenticated
fields and mutable routing fields. Define canonical byte order, maximum extension
count, duplicate-field handling, unknown-critical-extension rejection and
unknown-optional-extension skipping. Validate every length before allocation or
copy. Reserved bits must have defined send/receive behavior.

Choose endpoint identifier widths using network-size and collision analysis.
Short hashes select candidates; successful cryptographic verification establishes
identity. Define path collision recovery and loop limits independently.

## Cryptographic decisions

Evaluate established authenticated-encryption suites available on target MCUs.
Compare flash/RAM cost, packet overhead and existing accelerators. Specify key
agreement, key derivation with domain separation, nonce construction, key rotation
and group sender identity. Do not design a bespoke cipher or infer security from
a longer truncated MAC alone.

Nonce uniqueness must survive reboot and multi-node hosting. Decide between
persistent counters and session-derived keys, including power-loss behavior and
storage wear. Define a bounded replay window per authenticated sender/session.
Review management commands separately for replay and authorisation.

Compression encoding, immutable return route and capability revision belong in
the authenticated portion. Mutable routing fields need a documented abuse model.
Have the chosen construction reviewed before distributing V2 firmware.

## Migration

Authenticated capability exchange controls V2 sends. Unknown peers use V1.
Distinguish intentional V1 operation from an attacker forcing a downgrade; users
or deployments may pin required security/features after successful enrollment.

Decide whether V1 repeaters can carry V2 opaquely. The baseline parser rejects
unsupported versions, so transparent carriage must not be assumed. Publish a
topology compatibility matrix before deployment. Mixed groups need an explicit
version policy and must not broadcast two versions as two logical messages.

## Consensus process and release gate

Present requirements and open choices to maintainers and client authors. Resolve
disagreements in a decision log. Publish a draft with packet examples and known
invalid frames. Obtain two independently written interoperable codecs, native
parser fuzzing, hardware topology tests and cryptographic review.

External reviewers determine consensus. A local RFC or repository commit cannot
claim it. Record reviewers, agreed revision and unresolved points before changing
this document's status to accepted.
