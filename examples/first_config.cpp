#include "loml.hpp"
#include <iostream>
#include <stdexcept>

using std::cout;
using std::endl;

int main() {
    try {
        // Load sample.oml with changes from changes.oml.
        loml::table cfg("sample.oml");

        // Get the section with top-level keys.
        auto root = cfg.root();

        // Find telemetry_config inside falcon_9.
        auto falcon_9 = cfg.section("falcon_9");
        auto telemetry = falcon_9.nested("telemetry_config");

        // Print the title and description if both are present.
        auto title = root.get("name");
        auto description = root.get("description");
        if (title && description) {
            cout << *title << endl << *description << endl;
        }

        // Print the Falcon 9 missions.
        if (auto missions = falcon_9.array("missions")) {
            cout << "Missions:" << endl;
            for (const auto& mission : *missions) {
                cout << "- " << mission << endl;
            }
        }

        // Print telemetry if all three values are present and valid.
        auto format = telemetry.get("format");
        auto rate = telemetry.get<int>("sampling_rate");
        auto enabled = telemetry.get<bool>("enabled");

        if (format && rate && enabled) {
            cout << "Falcon 9" << endl
                 << "Format: " << *format << endl
                 << "Sampling rate: " << *rate << endl
                 << "Enabled: " << std::boolalpha << *enabled << endl;
        }
    } catch (const std::runtime_error& error) {
        std::cerr << error.what() << endl;
        return 1;
    }
}
