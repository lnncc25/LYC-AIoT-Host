#include "csvutils.h"

namespace CsvUtils {

QString escapeCell(QString text)
{
    text.replace("\"", "\"\"");
    const bool needsQuotes = text.contains(',') || text.contains('"')
                             || text.contains('\n') || text.contains('\r');
    return needsQuotes ? "\"" + text + "\"" : text;
}

}
