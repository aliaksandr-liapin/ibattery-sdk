# Battery SDK Phase 0 Firmware Template

This template is intentionally minimal and reusable for battery experiments.

## Files
- `main.c` - boot log + heartbeat loop + ADC hook point
- `battery_adc.c` - placeholder ADC implementation
- `battery_adc.h` - ADC interface
- `app_config.h` - small configuration surface

## Expected runtime behavior
- Prints a boot banner once
- Prints heartbeat every 3 seconds
- Prints that ADC sampling is disabled until the ADC step begins

## Serial logging verification target
You should see logs similar to:

```text
Battery SDK experiment template booting
Phase 0 firmware baseline started
battery ADC placeholder initialized
Heartbeat #1
Battery ADC sampling disabled in app_config.h
Heartbeat #2
Battery ADC sampling disabled in app_config.h
```

## Next Phase 0 step
Replace placeholder ADC logic with real SAADC measurement and wire battery input safely.
