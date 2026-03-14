"""Real-time anomaly checks on individual decoded telemetry packets.

These checks run inline during streaming — no InfluxDB query needed.
"""

import logging

logger = logging.getLogger(__name__)


def check_realtime(decoded: dict) -> list[dict]:
    """Run quick threshold checks on a single decoded packet.

    Args:
        decoded: Dictionary from decoder.decode_packet().

    Returns:
        List of anomaly dicts (empty if all normal).
    """
    anomalies = []

    voltage_v = decoded.get("voltage_v", 0.0)
    temperature_c = decoded.get("temperature_c", 25.0)
    soc_pct = decoded.get("soc_pct", 0.0)

    # Critical: very low voltage
    if voltage_v < 3.0:
        anomalies.append({
            "type": "voltage_critical",
            "severity": "critical",
            "value": voltage_v,
            "threshold": 3.0,
            "message": f"Very low voltage: {voltage_v:.3f}V",
        })
    # Warning: low voltage with high SoC (inconsistency)
    elif voltage_v < 3.2 and soc_pct > 30.0:
        anomalies.append({
            "type": "soc_inconsistency",
            "severity": "warning",
            "value": voltage_v,
            "threshold": 3.2,
            "message": f"Voltage {voltage_v:.3f}V but SoC {soc_pct:.1f}% — inconsistent",
        })

    # High temperature
    if temperature_c > 45.0:
        anomalies.append({
            "type": "temp_high",
            "severity": "warning",
            "value": temperature_c,
            "threshold": 45.0,
            "message": f"High temperature: {temperature_c:.1f}C",
        })

    # Low temperature (LiPo danger zone)
    if temperature_c < 0.0:
        anomalies.append({
            "type": "temp_low",
            "severity": "critical",
            "value": temperature_c,
            "threshold": 0.0,
            "message": f"Low temperature: {temperature_c:.1f}C — LiPo danger",
        })

    return anomalies
