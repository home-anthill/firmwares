# if SSL==true
extern WiFiClientSecure wifi_client;
# else 
extern WiFiClient wifi_client;
# endif

void wifi_init_ca();

void wifi_connect(char* mac_address);

void wifi_reconnect(char* mac_address);

int wifi_get_status();