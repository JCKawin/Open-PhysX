#include "ui/load_report_panel.hpp"

#include "ui/file_menu.hpp"

#include "imgui.h"

namespace openphysx {

void DrawLoadReportPanel(FileSession& session)
{
    if (!session.load_report_open)
        return;
    if (session.load_report.entries.empty())
    {
        session.load_report_open = false;
        return;
    }

    int warnings = 0;
    int errors = 0;
    for (const LoadReport::Entry& entry : session.load_report.entries)
    {
        if (entry.severity == LoadReport::Severity::Error)
            ++errors;
        else
            ++warnings;
    }

    if (ImGui::Begin("Load Report", &session.load_report_open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::Text("The last load finished with %d warning(s) and %d error(s).", warnings, errors);
        ImGui::TextWrapped("Problematic values were replaced with safe defaults. Save to keep the repaired project.");
        ImGui::Separator();
        if (ImGui::BeginChild("##load-report-entries", ImVec2(0.0f, 240.0f), true))
        {
            for (const LoadReport::Entry& entry : session.load_report.entries)
            {
                if (entry.severity == LoadReport::Severity::Error)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.45f, 1.0f));
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::TextWrapped("%s", entry.message.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Clear", ImVec2(-1.0f, 0.0f)))
        {
            session.load_report.entries.clear();
            session.load_report_open = false;
        }
    }
    ImGui::End();
}

} // namespace openphysx
