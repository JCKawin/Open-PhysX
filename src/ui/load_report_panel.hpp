#pragma once

namespace openphysx {

struct FileSession;

// Non-fatal warnings collected while a project loaded. Opens automatically when
// a load reports entries, or from the Window menu. UI only; the data is LoadReport.
void DrawLoadReportPanel(FileSession& session);

} // namespace openphysx
