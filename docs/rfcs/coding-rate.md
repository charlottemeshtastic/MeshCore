# Per-hop coding rate experiment

Status: design and hardware experiment; no automatic CR selection is enabled.

## Compatibility experiment

Record chip family, driver commit, header mode, frequency, bandwidth, SF and CR.
For each supported transmitter/receiver pair, vary transmit CR 4/5 through 4/8
while holding receive settings constant. Verify actual packet decode, not merely
header detection. Repeat with implicit-header mode where supported, and with
V1 firmware receivers. Capability is specific to the measured configuration.

Begin with SX1262 and SX1276. Add LLCC68, SX1268, LR1110, LR2021 and STM32WL only
after their matrix rows pass. A successful experiment on one chip does not permit
advertising universal adaptive-CR support.

## Selection policy

Choose CR for the next physical hop using observations for that neighbor and its
full identity. Inbound SNR describes the opposite direction. Use peer-reported
outbound SNR and attributable delivery results where available. An end-to-end ACK
failure is not proof that the first radio hop was weak.

Unknown, stale or unconfirmed neighbors use the configured conservative CR.
Floods have several potential receivers and use the conservative CR. Strong-hop
selection requires a sustained margin; weak-hop selection reacts sooner. Specify
thresholds as configuration tied to SF, with hysteresis and a minimum sample
count. Do not choose a universal threshold from one field trial.

## Driver boundary

Add an optional per-transmission preparation hook. Its default retains configured
CR. Drivers report whether the measured header configuration supports this hook.
Apply CR after receive activity ends and before airtime estimation, transmit
timeouts and duty-cycle accounting. Restore configured receive settings after
success, send-start failure and timeout. Reject changes while RX/TX is active.

The setting belongs to the physical transmission, not to the encrypted message
or virtual node. A virtual host has one selection policy and airtime budget.

## Acceptance

Compare a fixed conservative CR against the policy under controlled attenuation
and a field topology. Record delivered bytes per airtime, loss, retransmissions,
energy and recovery after a link deteriorates. Test stale estimates, asymmetric
links, unknown receivers, mixed firmware and every failure cleanup path.

Enable the feature only for measured chip/header combinations. Leave the default
disabled until the experiment shows a delivery or airtime benefit.
