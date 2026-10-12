# Multiple logical nodes on one radio

Status: proposed architecture. The baseline Mesh class owns one `self_id`.

## Host and logical node boundary

One physical host owns the radio, packet pool, inbound/outbound scheduling,
forwarding deduplication, duty-cycle budget and radio settings. Logical nodes own
identities, peer secrets, contacts, application handlers and per-node preferences.
The host registers nodes in a fixed-capacity table during setup.

Refactor packet construction and endpoint dispatch out of the single-identity
Mesh object before registering several independent Mesh objects. Calling several
Mesh loops against one Radio is insufficient: one loop can consume another's
packet, and each dispatcher would account airtime independently.

## Receive flow

The host parses once and computes physical forwarding once. It supplies an
immutable view to candidate endpoints. Short destination/channel hashes can
collide; resolve all bounded candidates through authentication. A logical node
receiving a packet does not suppress another intended local receiver merely
because the physical forwarding deduplication table has seen it.

Decrypt into separate bounded workspace. Do not let handlers mutate the shared
packet path or ciphertext. Keep a distinct delivery-deduplication domain for each
logical identity. Local delivery between nodes should use the same authentication
and message identity semantics without unnecessary RF transmission.

## Send flow

Outbound construction takes an explicit source identity and node handle. Queue
entries retain the node handle and routing context. The host arbitrates fair
access under one physical airtime budget; priorities must not let one logical
node starve every other node.

Forwarding advertises one physical forwarding identity. Logical endpoint adverts
are separate and rate-limited. Define whether endpoint identities should ever be
used as repeater path hashes; the initial proposal keeps them out of paths.

## Lifecycle and storage

Start with setup-only registration. Persist each node's keys and state under a
stable namespace. A failed load disables that node with an explicit status.
Do not copy the host key into every node. Runtime removal requires cancelling
queued ownership and invalidating handles before freeing state.

Budget RAM by node count, contact capacity, crypto workspace and packet pool.
Build-time limits must fit the smallest supported target. Avoid multiplying full
radio dispatcher instances as the cost of adding an endpoint.

## Acceptance

Run two endpoint identities with distinct peers and keys on one device. Test hash
collisions, local delivery, RF delivery, node-specific duplicates, forwarding
once, queue starvation, pool exhaustion and reboot. One identity's contact or key
must never be used to decrypt or ACK a message belonging to the other.
