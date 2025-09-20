#include "WiFi.h"
#include <vector>
#include <algorithm>

const int SCAN_INTERVAL = 10000;
const int MAX_NETWORKS_DISPLAY = 50;
const bool SORT_BY_SIGNAL = true;
const bool FILTER_DUPLICATES = true;
const int MIN_RSSI_THRESHOLD = -90;

struct NetworkInfo {
  String ssid;
  int32_t rssi;
  uint8_t channel;
  wifi_auth_mode_t encryption;
  String bssid;
  bool hidden;
};

struct ScanStats {
  int totalScans = 0;
  int totalNetworks = 0;
  int uniqueNetworks = 0;
  unsigned long startTime;
  String strongestNetwork;
  int strongestRSSI = -100;
};

ScanStats stats;
std::vector<NetworkInfo> allNetworks;
unsigned long lastScanTime = 0;
bool continuousScan = true;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  stats.startTime = millis();
  
  printHeader();
  printCommands();
}

void loop() {
  handleSerialCommands();
  
  if (continuousScan && (millis() - lastScanTime >= SCAN_INTERVAL)) {
    performWiFiScan();
    lastScanTime = millis();
  }
  
  delay(100);
}

void printHeader() {
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                         ESP32 WiFi Scanner                   ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();
}

void printCommands() {
  Serial.println("Available Commands:");
  Serial.println("  's' - Single scan");
  Serial.println("  'c' - Toggle continuous scanning");
  Serial.println("  'a' - Analyze all discovered networks");
  Serial.println("  'f' - Find specific network (SSID)");
  Serial.println("  'ch' - Show channel usage");
  Serial.println("  'sec' - Show security analysis");
  Serial.println("  'stat' - Show statistics");
  Serial.println("  'clear' - Clear network history");
  Serial.println("  'help' - Show this menu");
  Serial.println("  'export' - Export data as JSON");
  Serial.println();
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "s") {
      performWiFiScan();
    } else if (command == "c") {
      continuousScan = !continuousScan;
      Serial.println(continuousScan ? "Continuous scanning enabled" : "Continuous scanning disabled");
    } else if (command == "a") {
      analyzeAllNetworks();
    } else if (command == "f") {
      Serial.println("Enter SSID to find:");
      findSpecificNetwork();
    } else if (command == "ch") {
      showChannelUsage();
    } else if (command == "sec") {
      showSecurityAnalysis();
    } else if (command == "stat") {
      showStatistics();
    } else if (command == "clear") {
      allNetworks.clear();
      stats = ScanStats();
      stats.startTime = millis();
      Serial.println("Network history cleared");
    } else if (command == "help") {
      printCommands();
    } else if (command == "export") {
      exportDataAsJSON();
    }
  }
}

void performWiFiScan() {
  Serial.println("🔍 Scanning for WiFi networks...");
  
  int networkCount = WiFi.scanNetworks(false, true, false, 300);
  
  if (networkCount == WIFI_SCAN_FAILED) {
    Serial.println("❌ Scan failed");
    return;
  }
  
  if (networkCount == 0) {
    Serial.println("❌ No networks found");
    return;
  }
  
  stats.totalScans++;
  stats.totalNetworks += networkCount;
  
  std::vector<NetworkInfo> currentNetworks;
  
  for (int i = 0; i < networkCount; i++) {
    NetworkInfo network;
    network.ssid = WiFi.SSID(i);
    network.rssi = WiFi.RSSI(i);
    network.channel = WiFi.channel(i);
    network.encryption = WiFi.encryptionType(i);
    network.bssid = WiFi.BSSIDstr(i);
    network.hidden = (network.ssid.length() == 0);
    
    if (network.rssi >= MIN_RSSI_THRESHOLD) {
      currentNetworks.push_back(network);
      
      if (network.rssi > stats.strongestRSSI) {
        stats.strongestRSSI = network.rssi;
        stats.strongestNetwork = network.hidden ? "[Hidden Network]" : network.ssid;
      }
    }
  }
  
  if (SORT_BY_SIGNAL) {
    std::sort(currentNetworks.begin(), currentNetworks.end(), 
              [](const NetworkInfo &a, const NetworkInfo &b) {
                return a.rssi > b.rssi;
              });
  }
  
  for (const auto& network : currentNetworks) {
    if (!FILTER_DUPLICATES || !networkExists(network.bssid)) {
      allNetworks.push_back(network);
    }
  }
  
  displayScanResults(currentNetworks);
  WiFi.scanDelete();
}

bool networkExists(const String& bssid) {
  for (const auto& network : allNetworks) {
    if (network.bssid == bssid) {
      return true;
    }
  }
  return false;
}

void displayScanResults(const std::vector<NetworkInfo>& networks) {
  Serial.printf("✅ Found %d networks (showing up to %d):\n\n", 
                networks.size(), MAX_NETWORKS_DISPLAY);
  
  Serial.println("┌────┬───────────────────────────────────┬──────┬─────┬────────────┬───────────────────┬────────┐");
  Serial.println("│ #  │ SSID                          │ RSSI │ CH  │ Security   │ BSSID             │ Status │");
  Serial.println("├────┼───────────────────────────────────┼──────┼─────┼────────────┼───────────────────┼────────┤");
  
  int displayCount = std::min((int)networks.size(), MAX_NETWORKS_DISPLAY);
  
  for (int i = 0; i < displayCount; i++) {
    const NetworkInfo& network = networks[i];
    
    String ssidDisplay = network.hidden ? "[Hidden Network]" : network.ssid;
    if (ssidDisplay.length() > 29) {
      ssidDisplay = ssidDisplay.substring(0, 26) + "...";
    }
    
    String signalQuality = getSignalQuality(network.rssi);
    String securityType = getEncryptionType(network.encryption);
    
    Serial.printf("│%3d │ %-29s │ %4d │ %2d  │ %-10s │ %s │ %-6s │\n",
      i + 1,
      ssidDisplay.c_str(),
      network.rssi,
      network.channel,
      securityType.c_str(),
      network.bssid.c_str(),
      signalQuality.c_str()
    );
  }
  
  Serial.println("└────┴───────────────────────────────────┴──────┴─────┴────────────┴───────────────────┴────────┘");
  
  showQuickSummary(networks);
}

void showQuickSummary(const std::vector<NetworkInfo>& networks) {
  int openNetworks = 0;
  int wpa3Networks = 0;
  int hiddenNetworks = 0;
  
  for (const auto& network : networks) {
    if (network.encryption == WIFI_AUTH_OPEN) openNetworks++;
    if (network.encryption == WIFI_AUTH_WPA3_PSK || 
        network.encryption == WIFI_AUTH_WPA2_WPA3_PSK) wpa3Networks++;
    if (network.hidden) hiddenNetworks++;
  }
  
  Serial.printf("\n📊 Quick Summary: %d total | %d open | %d WPA3+ | %d hidden\n\n", 
                networks.size(), openNetworks, wpa3Networks, hiddenNetworks);
}

void analyzeAllNetworks() {
  if (allNetworks.empty()) {
    Serial.println("❌ No networks in history. Run a scan first.");
    return;
  }
  
  Serial.printf("📈 Analyzing %d unique networks discovered...\n\n", allNetworks.size());
  
  int securityCounts[WIFI_AUTH_MAX] = {0};
  for (const auto& network : allNetworks) {
    if (network.encryption < WIFI_AUTH_MAX) {
      securityCounts[network.encryption]++;
    }
  }
  
  Serial.println("🔐 Security Distribution:");
  if (securityCounts[WIFI_AUTH_OPEN] > 0) 
    Serial.printf("  Open: %d networks\n", securityCounts[WIFI_AUTH_OPEN]);
  if (securityCounts[WIFI_AUTH_WPA2_PSK] > 0) 
    Serial.printf("  WPA2: %d networks\n", securityCounts[WIFI_AUTH_WPA2_PSK]);
  if (securityCounts[WIFI_AUTH_WPA3_PSK] > 0) 
    Serial.printf("  WPA3: %d networks\n", securityCounts[WIFI_AUTH_WPA3_PSK]);
  if (securityCounts[WIFI_AUTH_WPA2_WPA3_PSK] > 0) 
    Serial.printf("  WPA2/WPA3: %d networks\n", securityCounts[WIFI_AUTH_WPA2_WPA3_PSK]);
  
  Serial.println();
}

void showChannelUsage() {
  if (allNetworks.empty()) {
    Serial.println("❌ No networks in history. Run a scan first.");
    return;
  }
  
  int channelCount[15] = {0};
  
  for (const auto& network : allNetworks) {
    if (network.channel >= 1 && network.channel <= 14) {
      channelCount[network.channel]++;
    }
  }
  
  Serial.println("📡 Channel Usage Analysis:");
  Serial.println("Channel │ Networks │ Bar Chart");
  Serial.println("────────┼──────────┼─────────────────────");
  
  for (int ch = 1; ch <= 14; ch++) {
    if (channelCount[ch] > 0) {
      String bar = "";
      for (int i = 0; i < channelCount[ch]; i++) {
        bar += "█";
      }
      Serial.printf("   %2d   │    %2d    │ %s\n", ch, channelCount[ch], bar.c_str());
    }
  }
  Serial.println();
}

void showSecurityAnalysis() {
  if (allNetworks.empty()) {
    Serial.println("❌ No networks in history. Run a scan first.");
    return;
  }
  
  int vulnerable = 0;
  int secure = 0;
  
  Serial.println("🛡️  Security Analysis:");
  Serial.println("SSID                          │ Security   │ Risk Level");
  Serial.println("──────────────────────────────┼────────────┼────────────");
  
  for (const auto& network : allNetworks) {
    String ssid = network.hidden ? "[Hidden]" : network.ssid;
    if (ssid.length() > 29) ssid = ssid.substring(0, 26) + "...";
    
    String security = getEncryptionType(network.encryption);
    String risk;
    
    if (network.encryption == WIFI_AUTH_OPEN) {
      risk = "HIGH";
      vulnerable++;
    } else if (network.encryption == WIFI_AUTH_WEP) {
      risk = "HIGH";
      vulnerable++;
    } else if (network.encryption == WIFI_AUTH_WPA_PSK) {
      risk = "MEDIUM";
      vulnerable++;
    } else {
      risk = "LOW";
      secure++;
    }
    
    Serial.printf("%-29s │ %-10s │ %-10s\n", ssid.c_str(), security.c_str(), risk.c_str());
  }
  
  Serial.printf("\n📊 Summary: %d secure, %d potentially vulnerable\n\n", secure, vulnerable);
}

void showStatistics() {
  unsigned long uptime = (millis() - stats.startTime) / 1000;
  
  Serial.println("📈 Scanner Statistics:");
  Serial.printf("  Uptime: %lu seconds\n", uptime);
  Serial.printf("  Total scans performed: %d\n", stats.totalScans);
  Serial.printf("  Total networks detected: %d\n", stats.totalNetworks);
  Serial.printf("  Unique networks discovered: %d\n", allNetworks.size());
  Serial.printf("  Strongest signal: %s (%d dBm)\n", 
                stats.strongestNetwork.c_str(), stats.strongestRSSI);
  
  if (stats.totalScans > 0) {
    Serial.printf("  Average networks per scan: %.1f\n", 
                  (float)stats.totalNetworks / stats.totalScans);
  }
  Serial.println();
}

void exportDataAsJSON() {
  if (allNetworks.empty()) {
    Serial.println("❌ No data to export.");
    return;
  }
  
  Serial.println("📄 JSON Export:");
  Serial.println("{");
  Serial.println("  \"scan_data\": [");
  
  for (size_t i = 0; i < allNetworks.size(); i++) {
    const NetworkInfo& network = allNetworks[i];
    Serial.printf("    {\n");
    Serial.printf("      \"ssid\": \"%s\",\n", network.hidden ? "" : network.ssid.c_str());
    Serial.printf("      \"rssi\": %d,\n", network.rssi);
    Serial.printf("      \"channel\": %d,\n", network.channel);
    Serial.printf("      \"security\": \"%s\",\n", getEncryptionType(network.encryption).c_str());
    Serial.printf("      \"bssid\": \"%s\",\n", network.bssid.c_str());
    Serial.printf("      \"hidden\": %s\n", network.hidden ? "true" : "false");
    Serial.printf("    }%s\n", i < allNetworks.size() - 1 ? "," : "");
  }
  
  Serial.println("  ]");
  Serial.println("}");
  Serial.println();
}

void findSpecificNetwork() {
  Serial.println("Waiting for SSID input...");
  
  // Wait for user input with timeout
  unsigned long timeout = millis() + 10000; // 10 second timeout
  while (!Serial.available() && millis() < timeout) {
    delay(10);
  }
  
  if (!Serial.available()) {
    Serial.println("❌ Timeout waiting for input");
    return;
  }
  
  String searchSSID = Serial.readStringUntil('\n');
  searchSSID.trim();
  
  if (searchSSID.length() == 0) {
    Serial.println("❌ Invalid SSID");
    return;
  }
  
  Serial.printf("🔍 Searching for network: %s\n", searchSSID.c_str());
  
  bool found = false;
  for (const auto& network : allNetworks) {
    if (network.ssid.equalsIgnoreCase(searchSSID)) {
      Serial.println("✅ Network found:");
      Serial.printf("  SSID: %s\n", network.ssid.c_str());
      Serial.printf("  RSSI: %d dBm (%s)\n", network.rssi, getSignalQuality(network.rssi).c_str());
      Serial.printf("  Channel: %d\n", network.channel);
      Serial.printf("  Security: %s\n", getEncryptionType(network.encryption).c_str());
      Serial.printf("  BSSID: %s\n", network.bssid.c_str());
      found = true;
      break;
    }
  }
  
  if (!found) {
    Serial.println("❌ Network not found in scan history");
  }
  Serial.println();
}

String getSignalQuality(int rssi) {
  if (rssi > -30) return "EXCEL";
  else if (rssi > -50) return "GOOD";
  else if (rssi > -70) return "FAIR";
  else return "POOR";
}

String getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN: return "Open";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI_PSK";
    default: return "Unknown";
  }
}
