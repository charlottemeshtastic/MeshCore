# Sub-mesh profiles and client repeating

Status: proposed companion extension building on implemented repeat preferences.

## Profile model

Start with logical scopes sharing one frequency, bandwidth and spreading factor.
This permits simultaneous receive without pretending a single radio listens on
several frequencies. Frequency-specific profiles require a separate rendezvous
and scheduling design.

A profile contains a stable local ID, display name, scope name/key reference,
channel references, default send policy and repeat policy. Keep full peer public
keys as contact identities; short path hashes do not identify a profile.
Use fixed capacities with explicit "table full" responses. Profile identifiers
must survive reboot and must not be silently reused while queued messages exist.

Queued sends retain a snapshot of profile selection. Changing the selected
profile must not retag pending sends. Incoming messages carry the matched local
profile ID to the app. Scope-token collisions can yield several candidates;
payload authentication determines the applicable channel/contact where possible.
Do not silently pick the first profile on a collision.

## Repeat policy

The baseline companion has `isRepeatEn`, `CMD_SET_RADIO_PARAMS` repeat control,
allowed-repeat frequency queries and preference persistence. Extend that path.
Do not create a second repeat preference with conflicting defaults.

Repeating is opt-in per profile. Require matching scope, configured hop limits,
deduplication and a shared airtime budget. Unscoped traffic has its own explicit
setting. Preserve the firmware's allowed-repeat frequency checks. Disabling
repeating stops queued forwards without cancelling locally originated messages.

An off-grid profile can permit temporary repeating with a configured expiration
or session lifetime. Reboot behavior is explicit. Do not infer off-grid status
solely from loss of a phone's internet connection.

## Companion and app work

After capability query, add list/get/upsert/remove/select profile operations and
include profile identity in send/receive operations. Use length-delimited records,
schema revisions and explicit errors. The capability reports profile capacity
and supported repeat controls. Command numbers remain unassigned in this draft.

Apps provide a profile switcher, profile-aware conversation filters, repeat
controls and a visible repeat expiration. Key material must not appear in logs
or a profile list response. Legacy clients continue using the default profile.

## Persistence and acceptance

Use a versioned preference envelope with a transactional save. Migrate the
baseline default flood scope into one default profile. On failed migration,
retain the readable source configuration rather than silently erasing it.

Test queue isolation while switching profiles, channel/key isolation, duplicate
packets across profile candidates, full tables, deletion with pending sends,
repeat expiry, reboot and V1 app interoperability. RF checks measure forwarding
airtime and verify that a client does not multiply repeated packets per profile.
