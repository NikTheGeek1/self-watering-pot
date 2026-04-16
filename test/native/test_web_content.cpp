#include <gtest/gtest.h>

#include "web_content.h"

TEST(WebContentTest, DashboardIncludesManualWateringButtonAndHistoryCard) {
  const String html = buildDashboardPage();

  EXPECT_NE(html.std().find("Manual Watering"), std::string::npos);
  EXPECT_NE(html.std().find("Recent Watering"), std::string::npos);
  EXPECT_NE(html.std().find("/api/manual-water"), std::string::npos);
}

TEST(WebContentTest, ProvisioningPageEscapesUserFacingValues) {
  const String html = buildProvisioningPage("Smart<&>", "Need \"Wi-Fi\" <retry>");

  EXPECT_NE(html.std().find("Smart&lt;&amp;&gt;"), std::string::npos);
  EXPECT_NE(html.std().find("Need &quot;Wi-Fi&quot; &lt;retry&gt;"), std::string::npos);
}
