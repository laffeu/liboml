<img src="assets/banner.png" alt="liboml banner" width="460">

## Build & Install
```bash
git clone https://github.com/laffeu/liboml.git
cd liboml
make && make install
```

## Quick Start

### Configuration File

```oml
name = MORPHEUS

protocol {
    designation = MP-047
    classification = top_secret
    status = compromised
    authorization = level_4

    containment {
        object_class = keter
        threat_level = critical
        breach_confirmed = true
        affected_sectors = [oneiros, hypnos, lethe]
    }

    research {
        primary_subject = REM-047
        subjects_enrolled = 23
        subjects_missing = 8
        neural_activity = anomalous
        dream_synchronization = true
    }

    safeguards {
        personnel_sleeping = prohibited
        automated_wakeup = enabled
        termination_protocol = pending
    }

    incident {
        signal_origin = sublevel_09
        source_identified = false

        final_record = `
INCIDENT REPORT MP-047
RECOVERY STATUS: PARTIAL

At 03:17, all subjects entered REM sleep simultaneously.
No external stimulus was detected.

At 03:21, the monitoring system registered
twenty-four distinct neural signatures.

There were only twenty-three subjects.

At 03:24, the additional signature began to awaken.

Do not enter the lower levels.
Do not respond to voices originating from empty rooms.

If you hear your own voice speaking in your sleep,
the protocol has already failed.

END OF RECORD
`
    }
}
```