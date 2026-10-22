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
    status = compromised
    classification = top_secret

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
    }

    safeguards {
        automated_wakeup = enabled
        termination_protocol = pending
    }

    incident {
        signal_origin = sublevel_09
        source_identified = false

        final_record = `
At 03:17, all subjects entered REM sleep.

At 03:21, the system registered twenty-four
distinct neural signatures.

There were only twenty-three subjects.

Do not enter the lower levels.
Do not respond to voices from empty rooms.

If you hear your own voice in your sleep,
the protocol has already failed.
`
    }
}
```