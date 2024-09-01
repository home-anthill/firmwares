extern PubSubClient mqtt_client;

void mqtt_init(Client& wifi_client);

void mqtt_connect(String uuid);

void mqtt_notify_value(String uuid, String type, float value);

void mqtt_notify_online(String uuid);