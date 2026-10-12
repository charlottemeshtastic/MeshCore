# Round-trip manual paths

Status: proposed extension. No wire identifier is allocated by this draft.

## Behavior

A caller supplies an outbound route and an independent return route for one
transaction. Each route carries its own hash width and count. A zero-hop route
is valid. Missing return-route information differs from an explicit zero-hop
route; use a presence flag rather than an empty-array sentinel.

Repeaters consume the outbound route using V1 direct-routing rules. The endpoint
reads the return route only after authenticating/decrypting the transaction.
Replies and delivery ACKs use that supplied route without overwriting the
contact's learned route. A subsequent transaction can choose a different route.

Do not reverse the outbound path as a fallback. RF links can be asymmetric.
Timeout fallback to flooding is opt-in, retains the transaction identifier, and
uses the transaction's explicit scope policy.

## Proposed authenticated envelope

Define an endpoint extension carrying an envelope revision, transaction ID,
flags, optional return path metadata and bytes, body encoding, body length and
body. Agree on field widths and payload subtype after capability exchange is
specified. Do not allocate a reserved V1 header bit locally.

Validate metadata with `Packet::isValidPathLen`. Require every path byte to be
present, reject duplicate fields, and reject envelope plus body exceeding the
encrypted plaintext budget. Include the return path and body encoding in the
authenticated data. Mutable repeater path bytes remain outside this envelope.

The ACK binds the transaction ID and logical message contents. Document whether
the retry attempt participates in that binding, and preserve the baseline retry
semantics when translating ordinary V1 messages.

## Interfaces

Add a bounded `RoundTripRoute` value containing both paths and an explicit
return-route presence flag. Provide a send overload taking this value. A reply
context stores the authenticated return path only for the active transaction.
It does not retain a pointer into a freed packet.

Expose separate outbound and return fields in companion commands. Apps display
both paths, allow trace verification, and explain that route order is traversal
order. All path lengths use encoded metadata, not a raw-byte-count assumption.

The repeater already accepts supplied return paths in selected anonymous direct
requests. Audit their plaintext length checks before generalising that behavior.

## Acceptance

Exercise A -> R1 -> R2 -> B with B -> R3 -> A for the return route. Test widths
1, 2 and 3, zero hops, invalid width 4, 64-byte limits, absent route, truncated
route, ACK loss, retries and duplicate receive. The learned contact route must
remain unchanged after a transaction-scoped manual reply.

Unknown peers receive ordinary V1 traffic. A peer that claims the extension but
rejects its envelope returns an authenticated error where possible; it must not
interpret envelope bytes as display text.
