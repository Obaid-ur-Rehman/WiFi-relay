#include "WebPortal.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "Json.h"



String WebPortal::toStringIp(IPAddress ip)
{
	String res = "";
	for (int i = 0; i < 3; i++) {
		res += String((ip >> (8 * i)) & 0xFF) + ".";
	}
	res += String(((ip >> 8 * 3)) & 0xFF);
	return res;
}


boolean WebPortal::isIp(String str)
{
	for (size_t i = 0; i < str.length(); i++) {
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9')) {
			return false;
		}
	}
	return true;
}



WebPortal::WebPortal() :ESP8266WebServer()
{

}

WebPortal::~WebPortal()
{
}

void WebPortal::setup() {
	//Generate unique device name based on MAC address of WIFI adapter
	String mac = "relay" + WiFi.macAddress(); mac.replace(":", "");
	myHostname = (char *)calloc(mac.length() + 1, 1);
	strncpy(myHostname, mac.c_str(), mac.length());

	Serial.print("Host name: http://"); Serial.print(myHostname); Serial.println(".local");

	//Make device visible in for Microsoft Windows Network
	ssdp(myHostname);

	Serial.println("reading wifi config....");
	bool setupAP = true;
	//read wifi config
	if (SPIFFS.exists("/wifi.json")) {
		File f = SPIFFS.open("/wifi.json", "r");
		f.seek(0);
		JsonString json = JsonString(f.readString());
		String wlan_id = json.getValue("wlan_id");
		if (wlan_id.length() > 0) {
			server.ssid = wlan_id.c_str();
			server.password = json.getValue("wlan_pwd").c_str();
			Serial.println("wifi config OK.");
			setupAP = false;
		}
		else {
			Serial.println("No wlan id found...");
		}
		f.close();
	}
	else {
		Serial.println("WiFi config not found!");
	}
	pinMode(A0, INPUT_PULLUP);
	int a0 = analogRead(A0);
	Serial.printf("A0=%i\n", a0);
	if (a0 > 1000) {
		setupAP = true;
		if (SPIFFS.exists("/wifi.json")) SPIFFS.remove("/wifi.json");
	}

	if (setupAP) {
		Serial.println("Start portal");
		WiFi.disconnect();

		Serial.println();
		Serial.println("Configuring access point...");
		/* You can remove the password parameter if you want the AP to be open. */
		WiFi.softAPConfig(apIP, apIP, netMsk);
		WiFi.softAP(myHostname, softAP_password);
		delay(500); // Without delay I've seen the IP address blank
		Serial.print("AP IP address: ");
		Serial.println(WiFi.softAPIP());

		/* Setup the DNS server redirecting all the domains to the apIP */
		dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
		dnsServer.start(DNS_PORT, "*", apIP);
	}

	/* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
	on("/", handleRoot);
	on("/api/wifi", handleWifi);
	on("/api/wifisave", handleWifiSave);
	on("/api/template", handleTemplate);
	on("/api/setup", handleSetup);
	on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
	on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
	on("/update", handleUpdate);
	onNotFound(handleNotFound);
	begin(); // Web server start
	Serial.println("HTTP server started");

	connect = ssid.length() > 0; // Request WLAN connect if there is a SSID
}

void WebPortal::connectWifi() {
	Serial.println("Connecting as wifi client...");
	WiFi.disconnect();
	WiFi.begin(ssid.c_str(), password.c_str());
	int connRes = WiFi.waitForConnectResult();
	Serial.print("connRes: ");
	Serial.println(connRes);
}

void WebPortal::ssdp(const char* deviceName) {
	Serial.println("Setup SSDP....");
	//SSDP makes device visible on windows network
	on("/description.xml", HTTP_GET, [&]() {
		SSDP.schema(server.client());
	});
	SSDP.setSchemaURL("description.xml");
	SSDP.setHTTPPort(80);
	SSDP.setName(deviceName);
	SSDP.setModelName("ESP8266 based wifi relay");

	SSDP.setURL("/");
	SSDP.begin();
	SSDP.setDeviceType("upnp:rootdevice");
} //setup





/** Handle root or redirect to captive portal */
void WebPortal::handleRoot() {
	if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
		return;
	}
	handleFileRead("/index.html");
}

bool WebPortal::handleFileRead(String path) {
	if (path.endsWith("/")) path += "index.html";
	if (path.equals("/favicon.ico")) path = "icon.svg";
	char* contentType = server.getContentType(path);

	String s = "/html" + path;
	Serial.println("path=" + s);
	if (SPIFFS.exists(s)) {
		File file = SPIFFS.open(s, "r");
		server.sendFile(file, contentType, false);
		file.close();
		return true;
	}
	Serial.println("Not found!!! " + path);
	return false;
}

char* WebPortal::getContentType(String filename) {
	if (hasArg("download")) return "application/octet-stream";
	else if (filename.endsWith(".htm")) return "text/html";
	else if (filename.endsWith(".html")) return "text/html";
	else if (filename.endsWith(".css")) return "text/css";
	else if (filename.endsWith(".js")) return "application/javascript";
	else if (filename.endsWith(".png")) return "image/png";
	else if (filename.endsWith(".gif")) return "image/gif";
	else if (filename.endsWith(".jpg")) return "image/jpeg";
	else if (filename.endsWith(".ico")) return "image/x-icon";
	else if (filename.endsWith(".xml")) return "text/xml";
	else if (filename.endsWith(".svg")) return "image/svg+xml";
	else if (filename.endsWith(".pdf")) return "application/x-pdf";
	else if (filename.endsWith(".zip")) return "application/x-zip";
	else if (filename.endsWith(".gz")) return "application/x-gzip";
	return "text/plain";
}

void WebPortal::sendFile(File file, char* contenttype, bool addGzHeader) {
	String header;
	size_t size_to_send = file.size();
	Serial.println("file=" + String(file.name()) + " filesize=" + String(size_to_send));
	if (addGzHeader) {
		sendHeader("Content-Encoding", "gzip"); // console.print("Header1 ");
		//sendHeader("Cache-Control", "max-age=31536000", true);
	}
	sendHeader("Cache-Control", "max-age=31536000", true);
	_prepareHeader(header, 200, contenttype, size_to_send);
	sendContent(header);
	//	console.print("Header2 ");

	const size_t unit_size = 1024;
	char dataDownloadBuf[unit_size];
	while (size_to_send) {
		size_t will_send = (size_to_send < unit_size) ? size_to_send : unit_size;
		//	console.print("Read ");
		file.readBytes(&dataDownloadBuf[0], will_send);
		//	console.print("Send ");
		size_t sent = _currentClient.write(&dataDownloadBuf[0], will_send);
		//	console.println("Next ");
		if (sent == 0) {
			break;
		}
		size_to_send -= sent;
	}
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean WebPortal::captivePortal() {
	if (!WebPortal::isIp(server.hostHeader()) && server.hostHeader() != (String(server.myHostname) + ".local")) {
		Serial.println("Request redirected to captive portal");
		server.sendHeader("Location", String("http://") + WebPortal::toStringIp(server.client().localIP()), true);
		server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
		server.client().stop(); // Stop is needed because we sent no content length
		return true;
	}
	return false;
}

/** Wifi config page handler */
void WebPortal::handleWifi() {

	Serial.println("scan start");
	int n = WiFi.scanNetworks();

	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	Serial.println("scan done");
	JsonString ret = JsonString();
	ret.beginObject();
	if (n > 0) {
		ret.beginArray("ssid");
		for (int i = 0; i < n; i++) {
			ret.beginObject();
			ret.AddValue("name", WiFi.SSID(i));
			ret.AddValue("encryption", String(WiFi.encryptionType(i)));
			ret.AddValue("rssi", String(WiFi.RSSI(i)));
			//server.sendContent(String() + "\r\n<tr><td>SSID " + WiFi.SSID(i) + String((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : " *") + " (" + WiFi.RSSI(i) + ")</td></tr>");
			ret.endObject();
		}
		ret.endArray();
	};
	ret.endObject();
	server.send(200, "application/json", ret);
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WebPortal::handleWifiSave() {
	Serial.println("wifi save");
	server.ssid = server.arg("n");
	server.password = server.arg("p");
	server.sendHeader("Location", "/", true);
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server.client().stop(); // Stop is needed because we sent no content length

	if (server.ssid.length() == 0) {
		SPIFFS.remove("/wifi.json");
		Serial.println("wifi config removed");
	}
	else {
		File f = SPIFFS.open("/wifi.json", "w");
		f.seek(0);
		f.print("{\"wlan_id\":\"");
		f.print(server.ssid);
		f.print("\",\"wlan_pwd\":\"");
		f.print(server.password);
		f.println("\"}");
		f.flush();
		f.close();

		Serial.print("{\"wlan_id\":\"");
		Serial.print(server.ssid);
		Serial.print("\",\"wlan_pwd\":\"");
		Serial.print(server.password);
		Serial.println("\"}");
		Serial.println("OK!");
		Serial.println("wifi config saved.");


	}

	//saveCredentials();
	//server.connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID

}




uint16_t httpsPort = 443;
const char * host = "raw.githubusercontent.com";//TOD: read from settings file
const char * fingerprint = "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33";//TOD: read from settings file (can see in browser certificate info)

void WebPortal::handleUpdate() {
	String url = "";
	if (server.hasArg("url")) {
		url = server.arg("url");
	}
	else {
		if (SPIFFS.exists("/html/files.txt")) {
			File f = SPIFFS.open("/html/files.txt", "r");
			url = f.readStringUntil('\10');
			f.close();
		}
	}
	if (url.length() > 0) {
		server.send(200, "application/json", "{\"status\":\"ok\"}");
		Serial.printf("url=%s", url.c_str());
		updateFiles(url);
	}
	else {
		handleNotFound();
		return;
	}
}

void WebPortal::handleTemplate() {
	String name = "";
	if (server.hasArg("name")) {
		name = server.arg("name");
		String path = "/html/content/_" + name + ".html";
		Serial.printf("path=%s\n", path.c_str());

		if (SPIFFS.exists(path)) {
			Serial.printf("exist=%s\n", path.c_str());
			char* contentType = server.getContentType(path);
			File f = SPIFFS.open(path, "r");
			server.sendFile(f, contentType, false);
			f.close();
			return;
		}
	}
	handleNotFound();
}


void WebPortal::handleSetup() {
	String name = "";
	if (server.hasArg("type")) {
		name = server.arg("type");
		Serial.printf("setup: name=%s\n", name.c_str());
		int index = server.arg("index").toInt();

		JsonString ret = "";

		ret.beginObject();

		ret.beginArray("items");
		ret.endArray();

		ret.endObject();


		Serial.printf("setup: ret=%s\n", ret.c_str());

		server.send(200, "application/json", ret);
		return;
	}
	handleNotFound();
}



void WebPortal::updateFiles(String url) {
	updateFile(url, "/html/files.txt");
	if (SPIFFS.exists("/html/files.txt")) {
		File f = SPIFFS.open("/html/files.txt", "r");
		url = f.readStringUntil('\n');
		String fName = f.readStringUntil('\n');
		while (fName.length() > 0) {
			updateFile(url, fName);
			fName = f.readStringUntil('\n');
		}
		f.close();
	}
}

void WebPortal::updateFile(String url, String file) {


	WiFiClientSecure client;
	Serial.print("connecting to ");
	Serial.println(host);
	if (!client.connect(host, httpsPort)) {
		Serial.println("connection failed");
		return;
	}
	if (client.verify(fingerprint, host)) {
		Serial.println("certificate matches");
	}
	else {
		Serial.println("certificate doesn't match");
	}

	String action = url.substring(url.indexOf(host) + strlen(host)) + file;

	client.print(String("GET ") + action + " HTTP/1.1\r\n" +
		"Host: " + host + "\r\n" +
		"User-Agent: ESP8266\r\n" +
		"Connection: close\r\n\r\n");

	long len = -1;
	while (client.connected()) {
		String line = client.readStringUntil('\n');
		if (line.startsWith("Content-Length:")) {
			len = line.substring(15).toInt();
		}
		if (line == "\r") {
			Serial.println("headers received");
			break;
		}
	}

	Serial.printf("[HTTPS] GET: %s\n", action.c_str());
	File f = SPIFFS.open(file, "w");
	if (f) {
		uint8_t buff[128];
		while (client.connected() && (len > 0 || len == -1)) {
			size_t size = client.available();
			if (size) {
				int c = client.readBytes(buff, ((size > 128) ? 128 : size));
				f.write(buff, c);
				if (len > 0) 	len -= c;
			}
			else { break; }
			Serial.printf("bytes left %i", len);
			ESP.wdtDisable();
			ESP.wdtEnable(1000);
			//f.println(line);
			//line = client.readStringUntil('\n');
		}
		Serial.println("done.");
		f.close();
	}
}

void WebPortal::handleNotFound()
{
	if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
		return;
	}
	if (handleFileRead(server.uri())) {
		return;
	}
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.send(404, "text/plain", message);
}


void WebPortal::loop() {
	if (connect) {
		Serial.println("Connect requested");
		connect = false;
		connectWifi();
		lastConnectTry = millis();
	}
	{
		unsigned int s = WiFi.status();
		if (s == 0 && millis() > (lastConnectTry + 60000)) {
			/* If WLAN disconnected and idle try to connect */
			/* Don't set retry time too low as retry interfere the softAP operation */
			connect = true;
		}
		if (status != s) { // WLAN status change
			Serial.print("Status: ");
			Serial.println(s);
			status = s;
			if (s == WL_CONNECTED) {
				/* Just connected to WLAN */
				Serial.println("");
				Serial.print("Connected to ");
				Serial.println(ssid);
				Serial.print("IP address: ");
				Serial.println(WiFi.localIP());

				// Setup MDNS responder
				if (!MDNS.begin(myHostname)) {
					Serial.println("Error setting up MDNS responder!");
				}
				else {
					Serial.println("mDNS responder started");
					// Add service to MDNS-SD
					MDNS.addService("http", "tcp", 80);
				}
			}
			else if (s == WL_NO_SSID_AVAIL) {
				WiFi.disconnect();
			}
		}
	}
	// Do work:
	//DNS
	dnsServer.processNextRequest();
	//HTTP
	handleClient();
}


WebPortal server;