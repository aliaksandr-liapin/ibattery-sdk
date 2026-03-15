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

    # Critical: very low voltage (below 2.5V — dangerous for any chemistry)
    if voltage_v < 2.5:
        anomalies.append({
            "type": "voltage_critical",
            "severity": "critical",
            "value": voltage_v,
            "threshold": 2.5,
            "message": f"Very low voltage: {voltage_v:.3f}V",
        })
    # Warning: low voltage with high SoC (inconsistency)
    # Threshold 2.8V works for both CR2032 (~3.0V nominal) and LiPo (~3.7V nominal)
    elif voltage_v < 2.8 and soc_pct > 50.0:
        anomalies.append({
            "type": "soc_inconsistency",
            "severity": "warning",
            "value": voltage_v,
            "threshold": 2.8,
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
