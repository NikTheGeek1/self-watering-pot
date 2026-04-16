#include <iostream>

#include "web_content.h"

int main(int argc, char** argv) {
  const String mode = argc > 1 ? argv[1] : "dashboard";

  if (mode == "provision") {
    const String apSsid = argc > 2 ? argv[2] : "Smart-Pot-Setup-ABCD";
    const String message = argc > 3 ? argv[3] : "";
    std::cout << buildProvisioningPage(apSsid, message).c_str();
    return 0;
  }

  std::cout << buildDashboardPage().c_str();
  return 0;
}
