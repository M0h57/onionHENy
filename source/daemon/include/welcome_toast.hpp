#pragma once

#include <onion/debug_settings_route_policy.hpp>

#include <string>
#include <string_view>

namespace onion::daemon {

inline constexpr const char kWelcomeToastToolboxUriToken[] =
    "__ONIONHEN_TOOLBOX_URI__";

inline constexpr const char kWelcomeToastJsonTemplate[] =
    "{\n"
    "  \"rawData\": {\n"
    "    \"viewTemplateType\": \"InteractiveToastTemplateB\",\n"
    "    \"channelType\": \"Downloads\",\n"
    "    \"useCaseId\": \"IDC\",\n"
    "    \"toastOverwriteType\": \"No\",\n"
    "    \"isImmediate\": true,\n"
    "    \"priority\": 100,\n"
    "    \"viewData\": {\n"
    "      \"icon\": {\n"
    "        \"type\": \"Url\",\n"
    "        \"parameters\": {\n"
    "          \"url\": \"/user/data/OnionHEN/onionhen.png\"\n"
    "        }\n"
    "      },\n"
    "      \"message\": {\n"
    "        \"body\": \"OnionHEN\"\n"
    "      },\n"
    "      \"subMessage\": {\n"
    "        \"body\": \"Welcome to OnionHEN\"\n"
    "      },\n"
    "      \"actions\": [\n"
    "        {\n"
    "          \"actionName\": \"Go to the OnionHEN Toolbox\",\n"
    "          \"actionType\": \"DeepLink\",\n"
    "          \"defaultFocus\": true,\n"
    "          \"parameters\": {\n"
    "            \"actionUrl\": \"__ONIONHEN_TOOLBOX_URI__\"\n"
    "          }\n"
    "        }\n"
    "      ]\n"
    "    },\n"
    "    \"platformViews\": {\n"
    "      \"previewDisabled\": {\n"
    "        \"viewData\": {\n"
    "          \"icon\": {\n"
    "            \"type\": \"Predefined\",\n"
    "            \"parameters\": {\n"
    "              \"icon\": \"download\"\n"
    "            }\n"
    "          },\n"
    "          \"message\": {\n"
    "            \"body\": \"OnionHEN Running\"\n"
    "          }\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "  },\n"
    "  \"createdDateTime\": \"2025-12-14T03:14:51.473Z\",\n"
    "  \"localNotificationId\": \"588193127\"\n"
    "}";

inline std::string make_welcome_toast_json(std::string_view toolbox_uri) {
  std::string json = kWelcomeToastJsonTemplate;
  const std::string_view replacement =
      toolbox_uri.empty()
          ? std::string_view(
                onion::debug_settings_route::kStandardRoute.simple_uri)
          : toolbox_uri;

  const size_t pos = json.find(kWelcomeToastToolboxUriToken);
  if (pos != std::string::npos) {
    json.replace(pos, sizeof(kWelcomeToastToolboxUriToken) - 1,
                 replacement.data(), replacement.size());
  }
  return json;
}

} // namespace onion::daemon
