# MeshCore feature programme

Status: working proposals for the charlottemeshtastic fork. These documents do not
claim upstream agreement or production readiness.

Baseline: main commit `a366955cb2f67b8e6842d4f00d2b6a554dddd88a`.

## Delivery order

| Workstream | First deliverable | Dependencies | Release gate |
| --- | --- | --- | --- |
| Transport scopes | [V1 scope contract](transport-scopes.md), shared packet parser and HMAC vectors | None | Mixed radio/bridge tests and configuration migration |
| Manual round-trip paths | [Return-path extension](manual-paths.md) | Capability negotiation | Asymmetric routes, ACK correlation and malformed-path tests |
| Sub-mesh profiles | [Profile and repeat contract](sub-meshes.md) | Scope contract, companion protocol extension | App integration, persistence and traffic-isolation tests |
| LZW messages | [Bounded codec](message-compression.md) and interoperable vectors | Capability negotiation before on-air use | App codec, authenticated receive path and fallback tests |
| Adaptive coding rate | [Radio experiment](coding-rate.md) | Chipset compatibility measurements | Hardware evidence and per-hop policy tests |
| Virtual nodes | [Host architecture](virtual-nodes.md) | Shared radio scheduling and identity dispatch | Independent identities, forwarding once and capacity tests |
| V2 protocol | [Protocol discussion draft](v2-protocol.md) | Review and independent implementation | External agreement, cryptographic review and migration tests |

V2 discussion can proceed while V1 features are implemented. It must not silently
change the meaning of V1 fields. Keep implementation PRs small and independently
reviewable; do not combine all seven features in a firmware release.

## Compatibility rules

* Default firmware continues emitting V1 packets and ordinary messages.
* Unrecognised protocol versions are rejected before payload processing.
* Optional features need an authenticated capability agreement with the recipient.
* A firmware version number alone is insufficient evidence of feature support.
* A compressed or path-bearing message must not be retried as a second logical
  message merely because its ACK timed out. Retries retain one message identifier.
* Every parser checks bounds before copying. Every table has a documented capacity.
* No runtime heap allocation after setup; codec workspaces are bounded.
* Features changing preferences need a versioned migration and reboot tests.

## App integration boundary

The repository supplied for app work is this firmware repository. Its companion
protocol is in `docs/companion_protocol.md`; it contains no Android, iOS or web app
implementation. This programme can define firmware APIs and provide app vectors.
Implementing app screens and persistence needs the actual app repository.

The manual-request implementation adds a fixed schema-1 capability record with
explicit feature revisions. Fork-only companion commands 0xF0 and 0xF1 provide
manual requests and local capability queries. Authenticated peer queries use
request type 0xF1. Other proposed command numbers remain unassigned. These fork
allocations require review before claiming upstream interoperability.

Capability records distinguish peer capability from local firmware capability.
An app must never infer recipient support from the connected companion's support.

## Acceptance environment

Native tests cover wire bounds, transport HMAC vectors with real SHA256, codecs
and policy decisions. They do not prove chipset interoperability or RF delivery.
Build companion, repeater, RS232 and ESP-NOW variants, then exercise physical
devices with mixed firmware. Record RAM, stack high-water marks, flash size,
airtime, delivery rate and battery cost.

Use one scripted topology with independently chosen outbound and return routes.
Include a scope boundary, a bridge, hash-prefix collisions, a full packet pool,
device reboot and packet loss. Run V1-only devices through the same topology.

## Decisions requiring review

1. Keep V1 transport code slot 2 reserved, or negotiate return-scope semantics?
2. Should direct routing enforce scopes? The baseline region deny-direct flag is
   reserved and must not become active implicitly.
3. Define sub-meshes as logical scopes on one RF configuration first, or add
   frequency scheduling? The proposal starts with one RF configuration.
4. Review the authenticated request capability exchange before enabling text or compression extensions.
5. Review the proposed LZW variant; fixed nine-bit codes are one bounded option.
6. Choose radio families and explicit-header settings for CR experiments.
7. Agree on V2 requirements and threat model before choosing its wire layout.
