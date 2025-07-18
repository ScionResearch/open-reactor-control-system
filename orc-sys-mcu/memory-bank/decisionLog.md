# Decision Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2025-07-17 | Added all status topics from MQTT_TOPICS.md to the mqttTopics array in mqttManager.cpp, with simple getter functions using the status struct. Now all documented topics are published as MQTT outputs from this processor only. | This aligns the code with the documentation, ensures all required system status and sensor values are published, and keeps the implementation simple and outbound-only as requested. |
