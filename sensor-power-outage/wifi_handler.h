# if SSL==true
extern WiFiClientSecure wifi_client;
# else 
extern WiFiClient wifi_client;
# endif

extern String mac_address;

void wifi_init_ca();

void wifi_connect();

void wifi_reconnect();

int wifi_get_status();