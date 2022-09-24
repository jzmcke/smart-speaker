#include "setup_wifi.h"

WiFiMulti wifiMulti;

void
setup_wifi_setup(void)
{
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP("our blades are sharp", "longenoughnottocrack");
    wifiMulti.addAP("our blades are sharp (5G)", "longenoughnottocrack");
    Serial.println(WiFi.localIP());
}

void
setup_wifi_reconnect(void)
{
  int b_lost_connection = 0;
  while (wifiMulti.run() != WL_CONNECTED) {
      b_lost_connection = 1;
		  delay(100);
	}

  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
    Serial.println("WiFi connected");
  }
  if (b_lost_connection)
  {
    Serial.println("    ... connection re-established.");
  }
}