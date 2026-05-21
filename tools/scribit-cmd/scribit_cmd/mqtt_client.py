from __future__ import annotations

import paho.mqtt.client as mqtt


def mqtt_pub(host: str, port: int, user: str, pw: str, topic: str, msg: str) -> None:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if user or pw:
        client.username_pw_set(user, pw)

    rc = client.connect(host, port, keepalive=10)
    if rc != mqtt.MQTT_ERR_SUCCESS:
        raise RuntimeError(f"MQTT connect failed: {mqtt.error_string(rc)}")

    client.loop_start()
    try:
        info = client.publish(topic, payload=msg, qos=0, retain=False)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed: {mqtt.error_string(info.rc)}")
        info.wait_for_publish(timeout=5.0)
        if not info.is_published():
            raise TimeoutError(f"MQTT publish timed out for topic {topic!r}")
    finally:
        client.disconnect()
        client.loop_stop()

